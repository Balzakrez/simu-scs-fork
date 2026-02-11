#!/usr/bin/env python3
import os, re, sys
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results

# *********************************************************************************** #

def extract_node_id(module_str):
    match = re.search(r"node\[(\d+)\]", module_str)
    if match: return int(match.group(1)) 
    match_ue = re.search(r"ue\[(\d+)\]", module_str)
    if match_ue: return int(match_ue.group(1))
    return -1

# *********************************************************************************** #

def analyze_simu5g_throughput(filepath, config_name):
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # Simu5G RLC Throughput Vector
    TPUT_FILTER = "*rlcThroughputUl:vector*"

    print("Extracting Throughput vectors...")
    df = results.get_vectors(TPUT_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No RLC Throughput data found.")
        return

    print(f"Found {len(df)} vectors. Processing...")

    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    df = df.sort_values('NodeID')

    # Convert Bytes/s to kbps 
    # 1 Byte = 8 bits
    # 1000 bits = 1 kbit
    df['Tput_kbps'] = df['vecvalue'].apply(lambda x: np.array(x) * 8.0 / 1000.0)

    # Calculate Stats
    df['Mean_Tput'] = df['Tput_kbps'].apply(np.mean)
    
    global_mean = df['Mean_Tput'].mean()
    print(f"Global Mean Throughput: {global_mean:.2f} kbps")

    # Plot Mean Throughput 
    print("Generating Mean Throughput Bar Chart...")
    fig1, ax1 = plt.subplots(figsize=(14, 6))
    
    node_ids = df['NodeID']
    means = df['Mean_Tput']
    
    # Colors (Green > 75kbps, Orange > 40kbps, Red < 40kbps)
    colors = ['#4CAF50' if m > 75 else '#FF9800' if m > 40 else '#F44336' for m in means]

    ax1.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)
    ax1.axhline(y=global_mean, color='blue', linestyle='-', linewidth=2, label=f'Global Mean: {global_mean:.1f} kbps')
    
    ax1.set_xlabel('Node Index')
    ax1.set_ylabel('Throughput (kbps)')
    ax1.set_title(f'Simu5G RLC Throughput UL - {config_name}\n(Global Mean: {global_mean:.1f} kbps)', fontweight='bold')
    ax1.legend(loc='lower right')
    ax1.grid(axis='y', linestyle='--', alpha=0.5)
    
    plt.tight_layout()

    # Save plot
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_simu5g_throughput_mean_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    print(f"[OK] Saved {output_filename}")


# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_simu5g_throughput.py <path-file.vec>")
        sys.exit(1)
    filepath = sys.argv[1]
    config_name = filepath.split(os.sep)[-2] # Assuming config name is the parent directory name
    analyze_simu5g_throughput(filepath, config_name)