#!/usr/bin/env python3
"""
plot_rtt.py - RTT analysis script for hybrid TN-NTN vehicular simulation results.

Analyzes round-trip time measured at vehicle nodes from RTT_Probe echo exchanges:
  - node[*].app[1] (UdpBasicApp) sends RTT_Probe packets to server:7000
  - server.app[1]  (UdpEchoApp)  echoes them back
  - rcvdPkLifetime at node[*].app[1] measures the full RTT

rcvdPkLifetime is computed by INET as dataAge(packetReceived), which measures
the age of the received packet from its original creation time. Since UdpEchoApp
preserves the creationTime of the original probe in the echo reply, this signal
correctly captures the full round-trip time: probe creation -> server -> node.
This is consistent with the RTT measurement used internally by QoSBasedStrategy.

Usage:
    python plot_rtt.py <path-to-results.vec>
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


def save_plot(fig, filepath, config_name, suffix):
    """Saves the figure to the same directory as the input results file."""
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_filename = os.path.join(dir_name, f"plot_rtt_{suffix}_{config_name}.png")
    fig.savefig(output_filename, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_filename}")


# ============================================================================
# RTT Analysis
# ============================================================================

def analyze_rtt(filepath, config_name):
    """
    Loads rcvdPkLifetime vectors from node app[1] (RTT_Probe echo receiver),
    computes per-node RTT statistics, and generates a mean RTT bar chart.

    rcvdPkLifetime = dataAge(packetReceived) measures the full RTT because
    UdpEchoApp preserves the original probe's creationTime in the echo reply.
    """
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # rcvdPkLifetime emitted by UdpBasicApp when a reply packet is received
    RTT_FILTER = "module=~*node[*].app[1] AND name=~*rcvdPkLifetime:vector*"

    print("Extracting RTT vectors...")
    df = results.get_vectors(RTT_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No rcvdPkLifetime vectors found for node[*].app[1].")
        print("[WARN] Verify that app[1] is correctly configured as the RTT_Probe sender")
        print("[WARN] and that rcvdPkLifetime statistics are enabled in the .ini file.")
        return

    print(f"Found {len(df)} vectors.")
    print(f"Signal names: {df['name'].unique()}")

    # Extract node IDs and drop unmatched rows
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    if df.empty:
        print("[WARN] No nodes matched pattern 'node[N]'. Check module names.")
        return
    df = df.sort_values('NodeID')

    # Convert RTT values from seconds to milliseconds
    df['RTT_ms'] = df['vecvalue'].apply(lambda x: np.array(x) * 1000.0)

    # Compute per-node statistics
    df['Mean_RTT'] = df['RTT_ms'].apply(np.mean)
    df['Min_RTT']  = df['RTT_ms'].apply(np.min)
    df['Max_RTT']  = df['RTT_ms'].apply(np.max)
    df['Std_RTT']  = df['RTT_ms'].apply(np.std)
    df['Count']    = df['RTT_ms'].apply(len)

    # Print debug table
    print(f"\n{'Node':<8} {'Count':<8} {'Mean(ms)':<12} {'Min(ms)':<12} {'Max(ms)':<12} {'Std(ms)':<12}")
    print("-" * 64)
    for _, row in df.iterrows():
        print(
            f"{int(row['NodeID']):<8} {int(row['Count']):<8} "
            f"{row['Mean_RTT']:<12.2f} {row['Min_RTT']:<12.2f} "
            f"{row['Max_RTT']:<12.2f} {row['Std_RTT']:<12.2f}"
        )

    # Global mean across all nodes (mean of per-node means)
    global_mean_rtt = df['Mean_RTT'].mean()
    print(f"\nGlobal Mean RTT: {global_mean_rtt:.2f} ms")

    # Global mean weighted by count (more accurate overall mean across all samples)
    true_global_mean_rtt = (df['Mean_RTT'] * df['Count']).sum() / df['Count'].sum()
    print(f"Global Mean RTT (Weighted): {true_global_mean_rtt:.2f} ms")

    # -------------------------------------------------------------------------
    # Plot: Mean RTT bar chart per node
    # -------------------------------------------------------------------------
    print("Generating Mean RTT bar chart...")

    node_ids = df['NodeID'].tolist()
    means    = df['Mean_RTT'].tolist()

    # Color bars by latency thresholds:
    # < 50ms  -> green  (low latency, suitable for both cellular and satellite)
    # < 200ms -> orange (acceptable for most applications)
    # >= 200ms -> red   (high latency, likely satellite-dominated periods)
    colors = [
        GREEN if m < 50 else ORANGE if m < 200 else VERMILLION
        for m in means
    ]

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Reference threshold lines
    ax.axhline(50,  color=GREEN, linestyle='--', alpha=0.5, label='Low latency threshold (50 ms)')
    ax.axhline(200, color=VERMILLION,   linestyle='--', alpha=0.5, label='High latency threshold (200 ms)')

    # Global mean line
    ax.axhline(y=global_mean_rtt, color=BLUE, linestyle='-', linewidth=2,
               label=f'Global mean: {global_mean_rtt:.1f} ms')

    ax.set_xlabel('Node Index')
    ax.set_ylabel('Average RTT (ms)')
    ax.set_title(
        f'Average Round Trip Time per Node - {config_name}\n'
        f'(Global Mean: {global_mean_rtt:.1f} ms)'
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
        print("Usage: python plot_rtt.py <path-to-results.vec>")
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
        analyze_rtt(filepath, config_name)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")