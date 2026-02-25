#!/usr/bin/env python3
"""
plot_throughput.py - Throughput analysis script for hybrid TN-NTN vehicular simulation results.

Measures and plots two complementary metrics:

  1. Cumulative bytes transferred per node — available for ALL strategies:
     Uses three scalars emitted by both QoSBasedStrategy and EnergyAwareStrategy
     in finish():
       - satelliteTotalBytes:   bytes received via satellite interface
       - cellularTotalBytes:    bytes received via cellular interface
       - totalBytesTransferred: sum of both interfaces
     Displayed as a stacked bar chart (satellite + cellular) per node, showing
     both total volume and interface usage breakdown.
     Reads from the .sca file.

  2. QoS window throughput per node — QoS strategy only:
     Uses the 'currentThroughput' vector emitted periodically by QoSBasedStrategy.
     Reflects RTT_Probe echo reply throughput over the most recent cutOffInterval
     sliding window — the same value used internally for QoS scoring and switching.
     Not available when EnergyAwareStrategy is the active switching mode.
     Reads from the .vec file.

Usage:
    python plot_throughput.py <results.vec> [results.sca]
    If .sca is omitted, inferred by replacing .vec extension.
"""

import sys
import os
import re
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, BLUE, GREEN, ORANGE, VERMILLION, SKY_BLUE

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


def save_plot(fig, filepath, config_name, suffix):
    """Saves the figure to the same directory as the input results file."""
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_filename = os.path.join(dir_name, f"plot_throughput_{suffix}_{config_name}.png")
    fig.savefig(output_filename, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_filename}")


def build_bar_chart(ax, node_ids, means, global_mean, low_thresh, high_thresh,
                    ylabel, title, low_label, high_label):
    """
    Renders a colored bar chart with threshold lines and global mean line.
    Bars are colored green/orange/red based on whether they exceed high_thresh,
    low_thresh, or neither.
    """
    colors = [
        GREEN if m > high_thresh else ORANGE if m > low_thresh else VERMILLION
        for m in means
    ]
    ax.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)
    ax.axhline(high_thresh, color=GREEN, linestyle='--', alpha=0.5, label=high_label)
    ax.axhline(low_thresh,  color=VERMILLION,   linestyle='--', alpha=0.5, label=low_label)
    ax.axhline(y=global_mean, color=BLUE, linestyle='-', linewidth=2,
               label=f'Global Mean: {global_mean:.1f}')
    ax.set_xlabel('Node Index', fontsize=12)
    ax.set_ylabel(ylabel, fontsize=12)
    ax.set_title(title, fontsize=14, fontweight='bold')
    ax.set_xlim(min(node_ids) - 1, max(node_ids) + 1)
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)


# ============================================================================
# Metric 1: Cumulative bytes transferred per node (all strategies)
# ============================================================================

def plot_cumulative_bytes(sca_filepath, config_name):
    """
    Plots cumulative bytes transferred per node as a stacked bar chart
    (satellite bytes + cellular bytes), using three scalars emitted in finish()
    by both QoSBasedStrategy and EnergyAwareStrategy:
      - satelliteTotalBytes
      - cellularTotalBytes
      - totalBytesTransferred

    No sim-time is needed: we plot bytes directly, which is more informative
    than throughput alone because it shows the interface usage breakdown.
    """
    print("\n--- Metric 1: Cumulative bytes transferred per node (finish() scalars) ---")
    results.set_inputs(sca_filepath)

    BASE = "module=~*node[*].interfaceManager*"

    df_sat  = results.get_scalars(f"{BASE} AND name=~*satelliteTotalBytes*")
    df_cell = results.get_scalars(f"{BASE} AND name=~*cellularTotalBytes*")
    df_tot  = results.get_scalars(f"{BASE} AND name=~*totalBytesTransferred*")

    if df_tot.empty:
        print("[WARN] No totalBytesTransferred scalars found.")
        print("[WARN] Verify that scalar recording is enabled in the .ini file and")
        print("[WARN] that the interfaceManager module name matches 'interfaceManager'.")
        return

    print(f"Found {len(df_tot)} totalBytesTransferred scalars.")
    print(f"[DEBUG] Modules: {df_tot['module'].unique()[:3]}")

    # Extract NodeID for all three dataframes
    for df in (df_sat, df_cell, df_tot):
        df['NodeID'] = df['module'].apply(extract_node_id)

    df_sat  = df_sat[df_sat['NodeID']   != -1].sort_values('NodeID')
    df_cell = df_cell[df_cell['NodeID'] != -1].sort_values('NodeID')
    df_tot  = df_tot[df_tot['NodeID']   != -1].sort_values('NodeID')

    node_ids   = df_tot['NodeID'].tolist()
    total_vals = df_tot['value'].tolist()

    # Fall back to zeros if per-interface scalars are missing
    sat_vals  = df_sat['value'].tolist()  if not df_sat.empty  else [0.0] * len(node_ids)
    cell_vals = df_cell['value'].tolist() if not df_cell.empty else [0.0] * len(node_ids)

    # Convert bytes to KB for readability
    sat_kb   = [v / 1000.0 for v in sat_vals]
    cell_kb  = [v / 1000.0 for v in cell_vals]
    total_kb = [v / 1000.0 for v in total_vals]

    # Print debug table
    print(f"\n{'Node':<8} {'Satellite(KB)':<16} {'Cellular(KB)':<16} {'Total(KB)':<12}")
    print("-" * 54)
    for i, node_id in enumerate(node_ids):
        print(f"{node_id:<8} {sat_kb[i]:<16.1f} {cell_kb[i]:<16.1f} {total_kb[i]:<12.1f}")

    # Global mean across all nodes (mean of per-node means)
    global_mean_total = np.mean(total_kb)
    print(f"\nGlobal Mean Throughput Total: {global_mean_total:.1f} KB")

    # Stacked bar chart: satellite (blue) + cellular (green)
    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.bar(node_ids, sat_kb,  color=SKY_BLUE, edgecolor='black', linewidth=0.5,
           width=0.8, alpha=0.85, label='Satellite')
    ax.bar(node_ids, cell_kb, color=GREEN, edgecolor='black', linewidth=0.5,
           width=0.8, alpha=0.85, label='Cellular', bottom=sat_kb)

    # Global mean total line
    ax.axhline(y=float(global_mean_total), color=BLUE, linestyle='-', linewidth=2, label=f'Global Mean: {global_mean_total:.1f} KB')

    ax.set_xlabel('Node Index')
    ax.set_ylabel('Bytes Transferred (KB)')
    ax.set_title(
        f'Cumulative Bytes Transferred per Node - {config_name}\n'
        f'(Satellite + Cellular | Global Mean: {global_mean_total:.1f} KB)',
    )
    ax.set_xlim(min(node_ids) - 1, max(node_ids) + 1)
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    save_plot(fig, sca_filepath, config_name, "cumulative_bytes")


