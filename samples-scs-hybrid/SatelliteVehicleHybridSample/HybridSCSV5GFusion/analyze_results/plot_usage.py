#!/usr/bin/env python3
"""
plot_usage.py - Interface usage analysis script for hybrid TN-NTN vehicular simulation.

Analyzes how much time each vehicle node spent on each interface (satellite vs cellular)
using the cumulative usage time vectors emitted by HybridInterfaceManager:
  - satUsageSignal:  cumulative seconds spent on satellite interface
  - cellUsageSignal: cumulative seconds spent on cellular interface

The last value of each vector represents the total time spent on that interface
over the entire simulation. Two plots are generated:
  1. Stacked bar chart: per-node usage percentage (satellite + cellular = 100%)
  2. Pie chart: global aggregate usage across all nodes

Usage:
    python plot_usage.py <path-to-results.vec>
"""

import os
import re
import sys
from utils import apply_style, FIG_SINGLE, FIG_SQUARE, BLUE, GREEN, SKY_BLUE
import matplotlib.pyplot as plt
from omnetpp.scave import results
 
# ============================================================================
# Utility
# ============================================================================

def extract_node_id(module_str):
    """
    Extracts the integer node index from a module path string.
    E.g. "Network.node[3].interfaceManager" -> 3
    Returns -1 if no match is found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1


def save_plot(fig, filepath, config_name, filename):
    """Saves the figure to the same directory as the input results file."""
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_path = os.path.join(dir_name, filename)
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_path}")


# ============================================================================
# Main analysis
# ============================================================================

def plot(filepath, config_name):
    """
    Loads satUsageTime and cellUsageTime vectors, computes per-node interface
    usage percentages, and generates a stacked bar chart and a pie chart.
    """
    # Load both usage signals in a single query
    COMBINED_FILTER = "*cellUsageTime:vector* OR *satUsageTime:vector*"

    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    print("Extracting usage vectors...")
    df_vectors = results.get_vectors(COMBINED_FILTER, include_attrs=True)

    if df_vectors.empty:
        print("[WARN] No usage vectors found.")
        print("[WARN] Verify that satUsageTime and cellUsageTime are recorded in the .ini file.")
        return

    print(f"Found {len(df_vectors)} vectors.")
    print("Processing data...\n")

    # The last value of each vector is the cumulative time at end of simulation
    df_vectors['final_time'] = df_vectors['vecvalue'].apply(
        lambda x: x[-1] if len(x) > 0 else 0.0
    )
    df_vectors['NodeID'] = df_vectors['module'].apply(extract_node_id)

    # Pivot: rows=NodeID, columns=signal source, values=final cumulative time
    usage_summary = df_vectors.pivot_table(
        index='NodeID',
        columns='source',  # differentiates cellUsageSignal from satUsageSignal
        values='final_time',
        fill_value=0.0     # nodes that never used an interface get 0
    )

    # Rename columns for clarity
    usage_summary = usage_summary.rename(columns={
        'cellUsageSignal': 'Cellular_Time_s',
        'satUsageSignal':  'Satellite_Time_s'
    })

    # Compute total time and usage percentages per node
    usage_summary['Total_Time_s'] = usage_summary['Cellular_Time_s'] + usage_summary['Satellite_Time_s']
    usage_summary['Cell_%'] = (usage_summary['Cellular_Time_s'] / usage_summary['Total_Time_s']) * 100
    usage_summary['Sat_%']  = (usage_summary['Satellite_Time_s'] / usage_summary['Total_Time_s']) * 100

    usage_summary = usage_summary.sort_index()

    print(usage_summary.to_string())

    # Global statistics
    global_avg_cell = usage_summary['Cellular_Time_s'].mean()
    global_avg_sat  = usage_summary['Satellite_Time_s'].mean()
    print(f"\nGlobal Avg Cellular Time:  {global_avg_cell:.1f} s")
    print(f"Global Avg Satellite Time: {global_avg_sat:.1f} s")

    print(f"Global Avg Satellite Usage: {usage_summary['Sat_%'].mean():.1f} %")
    print(f"Global Avg Cellular Usage: {usage_summary['Cell_%'].mean():.1f} %")

    # Interface preference breakdown
    sat_preferred  = (usage_summary['Sat_%']  > usage_summary['Cell_%']).sum()
    cell_preferred = (usage_summary['Sat_%']  < usage_summary['Cell_%']).sum()
    equal_split    = (usage_summary['Sat_%'] == usage_summary['Cell_%']).sum()
    print(f"\nSatellite-preferred nodes: {sat_preferred}")
    print(f"Cellular-preferred nodes:  {cell_preferred}")
    print(f"Equal split nodes:         {equal_split}")
    print(f"Total nodes:               {sat_preferred + cell_preferred + equal_split}\n")

    # Generate plots
    bar_plot_usage(usage_summary, filepath, config_name)
    pie_plot_usage(usage_summary, filepath, config_name)


# ============================================================================
# Plots
# ============================================================================

def bar_plot_usage(usage_summary, filepath, config_name):
    """
    Generates a stacked bar chart showing per-node interface usage percentage.
    Each bar sums to 100%: cellular (green, bottom) + satellite (blue, top).
    A dashed horizontal line marks the mean satellite usage across all nodes.
    """
    print("Generating stacked bar chart...")

    node_ids = usage_summary.index
    cell_pct = usage_summary['Cell_%']
    sat_pct  = usage_summary['Sat_%']

    print(f"Global Avg Satellite Usage: {sat_pct.mean():.1f} %")
    print(f"Global Avg Cellular Usage: {cell_pct.mean():.1f} %")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.bar(node_ids, cell_pct, label='Cellular',  color=GREEN, edgecolor='black', width=0.8)
    ax.bar(node_ids, sat_pct,  label='Satellite', color=SKY_BLUE, edgecolor='black', width=0.8, bottom=cell_pct)

    # Mean satellite usage reference line
    mean_sat = sat_pct.mean()
    ax.axhline(mean_sat, color=BLUE, linestyle='--', linewidth=2, label=f'Mean Satellite: {mean_sat:.1f}%')

    ax.set_xlabel('Node Index')
    ax.set_ylabel('Time Usage (%)')
    ax.set_title(f'Interface Usage Distribution - {config_name}')
    ax.set_ylim(0, 100)
    ax.set_xlim(min(node_ids) - 1, max(node_ids) + 1)
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()
    save_plot(fig, filepath, config_name, f"plot_interface_usage_{config_name}.png")


def pie_plot_usage(usage_summary, filepath, config_name):
    """
    Generates a pie chart showing global aggregate interface usage across all nodes.
    Cellular time and satellite time are summed across all nodes to produce two slices.
    """

    cell_pct = usage_summary['Cell_%']
    sat_pct  = usage_summary['Sat_%']
    print(f"Global Avg Satellite Usage: {sat_pct.mean():.1f} %")
    print(f"Global Avg Cellular Usage: {cell_pct.mean():.1f} %")

    print("Generating pie chart...")

    fig, ax = plt.subplots(figsize=FIG_SQUARE)
    
    sizes   = [cell_pct.mean(), sat_pct.mean()]
    labels  = ['Cellular', 'Satellite']
    colors  = [GREEN, SKY_BLUE]
    explode = (0.1, 0)  # slightly explode the cellular slice for visual emphasis

    pie_result = ax.pie(
        sizes,
        explode=explode,
        labels=labels,
        colors=colors,
        autopct='%1.1f%%',
        shadow=True,
        startangle=90,
    )

    # Style the percentage labels inside the slices
    if len(pie_result) > 2:
        autotexts = pie_result[2]
        plt.setp(autotexts, size=14, weight='bold', color='white')

    ax.set_title(f'Interface Usage Distribution\n{config_name}')

    plt.tight_layout()
    save_plot(fig, filepath, config_name, f"pie_interface_usage_{config_name}.png")


# ============================================================================
# Entry point
# ============================================================================

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_interface_usage.py <path-to-results.vec>")
        sys.exit(1)

    filepath = sys.argv[1]
    apply_style()

    # Derive config name from parent directory name
    config_name = filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"

    print(f"Analyzing file:  {filepath}")
    print(f"Configuration:   {config_name}\n")

    try:
        plot(filepath, config_name)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")