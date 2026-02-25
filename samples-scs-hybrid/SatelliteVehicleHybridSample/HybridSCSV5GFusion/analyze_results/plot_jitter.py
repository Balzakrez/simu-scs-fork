#!/usr/bin/env python3
"""
plot_jitter.py - Jitter analysis script for hybrid TN-NTN vehicular simulation results.

Analyzes the currentJitter signal emitted periodically by QoSBasedStrategy via
HybridInterfaceManager. Jitter is computed as the mean absolute difference between
consecutive RTT samples over the current measurement window (cutOffInterval):

    jitter = sum(|RTT[i] - RTT[i-1]|) / (N - 1)

This is the same jitter value used internally for QoS scoring and switching decisions.
Only available for runs using switchingMode = "qos-based".

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
    output_filename = os.path.join(dir_name, f"plot_jitter_{suffix}_{config_name}.png")
    fig.savefig(output_filename, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_filename}")


# ============================================================================
# Jitter Analysis
# ============================================================================

def analyze_jitter(filepath, config_name):
    """
    Loads currentJitter vectors emitted by QoSBasedStrategy (via HybridInterfaceManager),
    computes per-node mean jitter, and generates a bar chart.

    The signal is emitted every qosCheckInterval and reflects jitter computed
    over RTT_Probe samples within the most recent cutOffInterval window.
    Values are in seconds and are converted to milliseconds for readability.
    """
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # currentJitter is registered and emitted by QoSBasedStrategy
    # via HybridInterfaceManager (the owning cModule).
    # Broad filter used to avoid module name casing issues.
    JITTER_FILTER = "*currentJitter:vector*"

    print("Extracting jitter vectors...")
    df = results.get_vectors(JITTER_FILTER, include_attrs=True) 

    if df.empty:
        print("[WARN] No currentJitter vectors found.")
        print("[WARN] Verify that 'currentJitter' is recorded in the .ini file")
        print("[WARN] and that QoSBasedStrategy is the active switching mode.")
        return

    print(f"Found {len(df)} vectors.")

    # Extract node IDs and drop unmatched rows
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    if df.empty:
        print("[WARN] No nodes matched pattern 'node[N]'. Check module names.")
        return
    df = df.sort_values('NodeID')

    # Convert jitter from seconds to milliseconds
    df['Jitter_ms'] = df['vecvalue'].apply(lambda x: np.array(x) * 1000.0)

    # Compute per-node mean jitter
    df['Mean_Jitter'] = df['Jitter_ms'].apply(np.mean)

    # Global mean across all nodes (mean of per-node means)
    global_mean_jitter = df['Mean_Jitter'].mean()
    print(f"Global Mean Jitter: {global_mean_jitter:.2f} ms")

    # Global mean weighted by count (more accurate overall mean across all samples)
    df['Sample_Count'] = df['Jitter_ms'].apply(len)
    true_global_mean_jitter = (df['Mean_Jitter'] * df['Sample_Count']).sum() / df['Sample_Count'].sum()
    print(f"Global Mean Jitter (weighted): {true_global_mean_jitter:.2f} ms")

    # -------------------------------------------------------------------------
    # Plot: Mean jitter bar chart per node
    # -------------------------------------------------------------------------
    print("Generating mean jitter bar chart...")

    node_ids = df['NodeID'].tolist()
    means    = df['Mean_Jitter'].tolist()

    # Color bars by jitter quality thresholds:
    # < 10ms -> green  (excellent, negligible variation)
    # < 30ms -> orange (acceptable for most applications)
    # >= 30ms -> red   (poor, likely interface switching artifacts or congestion)
    colors = [
        GREEN if m < 10 else ORANGE if m < 30 else VERMILLION
        for m in means
    ]

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Reference threshold lines matching the color thresholds above
    ax.axhline(10, color=GREEN, linestyle='--', alpha=0.5, label='Excellent (< 10 ms)')
    ax.axhline(30, color=VERMILLION,   linestyle='--', alpha=0.5, label='Poor threshold (30 ms)')

    # Global mean line
    ax.axhline(y=global_mean_jitter, color=BLUE, linestyle='-', linewidth=2,
               label=f'Global Mean: {global_mean_jitter:.1f} ms')

    ax.set_xlabel('Node Index')
    ax.set_ylabel('Average Jitter (ms)')
    ax.set_title(
        f'Average Jitter per Node - {config_name}\n'
        f'(Global Mean: {global_mean_jitter:.1f} ms)'
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
        analyze_jitter(filepath, config_name)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")