# ============================================================================
# Metric 2: QoS window throughput from QoSBasedStrategy (qos-based only)
# ============================================================================

def plot_qos_throughput(vec_filepath, config_name):
    """
    Plots mean QoS window throughput per node using the 'currentThroughput'
    vector emitted periodically by QoSBasedStrategy via HybridInterfaceManager.

    This reflects RTT_Probe echo reply throughput over the most recent
    cutOffInterval sliding window, and is the value used internally for
    QoS scoring and switching decisions.

    Only available for runs using switchingMode = "qos-based".
    Skipped silently for EnergyAwareStrategy runs.
    """
    print("\n--- Metric 2: QoS window throughput (QoSBasedStrategy only) ---")
    results.set_inputs(vec_filepath)

    # Broad filter to avoid module name casing issues
    FILTER = "*currentThroughput:vector*"
    df = results.get_vectors(FILTER, include_attrs=True)

    if df.empty:
        print("[INFO] No currentThroughput vectors found.")
        print("[INFO] This is expected when EnergyAwareStrategy is the active switching mode.")
        print("[INFO] Skipping QoS window throughput plot.")
        return

    print(f"Found {len(df)} vectors.")
    print(f"[DEBUG] Modules: {df['module'].unique()[:3]}")

    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    if df.empty:
        print("[WARN] No nodes matched pattern 'node[N]'. Check module names.")
        return
    df = df.sort_values('NodeID')

    # Convert bps -> kbps
    df['Throughput_kbps'] = df['vecvalue'].apply(lambda x: np.array(x) / 1000.0)
    df['Mean_Tput'] = df['Throughput_kbps'].apply(
        lambda x: np.mean(x) if len(x) > 0 else 0.0
    )

    node_ids    = df['NodeID'].tolist()
    means       = df['Mean_Tput'].tolist()
    
    # Global mean across all nodes (mean of per-node means)
    global_mean = np.mean(means)
    print(f"Global Mean QoS Window Throughput: {global_mean:.2f} kbps")

    # Global mean weighted by count (more accurate overall mean across all samples)
    df['Sample_Count'] = df['Throughput_kbps'].apply(len)
    true_global_mean = (df['Mean_Tput'] * df['Sample_Count']).sum() / df['Sample_Count'].sum()
    print(f"Global Mean QoS Window Throughput (weighted): {true_global_mean:.2f} kbps")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)
    build_bar_chart(
        ax, node_ids, means, global_mean,
        low_thresh=40, high_thresh=75,
        ylabel='Average Throughput (kbps)',
        title=(
            f'Average QoS Window Throughput per Node - {config_name}\n'
            f'(QoSBasedStrategy sliding window | Global Mean: {global_mean:.1f} kbps)'
        ),
        low_label='Poor (< 40 kbps)',
        high_label='Target (75+ kbps)'
    )
    plt.tight_layout()
    save_plot(fig, vec_filepath, config_name, "qos_window")


# ============================================================================
# Entry point
# ============================================================================

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_throughput.py <results.vec> [results.sca]")
        sys.exit(1)

    vec_filepath = sys.argv[1]
    sca_filepath = sys.argv[2] if len(sys.argv) >= 3 else vec_filepath.replace('.vec', '.sca')

    # Derive config name from parent directory of the vec file
    config_name = vec_filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"

    print(f"Analyzing vec:  {vec_filepath}")
    print(f"Analyzing sca:  {sca_filepath}")
    print(f"Configuration:  {config_name}\n")

    apply_style()

    try:
        # Metric 1: cumulative bytes from finish() scalars (all strategies)
        if os.path.exists(sca_filepath):
            plot_cumulative_bytes(sca_filepath, config_name)
        else:
            print(f"[WARN] .sca file not found at {sca_filepath}, skipping Metric 1.")
            print("[WARN] Pass it explicitly: python plot_throughput.py <results.vec> <results.sca>")

        # Metric 2: QoS window throughput from vectors (qos-based only)
        plot_qos_throughput(vec_filepath, config_name)

    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")