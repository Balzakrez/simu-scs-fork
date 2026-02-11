#!/usr/bin/env python3
import os, re, sys
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results

# *********************************************************************************** #

def extract_node_id(module_str):
    """
    Extracts the integer Node ID from the module string.
    Adapts to Simu5G paths like 'Network.node[0].cellularNic...'
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1)) 
    # Fallback for pure Simu5G naming (e.g. ue[0])
    match_ue = re.search(r"ue\[(\d+)\]", module_str)
    if match_ue:
        return int(match_ue.group(1))
    return -1

# *********************************************************************************** #

def calculate_jitter(delay_vector):
    """
    Calculates Jitter from a Delay vector.
    Jitter = |Delay(i+1) - Delay(i)|
    Returns values in Milliseconds.
    """
    if len(delay_vector) < 2:
        return np.array([]) # Not enough samples
    
    delays_ms = np.array(delay_vector) * 1000.0 # Convert s to ms
    # Calculate absolute difference between consecutive delays
    jitter = np.abs(np.diff(delays_ms))
    return jitter

# *********************************************************************************** #

def analyze_simu5g_jitter(filepath, config_name):
    print(f"Loading file: {filepath}...")
    results.set_inputs(filepath)

    # Simu5G Delay Vector
    DELAY_FILTER = "*rlcPduDelayUl:vector*"

    print("Extracting Delay vectors to calculate Jitter...")
    df_vectors = results.get_vectors(DELAY_FILTER, include_attrs=True)

    if df_vectors.empty:
        print("[WARN] No RLC Delay data found.")
        return

    print(f"Found {len(df_vectors)} vectors. Processing...")

    # Add NodeID to DataFrame
    df_vectors['NodeID'] = df_vectors['module'].apply(extract_node_id)
    df_vectors = df_vectors[df_vectors['NodeID'] != -1]
    df_vectors = df_vectors.sort_values('NodeID')

    # KEY STEP: Calculate Jitter from Delay 
    df_vectors['Jitter_ms'] = df_vectors['vecvalue'].apply(lambda x: calculate_jitter(x))
    
    # Remove nodes with insufficient data (empty jitter arrays)
    df_vectors = df_vectors[df_vectors['Jitter_ms'].apply(len) > 0]

    # Calculate Stats
    df_vectors['Mean_Jitter'] = df_vectors['Jitter_ms'].apply(np.mean)
    
    # Global Mean Jitter
    global_mean = df_vectors['Mean_Jitter'].mean()
    print(f"Global Mean Calculated Jitter: {global_mean:.2f} ms")

    # Plot Mean Jitter 
    print("Generating Mean Jitter Bar Chart...")
    fig1, ax1 = plt.subplots(figsize=(14, 6))
    
    node_ids = df_vectors['NodeID']
    means = df_vectors['Mean_Jitter']
    
    # if mean Jitter < 10ms: Green, <30ms: Orange, else Red
    colors = ['#4CAF50' if m < 10 else '#FF9800' if m < 30 else '#F44336' for m in means]

    ax1.bar(node_ids, means, color=colors, edgecolor='black', width=0.8, alpha=0.8)
    ax1.axhline(y=global_mean, color='blue', linestyle='-', linewidth=2, label=f'Global Mean: {global_mean:.1f} ms')
    
    ax1.set_xlabel('Node Index')
    ax1.set_ylabel('Calculated Jitter (ms)')
    ax1.set_title(f'Simu5G RLC Jitter (Derived from Delay) - {config_name}\n(Global Mean: {global_mean:.1f} ms)', fontweight='bold')
    ax1.legend()
    ax1.grid(axis='y', linestyle='--', alpha=0.5)
    
    plt.tight_layout()

    # Save plot
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_simu5g_jitter_mean_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    print(f"[OK] Saved {output_filename}")

# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_simu5g_jitter.py <path-file.vec>")
        sys.exit(1)
    filepath = sys.argv[1]
    config_name = filepath.split(os.sep)[-2] # Assuming config name is the parent directory name
    if config_name == "." or config_name == "..":
        config_name = "Simulation"
    analyze_simu5g_jitter(filepath, config_name)