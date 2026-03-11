#!/usr/bin/env python3
"""
plot_jitter.py - Jitter analysis script for cellular-only vehicular simulation results.

Derives jitter from rcvdPkLifetime vectors measured at vehicle nodes from RTT_Probe
echo exchanges, using the same algorithm as QoSBasedStrategy::calculateAvgJitter():

    jitter(i) = |RTT(i) - RTT(i-1)|
    avg_jitter = sum(jitter) / (N - 1)

This ensures that jitter values are directly comparable to those computed by
QoSBasedStrategy in hybrid scenarios, enabling meaningful cross-scenario analysis.

Signal source:
  - node[*].app[1] (UdpBasicApp) sends RTT_Probe packets to server:7000
  - server.app[1]  (UdpEchoApp)  echoes them back
  - rcvdPkLifetime at node[*].app[1] captures the full RTT

Usage:
    python plot_jitter.py <path-to-results.vec>
"""

import os
import re
import sys
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, BLUE, GREEN, ORANGE, VERMILLION

# ============================================================================
# Utility
# ============================================================================

def extract_node_id(module_str):
    """
    Extracts the integer node index from a module path string.
    E.g. "Network.node[3].app[1]" -> 3
    Returns -1 if no match is found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1


def calculate_jitter(rtt_vector):
    """
    Derives jitter from an RTT sample vector using the same algorithm as
    QoSBasedStrategy::calculateAvgJitter():

        jitter(i) = |RTT(i) - RTT(i-1)|
        avg_jitter = sum(jitter) / (N - 1)

    Input values are expected in seconds; output is in milliseconds.
    Returns 0.0 if fewer than 2 samples are available.
    """
    samples = np.array(rtt_vector) * 1000.0  # Convert s -> ms
    if len(samples) < 2:
        return 0.0
    diffs = np.abs(np.diff(samples))
    return float(np.sum(diffs) / len(diffs))


def save_plot(fig, filepath, config_name, suffix):
    """Saves the figure to the same directory as the input results file."""
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_filename = os.path.join(dir_name, f"plot_simu5g_jitter_{suffix}_{config_name}.png")
    fig.savefig(output_filename, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_filename}")


# ============================================================================
# Jitter Analysis
# ============================================================================

def analyze_simu5g_jitter(filepath, config_name):
    """
    Loads rcvdPkLifetime vectors from node app[1] (RTT_Probe echo receiver),
    derives per-node jitter via consecutive RTT differences, and generates a
    mean jitter bar chart.

    The jitter formula mirrors QoSBasedStrategy::calculateAvgJitter() so that
    results are directly comparable to hybrid scenario outputs from plot_jitter.py.
    """
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    RTT_FILTER = "module=~*node[*].app[1] AND name=~*rcvdPkLifetime:vector*"

    print("Extracting RTT vectors to derive jitter...")
    df = results.get_vectors(RTT_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No rcvdPkLifetime vectors found for node[*].app[1].")
        print("[WARN] Verify that app[1] is configured as the RTT_Probe sender")
        print("[WARN] and that rcvdPkLifetime statistics are enabled in the .ini file.")
        return

    print(f"Found {len(df)} vectors.")

    # Extract node IDs and drop unmatched rows
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    if df.empty:
        print("[WARN] No nodes matched pattern 'node[N]'. Check module names.")
        return
    df = df.sort_values('NodeID')

    # Derive per-node mean jitter from RTT samples
    df['Mean_Jitter'] = df['vecvalue'].apply(calculate_jitter)
    df['Sample_Count'] = df['vecvalue'].apply(len)

    # Drop nodes with insufficient samples
    df = df[df['Sample_Count'] >= 2]
    if df.empty:
        print("[WARN] All nodes have insufficient RTT samples to compute jitter (< 2 samples).")
        return

    # Print debug table
    print(f"\n{'Node':<8} {'Samples':<10} {'Mean Jitter (ms)':<20}")
    print("-" * 38)
    for _, row in df.iterrows():
        print(f"{int(row['NodeID']):<8} {int(row['Sample_Count']):<10} {row['Mean_Jitter']:<20.2f}")

    # Global mean across all nodes (mean of per-node means)
    global_mean_jitter = df['Mean_Jitter'].mean()
    print(f"\nGlobal Mean Jitter: {global_mean_jitter:.2f} ms")

    # Global mean weighted by sample count
    true_global_mean_jitter = (df['Mean_Jitter'] * df['Sample_Count']).sum() / df['Sample_Count'].sum()
    print(f"Global Mean Jitter (weighted): {true_global_mean_jitter:.2f} ms")

    # -------------------------------------------------------------------------
    # Plot: Mean jitter bar chart per node
    # -------------------------------------------------------------------------
    print("Generating mean jitter bar chart...")

    node_ids = df['NodeID'].tolist()
    means    = df['Mean_Jitter'].tolist()

    # Color bars by jitter quality thresholds:
    # < 10ms  -> green  (excellent, negligible variation)
    # < 30ms  -> orange (acceptable for most applications)
    # >= 30ms -> red    (poor, likely congestion or switching artifacts)
    colors = [
        GREEN if m < 10 else ORANGE if m < 30 else VERMILLION
        for m in means
    ]

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Reference threshold lines matching the color thresholds above
    ax.axhline(10, color=GREEN,      linestyle='--', alpha=0.5, label='Excellent (< 10 ms)')
    ax.axhline(30, color=VERMILLION, linestyle='--', alpha=0.5, label='Poor threshold (30 ms)')

    # Global mean line
    ax.axhline(y=global_mean_jitter, color=BLUE, linestyle='-', linewidth=2,
               label=f'Global Mean: {global_mean_jitter:.1f} ms')

    ax.set_xlabel('Node Index')
    ax.set_ylabel('Average Jitter (ms)')
    ax.set_title(
        f'Average Jitter per Node: {global_mean_jitter:.1f} ms - {config_name}'
        # f'(Global Mean: {global_mean_jitter:.1f} ms)'
        # f'Average Jitter per Node - {config_name}\n'
    )
    ax.set_xlim(min(node_ids) - 1, max(node_ids) + 1)
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()

    save_plot(fig, filepath, config_name, "mean")


# ============================================================================
# Entry point
# ============================================================================

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_jitter.py <path-to-results.vec>")
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
        analyze_simu5g_jitter(filepath, config_name)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")