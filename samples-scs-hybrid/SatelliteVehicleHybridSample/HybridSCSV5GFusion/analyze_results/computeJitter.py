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

def analyze_jitter(filepath, config_name):
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # Filter for the Jitter vectors
    JITTER_FILTER = "*currentJitter:vector*"

    print("Extracting Jitter vectors...")
    df = results.get_vectors(JITTER_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No Jitter data found. Check if 'currentJitter' is recorded in .vec.")
        return

    print(f"Found {len(df)} vectors. Processing...")

    # Extract Node ID
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1] # Remove invalid nodes
    df = df.sort_values('NodeID')

    # Convert Jitter from Seconds to Milliseconds
    df['Jitter_ms'] = df['vecvalue'].apply(lambda x: np.array(x) * 1000.0)

    # Calculate Aggregate Stats per Node
    df['Mean_Jitter'] = df['Jitter_ms'].apply(np.mean)
    
    # Calculate Global Mean across all nodes 
    global_mean_jitter = df['Mean_Jitter'].mean()
    print(f"Global Mean Jitter: {global_mean_jitter:.2f} ms")

   
    print("Generating Mean Jitter Bar Chart...")
    
    fig1, ax1 = plt.subplots(figsize=(14, 6))
    
    node_ids = df['NodeID']
    means = df['Mean_Jitter']
    
    # Green < 10ms (Excellent), Orange < 30ms (Acceptable), Red > 30ms (Poor)
    colors = []
    for m in means:
        if m < 10: colors.append('#4CAF50')      # Green
        elif m < 30: colors.append('#FF9800')    # Orange
        else: colors.append('#F44336')           # Red

    # Plot Bars
    ax1.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Threshold Lines
    ax1.axhline(10, color='green', linestyle='--', alpha=0.5, label='Excellent (10ms)')
    ax1.axhline(30, color='red', linestyle='--', alpha=0.5, label='Poor Threshold (30ms)')

    # Global Mean Line (Solid Blue)
    ax1.axhline(y=global_mean_jitter, color='blue', linestyle='-', linewidth=2, 
                label=f'Global Mean: {global_mean_jitter:.1f} ms')

    ax1.set_xlabel('Node Index', fontsize=12)
    ax1.set_ylabel('Average Jitter (ms)', fontsize=12)
    
    # Title with Global Mean
    ax1.set_title(f'Average Jitter per Node - {config_name}\n\
                    (Global Mean: {global_mean_jitter:.1f} ms)', 
                    fontsize=14, fontweight='bold')
    
    ax1.set_xlim(min(node_ids)-1, max(node_ids)+1)
    ax1.legend(loc='upper right')
    ax1.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()
    
    # Save plot
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_jitter_mean_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    print(f"[OK] Saved {output_filename}")

# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_jitter_analysis.py <path-file.vec>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    config_name = filepath.split(os.sep)[-2] # Assuming config name is the parent directory name
    if config_name == "." or config_name == "..":
        config_name = "Simulation"

    print(f"\n Analyzing file: {filepath}")
    print(f" Config name: {config_name}\n")
   
    try:
        analyze_jitter(filepath, config_name)
    except Exception as e:
        print(f"[ERR] An error occurred: {e}")
        import traceback
        traceback.print_exc()

    print("\n Done!")