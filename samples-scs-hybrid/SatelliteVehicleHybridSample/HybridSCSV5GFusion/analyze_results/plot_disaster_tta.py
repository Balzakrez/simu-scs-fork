#!/usr/bin/env python3
"""
plot_disaster_tta.py - Safety-Critical Alert Scenario Analysis.

Computes and plots four metrics for the V2N2V alert dissemination scenario:

  1. Uplink PDR      : AlertTriggerPacket delivery ratio from node[0] to server.
                       node[0].app[0] packetSent:count vs server.app[0] packetReceived:count.

  2. Fleet PDR       : AlertPacket delivery ratio from server to each listening node.
                       server.app[0] packetSent:count vs node[i].app[0] packetReceived:count.
                       Per-node breakdown because AlertServerApp tracks packetSentSignal.

  3. Time-to-Alert   : End-to-end latency from node[0] trigger creation to node[i] reception.
                       rcvdPkLifetime at node[1..N].app[0]. The AlertServerApp preserves the
                       original CreationTimeTag from the trigger in each forwarded AlertPacket,
                       so rcvdPkLifetime = simTime_received - triggerCreationTime.

  4. Alert Jitter    : Inter-alert delivery variation at each listening node.
                       Derived from consecutive rcvdPkLifetime differences, using the same
                       algorithm as QoSBasedStrategy::calculateAvgJitter():
                           jitter(i) = |TTA(i) - TTA(i-1)|,  avg = sum / (N-1)

Usage:
    python plot_disaster_tta.py <path-to-results.vec> [--listeners N]

    --listeners N  : Total number of listener nodes (node[1..N]). Default: 99.
"""

import sys
import os
import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, BLUE, ORANGE, GREEN, VERMILLION

# ============================================================================
# Configuration
# ============================================================================

# Index of the alert application on each vehicle node.
ALERT_APP_INDEX = 0

# ============================================================================
# Shared utilities
# ============================================================================

