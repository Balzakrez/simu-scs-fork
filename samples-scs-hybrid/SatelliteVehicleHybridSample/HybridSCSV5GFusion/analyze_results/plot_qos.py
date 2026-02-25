#!/usr/bin/env python3
"""
Plot QoS score over time with switch events marked.
Usage: python plot_qos.py <results.vec> [--threshold 0.70]
"""

import sys
import os
import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, BLUE, VERMILLION, SKY_BLUE

def extract_node_id(module_str):
    match = re.search(r"node\[(\d+)\]", module_str)
    return int(match.group(1)) if match else -1


def aggregate_vectors(df):
    """Interpolate node vectors onto common time grid."""
    all_times = np.sort(np.unique(np.concatenate(df['vectime'].values)))
    matrix = []
    for _, row in df.iterrows():
        t, v = np.array(row['vectime']), np.array(row['vecvalue'])
        if len(t) > 0:
            matrix.append(np.interp(all_times, t, v, left=np.nan, right=np.nan))
    
    if not matrix:
        return None, None, None, None
    
    matrix = np.array(matrix)
    with np.errstate(invalid='ignore'):
        return (all_times,
                np.nanmean(matrix, axis=0),
                np.nanpercentile(matrix, 25, axis=0),
                np.nanpercentile(matrix, 75, axis=0))


def count_switch_events(df_iface):
    """Count total switches to satellite and cellular."""
    to_sat, to_cell = 0, 0
    for _, row in df_iface.iterrows():
        vals = np.array(row['vecvalue'])
        for i in range(1, len(vals)):
            if vals[i] != vals[i-1]:
                if vals[i] == 1:
                    to_sat += 1
                else:
                    to_cell += 1
    return to_sat, to_cell


def main(filepath, config_name, threshold):
    results.set_inputs(filepath)

    # Load QoS vectors
    df_qos = results.get_vectors("*qosScore*")
    if df_qos.empty:
        print("[ERR] No qosScore data found.")
        return

    df_qos['NodeID'] = df_qos['module'].apply(extract_node_id)
    df_qos = df_qos[df_qos['NodeID'] != -1]
    print(f"Found {len(df_qos)} nodes.")

    times, mean, p25, p75 = aggregate_vectors(df_qos)
    if times is None or mean is None or p25 is None or p75 is None:
        print("[ERR] No data to plot.")
        return

    # Count switch events
    df_iface = results.get_vectors("*lastActiveInterface*")
    to_sat, to_cell = 0, 0
    if not df_iface.empty:
        df_iface['NodeID'] = df_iface['module'].apply(extract_node_id)
        df_iface = df_iface[df_iface['NodeID'] != -1]
        to_sat, to_cell = count_switch_events(df_iface)
    print(f"Total switches: {to_sat} → Satellite, {to_cell} → Cellular")

    # Plot
    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.fill_between(times, p25, p75, alpha=0.25, color=SKY_BLUE, label='IQR (25–75th)')
    ax.plot(times, mean, color=BLUE, linewidth=2, label='Mean QoS Score')
    ax.axhline(y=threshold, color=VERMILLION, linestyle='--', linewidth=1.5,label=f'Threshold = {threshold}')

    ax.set_xlabel('Time (s)')
    ax.set_ylabel('QoS Score')
    ax.set_ylim(0, 1.05)
    ax.set_title(f'QoS Score — {config_name}\n'
                 f'Switches: {to_sat} → Sat, {to_cell} → Cell',)
    ax.legend(loc='lower right')
    ax.grid(True, alpha=0.3, linestyle='--')

    plt.tight_layout()
    out = os.path.join(os.path.dirname(filepath), f"plot_qos_{config_name}.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"[OK] Saved: {out}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('filepath')
    parser.add_argument('--threshold', type=float, default=0.70)
    args = parser.parse_args()

    apply_style()
    
    config_name = args.filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"

    try:
        main(args.filepath, config_name, args.threshold)
    except Exception as e:
        import traceback
        print(f"[ERR] {e}")
        traceback.print_exc()
        sys.exit(1)