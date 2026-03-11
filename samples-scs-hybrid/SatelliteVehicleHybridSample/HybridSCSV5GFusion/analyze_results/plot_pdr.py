#!/usr/bin/env python3
"""
plot_pdr.py - PDR analysis script for hybrid TN-NTN vehicular simulation results.

Computes and plots two complementary PDR metrics:
  1. Round-trip PDR per node (app[1], RTT_Probe): measures whether the probe
     completed the full vehicle->server->vehicle path. This is the same metric
     used internally by QoSBasedStrategy for switching decisions.
  2. Classic uplink PDR (app[0], BackgroundLoad): measures whether background
     traffic packets were delivered to the server. Computed globally because
     the server aggregates receptions from all nodes without per-source tracking.

Usage:
    python plot_pdr.py <path-to-results.vec>
"""

import sys
import os
import re
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, FIG_DOUBLE, BLUE, ORANGE, GREEN, VERMILLION

# ============================================================================
# Utility
# ============================================================================

def extract_node_id(module_str):
    """
    Extracts the numeric node index from a module path string.
    E.g. "Network.node[3].app[1]" -> 3
    Returns -1 if no match is found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1


def save_plot(fig, filepath, config_name, suffix):
    """Saves the figure to the same directory as the input file."""
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_filename = os.path.join(dir_name, f"plot_pdr_{suffix}_{config_name}.png")
    fig.savefig(output_filename, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_filename}")


# ============================================================================
# Metric 1: Round-trip PDR per node (RTT_Probe, app[1])
# ============================================================================

def plot_roundtrip_pdr_per_node(filepath, config_name):
    """
    Computes and plots the round-trip PDR for each vehicle node.

    Uses app[1] (RTT_Probe / UdpBasicApp -> UdpEchoApp):
      - packetSent:   probes sent by the node to the echo server
      - packetReceived: echo replies received back at the node

    PDR = received / sent * 100 (per node)

    This metric directly reflects the quality perceived by QoSBasedStrategy,
    since the strategy uses the same probe exchange to compute RTT and PDR
    for switching decisions. Note that this is a bidirectional metric: a probe
    may reach the server but its reply may be lost, which would count as a loss.
    """
    print("\n--- Metric 1: Round-trip PDR per node (RTT_Probe, app[1]) ---")
    results.set_inputs(filepath)

    PROBE_APP_INDEX = 1
    FILTER = (
        f"module=~*node[*].app[{PROBE_APP_INDEX}] "
        f"AND (name=~*packetReceived:vector* OR name=~*packetSent:vector*)"
    )
    df = results.get_vectors(FILTER, include_attrs=True)

    if df.empty:
        print(" [WARN] No RTT_Probe vectors found. Skipping round-trip PDR plot.")
        return

    print("Signal names found:", df['name'].unique())

    # Extract NodeID from module path
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    if df.empty:
        print(" [WARN] No nodes matched pattern 'node[N]'. Check module names.")
        return

    # Count packets per row (each row is one signal for one node)
    df['PacketCount'] = df['vecvalue'].apply(len)

    # Identify signal column names dynamically (OMNeT++ may add suffixes)
    sent_col_list = [c for c in df['name'].unique() if 'packetSent' in c]
    recv_col_list = [c for c in df['name'].unique() if 'packetReceived' in c]

    if not sent_col_list:
        print(" [WARN] No packetSent signal found. Skipping.")
        return
    if not recv_col_list:
        print(" [WARN] No packetReceived signal found. Skipping.")
        return

    sent_col = sent_col_list[0]
    recv_col = recv_col_list[0]
    print(f"Using signals: sent='{sent_col}', received='{recv_col}'")

    # Build pivot table: rows=NodeID, columns=signal name, values=packet count
    pdr_table = df.pivot_table(
        index='NodeID',
        columns='name',
        values='PacketCount',
        fill_value=0
    )

    # Warn about anomalous nodes that received replies without sending probes
    anomalous = pdr_table[pdr_table[sent_col] == 0]
    if not anomalous.empty:
        print(f" [WARN] Nodes with received>0 but sent=0: {anomalous.index.tolist()}")

    # Compute per-node PDR
    pdr_table['PDR'] = pdr_table.apply(
        lambda row: (row[recv_col] / row[sent_col] * 100) if row[sent_col] > 0 else 0.0,
        axis=1
    )
    pdr_table = pdr_table.sort_index()

    node_indices = pdr_table.index.tolist()
    pdr_values = pdr_table['PDR'].tolist()

    if not node_indices:
        print(" [WARN] No valid nodes found after processing.")
        return

    # Print debug table
    print(f"\n{'Node':<8} {'Sent':<10} {'Received':<10} {'PDR':<10}")
    print("-" * 40)
    for node_id in node_indices:
        row = pdr_table.loc[node_id]
        print(f"{node_id:<8} {int(row[sent_col]):<10} {int(row[recv_col]):<10} {row['PDR']:.1f}%")

    mean_pdr = np.mean(pdr_values)
    print(f"\nGlobal Mean Round-trip PDR: {mean_pdr:.1f}%")

    # Weighted PDR: sum(Rx_i) / sum(Tx_i)
    total_sent_rt     = pdr_table[sent_col].sum()
    total_received_rt = pdr_table[recv_col].sum()
    weighted_pdr      = (total_received_rt / total_sent_rt * 100) if total_sent_rt > 0 else 0.0
    print(f"Global Mean Round-trip PDR (weighted):   {weighted_pdr:.1f}%")

    # --- Plot ---
    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    # Color bars by PDR quality thresholds
    colors = [GREEN if p >= 95 else ORANGE if p >= 80 else VERMILLION
              for p in pdr_values]

    ax.bar(node_indices, pdr_values, color=colors, edgecolor='black',
           linewidth=0.5, width=0.8)
    ax.axhline(y=95, color=VERMILLION, linestyle='--', alpha=0.7, label='Quality threshold 95%')
    ax.axhline(y=mean_pdr, color=BLUE, linestyle='-', alpha=0.7, linewidth=2, label=f'Mean: {mean_pdr:.1f}%')

    ax.set_xlabel('Node Index')
    ax.set_ylabel('Round-trip PDR (%)')
    ax.set_title(
        f'Round-trip PDR per node: {mean_pdr:.1f}% - {config_name}\n'
        # f'Round-trip PDR per node (Global Mean={mean_pdr:.1f}%)\n'
        # f'Round-trip PDR per node (RTT_Probe) — {config_name} (mean={mean_pdr:.1f}%)\n'
        # f'Bidirectional metric: probe sent by node, echo reply received back'
    )
    ax.set_ylim(0, 105)
    ax.set_xlim(min(node_indices) - 1, max(node_indices) + 1)
    ax.legend(loc='lower left')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()

    save_plot(fig, filepath, config_name, "roundtrip")


def plot_classic_uplink_pdr(filepath, config_name):
    """
    Computes and plots the classic uplink PDR for background traffic.

    Uses scalars:
      - node[*].app[0] packetSent:count    (BackgroundLoad sent by each node)
      - server.app[0]  packetReceived:count (BackgroundLoad received at server)

    Produces 3 separate figures:
      1. Global PDR as a single bar
      2. Pie chart of delivered vs lost packets
      3. Packets sent per node (load distribution)

    LIMITATION: This metric is only available globally, not per node, because
    the server (UdpSink) aggregates all received packets without tracking the
    source node. Per-node classic PDR would require custom instrumentation
    on the server side.
    """
    print("\n--- Metric 2: Classic uplink PDR — global (BackgroundLoad, app[0]) ---")
    results.set_inputs(filepath)

    SENT_FILTER = "module=~*node[*].app[0] AND name=~*packetSent:count*"
    RECV_FILTER = "module=~*server.app[0] AND name=~*packetReceived:count*"

    df_sent = results.get_scalars(SENT_FILTER)
    df_recv = results.get_scalars(RECV_FILTER)

    if df_sent.empty:
        print(" [WARN] No packetSent:count scalars found for node app[0]. Skipping.")
        return
    if df_recv.empty:
        print(" [WARN] No packetReceived:count scalars found for server app[0]. Skipping.")
        return

    df_sent['NodeID'] = df_sent['module'].apply(extract_node_id)
    df_sent = df_sent[df_sent['NodeID'] != -1]
    if df_sent.empty:
        print(" [WARN] No nodes matched pattern 'node[N]'. Check module names.")
        return

    total_sent = df_sent['value'].sum()
    total_received = df_recv['value'].sum()
    total_lost = total_sent - total_received

    if total_sent == 0:
        print(" [WARN] Total sent is 0. Cannot compute PDR.")
        return

    global_pdr = (total_received / total_sent) * 100
    print(f"Total sent by all nodes:      {int(total_sent)}")
    print(f"Total received at server:     {int(total_received)}")
    print(f"Total lost:                   {int(total_lost)}")
    print(f"Classic uplink PDR (global):  {global_pdr:.1f}%")

    df_sent_sorted = df_sent.sort_values('NodeID')
    node_indices = df_sent_sorted['NodeID'].tolist()
    sent_values = df_sent_sorted['value'].tolist()

    # -------------------------------------------------------------------------
    # Figure 1: Global PDR — single bar
    # -------------------------------------------------------------------------
    bar_color = GREEN if global_pdr >= 95 else ORANGE if global_pdr >= 80 else VERMILLION

    fig1, ax1 = plt.subplots(figsize=FIG_SINGLE)
    ax1.bar(['All nodes'], [global_pdr], color=bar_color, edgecolor='black',
            linewidth=0.8, width=0.4)
    ax1.axhline(y=95, color=VERMILLION, linestyle='--', alpha=0.7,
                label='Quality threshold 95%')
    ax1.set_ylabel('Classic Uplink PDR (%)')
    ax1.set_title(
        # f'Classic Uplink PDR: {global_pdr:.1f}%\n'
        # f'Classic Uplink PDR (Global Mean: {global_pdr:.1f}%)\n'
        f'Classic Uplink PDR — {config_name}\n'
        # f'Unidirectional metric: node -> server'
    )
    ax1.set_ylim(0, 105)
    ax1.legend(loc='lower left')
    ax1.grid(axis='y', linestyle='--', alpha=0.5)
    ax1.text(0, global_pdr + 1.5, f'{global_pdr:.1f}%', ha='center', va='bottom', fontsize=12, fontweight='bold')
    plt.tight_layout()
    save_plot(fig1, filepath, config_name, "classic_uplink_global")

    # -------------------------------------------------------------------------
    # Figure 2: Pie chart — delivered vs lost
    # -------------------------------------------------------------------------
    fig2, ax2 = plt.subplots(figsize=FIG_SINGLE)
    pie_values = [total_received, total_lost]
    pie_labels = [
        f'Delivered\n{int(total_received)} ({global_pdr:.1f}%)',
        f'Lost\n{int(total_lost)} ({100 - global_pdr:.1f}%)'
    ]
    pie_colors = [GREEN, VERMILLION]
    explode = (0.1, 0)  # slightly explode the cellular slice for visual emphasis

    pie_result = ax2.pie(
        pie_values,
        explode=explode,
        labels=pie_labels,
        colors=pie_colors,
        autopct='%1.1f%%',
        shadow=True,
        startangle=90,
    )

    # Style the percentage labels inside the slices
    if len(pie_result) > 2:
        autotexts = pie_result[2]
        plt.setp(autotexts, size=14, weight='bold', color='white')

    ax2.set_title(
        f'BackgroundLoad delivery breakdown\n'
        # f'BackgroundLoad delivery breakdown — {config_name}\n'
        f'Total packets sent: {int(total_sent)}'
    )
    plt.tight_layout()
    save_plot(fig2, filepath, config_name, "classic_uplink_pie")

    # -------------------------------------------------------------------------
    # Figure 3: Packets sent per node — load distribution
    # -------------------------------------------------------------------------
    mean_sent = np.mean(sent_values)

    fig3, ax3 = plt.subplots(figsize=FIG_SINGLE)
    ax3.bar(node_indices, sent_values, color=BLUE, edgecolor='black',
            linewidth=0.5, width=0.8)
    ax3.axhline(y=mean_sent, color=ORANGE, linestyle='-', alpha=0.8,
                linewidth=2, label=f'Mean: {mean_sent:.0f} pkts')
    ax3.set_xlabel('Node Index')
    ax3.set_ylabel('Packets Sent')
    ax3.set_title(
        f'Packets sent per node\n'
        # f'Packets sent per node — {config_name}\n'
        # f'Load distribution across vehicles'
    )
    ax3.set_xlim(min(node_indices) - 1, max(node_indices) + 1)
    ax3.legend(loc='upper right')
    ax3.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    save_plot(fig3, filepath, config_name, "classic_uplink_sent")



# ============================================================================
# Entry point
# ============================================================================

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_pdr_scave.py <path-to-results.vec>")
        sys.exit(1)

    apply_style()

    filepath = sys.argv[1]

    # Derive config name from parent directory name
    config_name = filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"

    print(f"Analyzing file:  {filepath}")
    print(f"Configuration:   {config_name}")

    try:
        # Metric 1: Round-trip PDR per node (same metric as QoSBasedStrategy)
        vecfile = filepath.rsplit('.', 1)[0] + ".vec"
        plot_roundtrip_pdr_per_node(vecfile, config_name)
        # Metric 2: Classic uplink PDR global (BackgroundLoad -> UdpSink)
        scalarfile = filepath.rsplit('.', 1)[0] + ".sca"
        plot_classic_uplink_pdr(scalarfile, config_name)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")