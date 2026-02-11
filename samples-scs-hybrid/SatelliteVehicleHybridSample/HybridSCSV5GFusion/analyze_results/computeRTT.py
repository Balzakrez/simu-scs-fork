#!/usr/bin/env python3
import os, re, sys
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results

# *********************************************************************************** #

def extract_node_id(module_str):
    """
    Extracts the integer Node ID from the module string.
    Returns -1 if not found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1)) 
    return -1

# *********************************************************************************** #

def analyze_rtt(filepath, config_name):
    """
    Analyzes RTT vectors from the given .vec file and generates plots.
    """

    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # Filter for the RTT vector (check your .vec if name differs)
    RTT_FILTER = "*currentRTT:vector* OR *rtt:vector*"

    print("Extracting RTT vectors...")
    df = results.get_vectors(RTT_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No RTT data found. Check if 'currentRTT' is recorded in .vec.")
        return

    print(f"Found {len(df)} vectors. Processing...")

    # Extract Node ID
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1] # Remove invalid nodes
    df = df.sort_values('NodeID')

    # Convert RTT from Seconds to Milliseconds
    df['RTT_ms'] = df['vecvalue'].apply(lambda x: np.array(x) * 1000.0)

    # Calculate Aggregate Stats per Node
    df['Mean_RTT'] = df['RTT_ms'].apply(np.mean)
    
    # Calculate Global Mean across all nodes 
    global_mean_rtt = df['Mean_RTT'].mean()
    print(f"    Global Mean RTT: {global_mean_rtt:.2f} ms")

    print("Generating Mean RTT Bar Chart...")
    fig1, ax1 = plt.subplots(figsize=(14, 6))
    
    node_ids = df['NodeID']
    means = df['Mean_RTT']
    
    # If mean RTT < 50ms: Green, <200ms: Orange, else Red
    colors = []
    for m in means:
        if m < 50: colors.append('#4CAF50')      # Green
        elif m < 200: colors.append('#FF9800')   # Orange
        else: colors.append('#F44336')           # Red

    # Plot Bars
    ax1.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Threshold Lines
    ax1.axhline(50, color='green', linestyle='--', alpha=0.5, label='Low Latency (50ms)')
    ax1.axhline(200, color='red', linestyle='--', alpha=0.5, label='High Latency (200ms)')

    # Plot Global Mean Line
    ax1.axhline(y=global_mean_rtt, color='blue', linestyle='-', linewidth=2, 
                label=f'Global Mean: {global_mean_rtt:.1f} ms')

    ax1.set_xlabel('Node Index', fontsize=12)
    ax1.set_ylabel('Average RTT (ms)', fontsize=12)
    
    # Title with Global Mean 
    ax1.set_title(f'Average Round Trip Time per Node - {config_name}\n(Global Mean: {global_mean_rtt:.1f} ms)', 
                  fontsize=14, fontweight='bold')
    
    ax1.set_xlim(min(node_ids)-1, max(node_ids)+1)
    ax1.legend(loc='upper right')
    ax1.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()

    # Save plot
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_rtt_mean_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    print(f"[OK] Saved {output_filename}")

# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_rtt.py <path-file.vec>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    config_name = filepath.split(os.sep)[-2] # Assuming config name is the parent directory name
    if config_name == "." or config_name == "..":
        config_name = "Simulation"

    print(f"\n Analyzing file: {filepath}")
    print(f" Config name: {config_name}\n")
   
    analyze_rtt(filepath, config_name)