def extract_node_id(module_str):
    """
    Extracts the numeric node index from a module path string.
    E.g. "Network.node[3].app[0]" -> 3
    Returns -1 if no match is found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    return int(match.group(1)) if match else -1

def get_total_listener_nodes(scapath):
    """
    Derives the total number of listener nodes (node[1..N]) from the .sca file
    by finding the maximum NodeID across all node app records.
    Returns N such that listeners = node[1..N].
    """
    results.set_inputs(scapath)
    df = results.get_scalars(
        f"module=~*node[*].app[{ALERT_APP_INDEX}] AND name=~*packetSent:count*"
    )
    if df.empty:
        # fallback: try packetReceived
        df = results.get_scalars(
            f"module=~*node[*].app[{ALERT_APP_INDEX}] AND name=~*packetReceived:count*"
        )
    if df.empty:
        print(" [WARN] Cannot determine node count from .sca. Defaulting to 99.")
        return 99

    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    return int(df['NodeID'].max())  # node[0..N] -> N listeners


def save_plot(fig, filepath, config_name, suffix):
    """Saves the figure as PNG in the same directory as the input results file."""
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_filename = os.path.join(dir_name, f"plot_{suffix}_{config_name}.png")
    fig.savefig(output_filename, dpi=150)
    plt.close(fig)
    print(f"[OK] Plot saved: {output_filename}")


def calculate_jitter(rtt_vector_s):
    """
    Derives mean jitter from a TTA sample vector (in seconds).

    Algorithm mirrors QoSBasedStrategy::calculateAvgJitter():
        jitter(i) = |TTA(i) - TTA(i-1)|
        avg_jitter = sum(jitter) / (N - 1)

    Returns jitter in milliseconds. Returns 0.0 if fewer than 2 samples.
    """
    samples = np.array(rtt_vector_s) * 1000.0  # s -> ms
    if len(samples) < 2:
        return 0.0
    diffs = np.abs(np.diff(samples))
    return float(np.sum(diffs) / len(diffs))


# ============================================================================
# Metric 1: Uplink PDR  (node[0] -> server)
# ============================================================================

def plot_uplink_pdr(vecpath, scapath, config_name):
    """
    Computes and plots the uplink delivery ratio of AlertTriggerPackets.

    node[0].app[0] sends triggers to server:5000.
    server.app[0]  (AlertServerApp) receives them.

    Returns triggers_received so it can be reused by plot_fleet_pdr without
    re-reading the .sca file.
    """
    print("\n--- Metric 1: Uplink PDR (AlertTriggerPacket: node[0] -> server) ---")
    results.set_inputs(scapath)

    SENT_FILTER = f"module=~*node[0].app[{ALERT_APP_INDEX}] AND name=~*packetSent:count*"
    RECV_FILTER = f"module=~*server.app[{ALERT_APP_INDEX}] AND name=~*packetReceived:count*"

    df_sent = results.get_scalars(SENT_FILTER)
    df_recv = results.get_scalars(RECV_FILTER)

    if df_sent.empty:
        print(f" [WARN] No packetSent:count found for node[0].app[{ALERT_APP_INDEX}]. Skipping.")
        return 0
    if df_recv.empty:
        print(f" [WARN] No packetReceived:count found for server.app[{ALERT_APP_INDEX}]. Skipping.")
        return 0

    total_sent     = int(df_sent['value'].sum())
    total_received = int(df_recv['value'].sum())
    total_lost     = total_sent - total_received

    if total_sent == 0:
        print(" [WARN] Total sent is 0. Cannot compute uplink PDR.")
        return 0

    pdr = (total_received / total_sent) * 100
    print(f"Triggers sent by node[0]:    {total_sent}")
    print(f"Triggers received at server: {total_received}")
    print(f"Triggers lost:               {total_lost}")
    print(f"Uplink PDR:                  {pdr:.1f}%")

    bar_color = GREEN if pdr >= 95 else ORANGE if pdr >= 80 else VERMILLION

    fig, ax = plt.subplots(figsize=FIG_SINGLE)
    ax.bar(['node[0] → server'], [pdr], color=bar_color, edgecolor='black',
           linewidth=0.8, width=0.4)
    ax.axhline(y=95, color=VERMILLION, linestyle='--', alpha=0.7, label='Quality threshold 95%')
    ax.set_ylabel('Uplink PDR (%)')
    ax.set_title(f'Uplink PDR (AlertTriggerPacket) — {config_name}')
    ax.set_ylim(0, 105)
    ax.legend(loc='lower left')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    ax.text(0, pdr + 1.5, f'{pdr:.1f}%', ha='center', va='bottom',
            fontsize=12, fontweight='bold')
    plt.tight_layout()
    save_plot(fig, scapath, config_name, "uplink_pdr")

    return total_received  # passed to plot_fleet_pdr


# ============================================================================
# Metric 2: Fleet PDR  (server -> node[1..N])
# ============================================================================

def plot_fleet_pdr(scapath, config_name, triggers_received, total_listener_nodes):
    """
    Computes and plots per-node delivery ratio of AlertPackets from server to fleet.

    server.app[0]  (AlertServerApp) emits packetSentSignal for each AlertPacket sent.
    node[i].app[0] (UdpBasicApp)    records packetReceived:count per node.

    expected_per_node = triggers_received
        The server sends exactly one AlertPacket per listener node per trigger received.

    global_fleet_pdr denominator = triggers_received * total_listener_nodes
        Uses the configured total (not len(df_recv)) to correctly include nodes
        that received 0 packets and may have no record in the .sca file.
    """
    print("\n--- Metric 2: Fleet PDR (AlertPacket: server -> node[1..N]) ---")
    results.set_inputs(scapath)

    SENT_FILTER = f"module=~*server.app[{ALERT_APP_INDEX}] AND name=~*packetSent:count*"
    RECV_FILTER = (
        f"module=~*node[*].app[{ALERT_APP_INDEX}] "
        f"AND name=~*packetReceived:count*"
    )

    df_sent = results.get_scalars(SENT_FILTER)
    df_recv = results.get_scalars(RECV_FILTER, include_attrs=True)

    if df_sent.empty:
        print(f" [WARN] No packetSent:count found for server.app[{ALERT_APP_INDEX}]. Skipping.")
        return
    if df_recv.empty:
        print(f" [WARN] No packetReceived:count found for node[*].app[{ALERT_APP_INDEX}]. Skipping.")
        return

    df_recv['NodeID'] = df_recv['module'].apply(extract_node_id)
    df_recv = df_recv[df_recv['NodeID'] != -1]
    df_recv = df_recv[df_recv['NodeID'] != 0]   # exclude node[0] (the sender)
    df_recv = df_recv.sort_values('NodeID')

    if df_recv.empty:
        print(" [WARN] No listening nodes found in results. Skipping.")
        return

    total_sent_server    = int(df_sent['value'].sum())
    expected_per_node    = triggers_received
    node_ids             = df_recv['NodeID'].tolist()
    recv_vals            = df_recv['value'].tolist()
    total_received_fleet = sum(recv_vals)
    nodes_with_data      = len(df_recv)

    # Per-node PDR
    pdr_values = [
        (recv / expected_per_node * 100) if expected_per_node > 0 else 0.0
        for recv in recv_vals
    ]

    # Global PDR: uses total_listener_nodes to account for nodes with 0 receptions
    total_expected   = triggers_received * total_listener_nodes
    global_fleet_pdr = (total_received_fleet / total_expected * 100) if total_expected > 0 else 0.0
    mean_pdr         = np.mean(pdr_values)

    print(f"Total listener nodes (configured):    {total_listener_nodes}")
    print(f"Nodes with reception records in .sca: {nodes_with_data}")
    print(f"AlertPackets sent by server (total):  {total_sent_server}")
    print(f"AlertPackets received (total fleet):  {int(total_received_fleet)}")
    print(f"Expected per listener:                {expected_per_node}")
    print(f"Fleet PDR (global weighted):          {global_fleet_pdr:.1f}%")
    print(f"Fleet PDR (mean per node):            {mean_pdr:.1f}%")

    print(f"\n{'Node':<8} {'Received':<12} {'PDR (%)':<10}")
    print("-" * 30)
    for nid, rv, pdr in zip(node_ids, recv_vals, pdr_values):
        print(f"{nid:<8} {int(rv):<12} {pdr:.1f}%")

    colors = [GREEN if p >= 95 else ORANGE if p >= 80 else VERMILLION for p in pdr_values]

    fig, ax = plt.subplots(figsize=FIG_SINGLE)
    ax.bar(node_ids, pdr_values, color=colors, edgecolor='black', linewidth=0.5, width=0.8)
    ax.axhline(y=95, color=VERMILLION, linestyle='--', alpha=0.7, label='Quality threshold 95%')
    ax.axhline(y=float(mean_pdr), color=BLUE, linestyle='-', linewidth=2,
               label=f'Mean (nodes w/ data): {mean_pdr:.1f}%')
    ax.set_xlabel('Vehicle Node Index')
    ax.set_ylabel('Fleet PDR (%)')
    ax.set_title(
        f'Fleet PDR per node (AlertPacket) — {config_name}\n'
        f'Global weighted PDR: {global_fleet_pdr:.1f}%  '
        f'(incl. {total_listener_nodes - nodes_with_data} nodes at 0%)'
    )
    ax.set_ylim(0, 105)
    ax.set_xlim(0, total_listener_nodes + 1)
    ax.legend(loc='lower left')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    save_plot(fig, scapath, config_name, "fleet_pdr")


# ============================================================================
# Metric 3: Time-to-Alert  (node[0] trigger -> node[i] reception)
# ============================================================================

def plot_time_to_alert(vecpath, config_name, total_listener_nodes):
    """
    Computes and plots the end-to-end Time-to-Alert (TTA) per listening node.

    rcvdPkLifetime at node[i].app[0] = simTime_received - triggerCreationTime.
    The AlertServerApp preserves the original CreationTimeTag from the trigger
    in each forwarded AlertPacket, so this signal correctly measures the full
    V2N2V latency: node[0] creates trigger -> server -> node[i] receives alert.

    vals[0] = first AlertPacket received = moment the vehicle is first alerted.

    delivery_ratio uses total_listener_nodes as denominator to correctly account
    for nodes that never received an alert and have no .vec entry.
    """
    print("\n--- Metric 3: Time-to-Alert (rcvdPkLifetime at node[1..N].app[0]) ---")
    results.set_inputs(vecpath)

    SERVER_FILTER   = f"module=~*server.app[{ALERT_APP_INDEX}] AND name=~*packetReceived:vector*"
    LIFETIME_FILTER = (
        f"module=~*node[*].app[{ALERT_APP_INDEX}] "
        f"AND name=~*rcvdPkLifetime:vector*"
    )

    df_server = results.get_vectors(SERVER_FILTER)
    server_received = (
        not df_server.empty and len(df_server.iloc[0]['vecvalue']) > 0
    )
    print(f"Server received AlertTriggerPacket: {'YES' if server_received else 'NO'}")

    if not server_received:
        print(" [INFO] Server did not receive any trigger. Fleet cannot be notified.")
        return
    

    df = results.get_vectors(LIFETIME_FILTER, include_attrs=True)
    if df.empty:
        print(f" [WARN] No rcvdPkLifetime vectors found for node[*].app[{ALERT_APP_INDEX}]. Skipping.")
        return

    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    df = df[df['NodeID'] != 0]   # exclude node[0] (the sender)

    tta_data = {}
    for _, row in df.iterrows():
        vals = row['vecvalue']
        if len(vals) > 0:
            tta_data[row['NodeID']] = vals[0] * 1000.0  # first reception, s -> ms

    nodes_received = len(tta_data)
    # Correct denominator: total configured listeners, not only those with .vec entries
    delivery_ratio = (nodes_received / total_listener_nodes) * 100

    print(f"Fleet delivery ratio: {nodes_received}/{total_listener_nodes} ({delivery_ratio:.1f}%)")

    if nodes_received == 0:
        print(" [WARN] No listening node received an alert. Skipping TTA plot.")
        return

    tta_values   = list(tta_data.values())
    node_indices = list(tta_data.keys())
    mean_tta     = np.mean(tta_values)
    min_tta      = np.min(tta_values)
    max_tta      = np.max(tta_values)
    print(f"Mean TTA: {mean_tta:.2f} ms  |  Min: {min_tta:.2f} ms  |  Max: {max_tta:.2f} ms")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)
    ax.bar(node_indices, tta_values, color=BLUE, edgecolor='black', linewidth=0.5, width=0.8)
    ax.axhline(y=float(mean_tta), color=ORANGE, linestyle='-', linewidth=2,
               label=f'Mean TTA: {mean_tta:.1f} ms')
    ax.axhline(y=500, color=VERMILLION, linestyle='--', alpha=0.7,
               label='Safety limit (500 ms)')
    ax.set_xlabel('Vehicle Node Index')
    ax.set_ylabel('Time-to-Alert (ms)')
    ax.set_title(
        f'End-to-End Time-to-Alert (V2N2V) — {config_name}\n'
        f'Delivery ratio: {nodes_received}/{total_listener_nodes} ({delivery_ratio:.1f}%)'
    )
    ax.set_xlim(0, total_listener_nodes + 1)
    ax.set_ylim(0, max(max_tta, 500) * 1.2)
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    save_plot(fig, vecpath, config_name, "time_to_alert")


# ============================================================================
# Metric 4: Alert Jitter  (variation in TTA across consecutive alerts)
# ============================================================================

def plot_alert_jitter(vecpath, config_name, total_listener_nodes):
    """
    Derives and plots per-node alert delivery jitter from rcvdPkLifetime vectors.

    Uses the same algorithm as QoSBasedStrategy::calculateAvgJitter():
        jitter(i) = |TTA(i) - TTA(i-1)|
        avg_jitter = sum(jitter) / (N - 1)

    Nodes with fewer than 2 received alerts are excluded (jitter undefined).
    """
    print("\n--- Metric 4: Alert Jitter (rcvdPkLifetime at node[1..N].app[0]) ---")
    results.set_inputs(vecpath)

    LIFETIME_FILTER = (
        f"module=~*node[*].app[{ALERT_APP_INDEX}] "
        f"AND name=~*rcvdPkLifetime:vector*"
    )
    df = results.get_vectors(LIFETIME_FILTER, include_attrs=True)

    if df.empty:
        print(" [WARN] No rcvdPkLifetime vectors found. Skipping jitter plot.")
        return

    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    df = df[df['NodeID'] != 0]
    df = df.sort_values('NodeID')

    df['Mean_Jitter']  = df['vecvalue'].apply(calculate_jitter)
    df['Sample_Count'] = df['vecvalue'].apply(len)

    df = df[df['Sample_Count'] >= 2]
    if df.empty:
        print(" [WARN] No nodes have >= 2 alert samples. Cannot compute jitter.")
        return

    print(f"\n{'Node':<8} {'Samples':<10} {'Mean Jitter (ms)':<20}")
    print("-" * 38)
    for _, row in df.iterrows():
        print(f"{int(row['NodeID']):<8} {int(row['Sample_Count']):<10} {row['Mean_Jitter']:<20.2f}")

    global_mean_jitter = df['Mean_Jitter'].mean()
    weighted_jitter    = (
        (df['Mean_Jitter'] * df['Sample_Count']).sum() / df['Sample_Count'].sum()
    )
    print(f"\nGlobal Mean Jitter:            {global_mean_jitter:.2f} ms")
    print(f"Global Mean Jitter (weighted): {weighted_jitter:.2f} ms")

    node_ids = df['NodeID'].tolist()
    means    = df['Mean_Jitter'].tolist()
    colors   = [GREEN if m < 10 else ORANGE if m < 30 else VERMILLION for m in means]

    fig, ax = plt.subplots(figsize=FIG_SINGLE)
    ax.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)
    ax.axhline(10, color=GREEN,      linestyle='--', alpha=0.5, label='Excellent (< 10 ms)')
    ax.axhline(30, color=VERMILLION, linestyle='--', alpha=0.5, label='Poor threshold (30 ms)')
    ax.axhline(y=global_mean_jitter, color=BLUE, linestyle='-', linewidth=2,
               label=f'Global mean: {global_mean_jitter:.1f} ms')
    ax.set_xlabel('Vehicle Node Index')
    ax.set_ylabel('Average Alert Jitter (ms)')
    ax.set_title(f'Alert Delivery Jitter per node — {config_name}')
    ax.set_xlim(0, total_listener_nodes + 1)
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    save_plot(fig, vecpath, config_name, "alert_jitter")


# ============================================================================
# Entry point
# ============================================================================

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Safety-Critical Alert Scenario Analysis (V2N2V).'
    )
    parser.add_argument('vecpath', help='Path to the OMNeT++ .vec results file.')
    args = parser.parse_args()

    apply_style()

    vecpath              = args.vecpath
    scapath              = vecpath.rsplit('.', 1)[0] + ".sca"
    total_listener_nodes = get_total_listener_nodes(scapath)
    print(f"Listener nodes (auto-detected): {total_listener_nodes} (node[1..{total_listener_nodes}])")

    config_name = vecpath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "SafetyCritical"

    print(f"Analyzing file:   {vecpath}")
    print(f"Configuration:    {config_name}")
    print(f"Listener nodes:   {total_listener_nodes} (node[1..{total_listener_nodes}])")

    try:
        triggers_received = plot_uplink_pdr(vecpath, scapath, config_name)
        plot_fleet_pdr(scapath, config_name, triggers_received, total_listener_nodes)
        plot_time_to_alert(vecpath, config_name, total_listener_nodes)
        plot_alert_jitter(vecpath, config_name, total_listener_nodes)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nDone!")