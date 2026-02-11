#!/usr/bin/env python3
import sys, os, re
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results


# *********************************************************************************** #

def extract_node_id(module_str):
    """
    Extracts the node ID from the module string (e.g., 'Network.node[5].manager').
    Returns the integer ID or -1 if not found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1


# *********************************************************************************** #

def plot_single_metric(df, metric_name, config_name, lower_is_better=False):
    """
    Helper function to plot a single metric dataframe.
    lower_is_better: If True (e.g., for RTT), values below mean are Green.
    """
    
    # Clean and Group Data
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]

    # Calculate mean per node (aggregates multiple runs/seeds)
    df_grouped = df.groupby('NodeID')['value'].mean().reset_index()
    df_grouped = df_grouped.sort_values('NodeID')

    if df_grouped.empty:
        print(f" [SKIP] No valid data for {metric_name}")
        return

    node_indices = df_grouped['NodeID'].tolist()
    values = df_grouped['value'].tolist()
    global_mean = np.mean(values)

    # Plot Setup
    fig, ax = plt.subplots(figsize=(12, 6))

    # Color Logic
    colors = []
    for v in values:
        if lower_is_better:
            # For RTT: Lower than mean is Green (Good)
            colors.append('#4CAF50' if v <= global_mean else '#FF9800')
        else:
            # For PDR/Count: Higher than mean is Green (Good)
            colors.append('#4CAF50' if v >= global_mean else '#FF9800')

    ax.bar(node_indices, values, color=colors, edgecolor='black', alpha=0.8, width=0.8)

    # Mean Line
    ax.axhline(y=global_mean, color='blue', linestyle='--', linewidth=2, label=f'Mean: {global_mean:.4f}')

    # Labels
    ax.set_xlabel('Node Index', fontsize=12)
    ax.set_ylabel(metric_name, fontsize=12)
    ax.set_title(f'{metric_name}\nConfig: {config_name}', fontsize=14)
    
    # ax.set_xticks(node_indices)
    # ax.set_ylim(0, 105)
    ax.set_xlim(min(node_indices)-1, max(node_indices)+1)
    
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()

    # Save
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_{metric_name}_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    plt.close()
    print(f" [OK] Saved: {output_filename}")

# *********************************************************************************** #

def analyze_manager_metrics(filepath, config_name):
    """
    Analyzes the specific satellite and cellular metrics requested.
    """
    print(f" Loading scalars from {filepath}...")
    results.set_inputs(filepath)

    # List of metrics to analyze. 
    # Tuple format: (Metric Name, Lower_Is_Better_Boolean)
    metrics_to_analyze = [
        ("satellite_avg_rtt", True),    # RTT: Lower is better
        ("satellite_pdr", False),       # PDR: Higher is better
        ("satellite_ping_count", False),# Count: Higher is usually better (activity)
        ("cellular_avg_rtt", True),     # RTT: Lower is better
        ("cellular_pdr", False),        # PDR: Higher is better
        ("cellular_ping_count", False)  # Count: Higher is usually better
    ]

    for metric, lower_is_better in metrics_to_analyze:
        print(f" Processing metric: {metric}...")
        
        # Filter: Look for the specific scalar name
        # We use *metric* to match cases like "manager.satellite_avg_rtt"
        filter_expr = f"*{metric}*"
        
        try:
            df = results.get_scalars(filter_expr)
            if df.empty:
                print(f" [WARN] Metric not found: {metric}")
                continue
            
            plot_single_metric(df, metric, config_name, lower_is_better)
            
        except Exception as e:
            print(f" [ERR] Failed processing {metric}: {e}")

# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python computeScalars.py <path-to-sca-files>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    config_name = filepath.split(os.sep)[-2] if len(filepath.split(os.sep)) > 1 else "Simulation"

    print(f" Analysis Configuration: {config_name}")
    
    try:
        analyze_manager_metrics(filepath, config_name)
    except Exception as e:
        print(f" [ERR] Critical Error: {e}")
        import traceback
        traceback.print_exc()

    print("\n Done!")