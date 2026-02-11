#!/usr/bin/env python3
import sys
import os
import re
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
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

def analyze_throughput(filepath, config_name):
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # Filter for the Throughput vector
    THROUGHPUT_FILTER = "*currentThroughput:vector*"

    print("Extracting Throughput vectors...")
    df = results.get_vectors(THROUGHPUT_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No Throughput data found. Check if 'currentThroughput' is recorded in .vec.")
        return

    print(f"Found {len(df)} vectors. Processing...")

    # Extract Node ID
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1] # Remove invalid nodes
    df = df.sort_values('NodeID')

    # Convert Throughput from bps to kbps (Kilobits per second)
    # 1 kbps = 1000 bps
    df['Throughput_kbps'] = df['vecvalue'].apply(lambda x: np.array(x) / 1000.0)

    # Calculate Aggregate Stats per Node
    df['Mean_Tput'] = df['Throughput_kbps'].apply(np.mean)
    
    # Calculate Global Mean across all nodes 
    global_mean_tput = df['Mean_Tput'].mean()
    print(f"Global Mean Throughput: {global_mean_tput:.2f} kbps")

    print("Generating Mean Throughput Bar Chart...")
    fig1, ax1 = plt.subplots(figsize=(14, 6))
    
    node_ids = df['NodeID']
    means = df['Mean_Tput']
    
    # If mean Throughput > 75kbps: Green, >40kbps: Orange, else Red
    colors = []
    for m in means:
        if m > 75: colors.append('#4CAF50')      # Green (High/Good)
        elif m > 40: colors.append('#FF9800')    # Orange (Medium)
        else: colors.append('#F44336')           # Red (Low/Bad)

    # Plot Bars
    ax1.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Threshold Lines
    ax1.axhline(75, color='green', linestyle='--', alpha=0.5, label='Target (~75+ kbps)')
    ax1.axhline(40, color='red', linestyle='--', alpha=0.5, label='Poor (< 40 kbps)')

    # Global Mean Line (Solid Blue)
    ax1.axhline(y=global_mean_tput, color='blue', linestyle='-', linewidth=2, 
                label=f'Global Mean: {global_mean_tput:.1f} kbps')

    ax1.set_xlabel('Node Index', fontsize=12)
    ax1.set_ylabel('Average Throughput (kbps)', fontsize=12)
    
    # Title with Global Mean
    ax1.set_title(f'Average Throughput per Node - {config_name}\n(Global Mean: {global_mean_tput:.1f} kbps)', 
                  fontsize=14, fontweight='bold')
    
    ax1.set_xlim(min(node_ids)-1, max(node_ids)+1)
    ax1.legend(loc='lower right') # Legend at bottom so it doesn't cover high bars
    ax1.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()

    # Save plot
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_throughput_mean_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    print(f"[OK] Saved plot: {output_filename}")

    # print("Generating Throughput Box Plot (Distribution)...")
    # fig2, ax2 = plt.subplots(figsize=(14, 7))
    # data_to_plot = df['Throughput_kbps'].tolist()
    # labels = df['NodeID'].astype(str).tolist()
    # bplot = ax2.boxplot(data_to_plot, label=labels, patch_artist=True, showfliers=False) 
    # # Color boxes Teal/Cyan to distinguish from RTT/Jitter
    # for patch in bplot['boxes']:
    #     patch.set_facecolor('#00BCD4') # Cyan/Teal
    #     patch.set_alpha(0.6)
    # ax2.set_xlabel('Node Index', fontsize=12)
    # ax2.set_ylabel('Throughput (kbps)', fontsize=12)
    # ax2.set_title(f'Throughput Distribution - {config_name}', fontsize=14)
    # if len(labels) > 20:
    #     plt.xticks(rotation=90, fontsize=8)
    # ax2.grid(axis='y', linestyle='--', alpha=0.5)
    # plt.tight_layout()
    # dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    # output_filename = f'{dir_name}/plot_throughput_boxplot_{config_name}.png'
    # plt.savefig(output_filename, dpi=150)
    # print(f"[OK] Saved plot: {output_filename}")

# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_throughput_analysis.py <path-file.vec>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    config_name = filepath.split(os.sep)[-2] # Assuming config name is the parent directory name
    if config_name == "." or config_name == "..":
        config_name = "Simulation"

    print(f"\n Analyzing file: {filepath}")
    print(f" Config name: {config_name}\n")
   
    analyze_throughput(filepath, config_name)