#!/usr/bin/env python3
"""
Script to extract and visualize cumulative statistics (satellite vs cellular)
from OMNeT++ .sca files using omnetpp.scave.results.

Usage: python computeCumulativeStats.py <path-file.sca>
"""

import sys
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from omnetpp.scave import results



# *********************************************************************************** #

def extract_node_id(module_str):
    """
    Extracts the node ID from the module string.
    Example: 'Network.node[5].interfaceManager' -> 5
    Returns the integer ID or -1 if not found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1

# *********************************************************************************** #

def load_cumulative_scalars(filepath):
    """
    Loads cumulative statistics from .sca file.
    Returns a DataFrame with columns: NodeID, metric, satellite_value, cellular_value
    """
    print(f"Loading scalars from {filepath}")
    results.set_inputs(filepath)
    
    # Filter for cumulative statistics
    filter_expr = (
        "*satelliteCumulative* OR "
        "*cellularCumulative* OR "
        "*satelliteTotalBytes* OR "
        "*cellularTotalBytes* OR "
        "*totalBytesTransferred*"
    )
    
    df = results.get_scalars(filter_expr)
    
    if df.empty:
        print(" [WARN] No cumulative statistics found in .sca file")
        return None
    
    # Extract NodeID
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    
    if df.empty:
        print(" [WARN] No valid nodes found")
        return None
    
    return df


# *********************************************************************************** #

def process_cumulative_data(df):
    """
    Processes raw scalar data into a structured format.
    Returns dict with per-node statistics.
    """
    stats_dict = {}
    
    for node_id in df['NodeID'].unique():
        node_data = df[df['NodeID'] == node_id]
        
        stats_dict[node_id] = {
            'satellite': {
                'throughput': 0.0,
                'rtt': 0.0,
                'pdr': 0.0,
                'jitter': 0.0,
                'bytes': 0.0
            },
            'cellular': {
                'throughput': 0.0,
                'rtt': 0.0,
                'pdr': 0.0,
                'jitter': 0.0,
                'bytes': 0.0
            },
            'total_bytes': 0.0
        }
        
        for _, row in node_data.iterrows():
            name = row['name']
            value = row['value']
            
            # Satellite metrics
            if 'satelliteCumulativeThroughput' in name:
                stats_dict[node_id]['satellite']['throughput'] = value
            elif 'satelliteCumulativeRTT' in name:
                stats_dict[node_id]['satellite']['rtt'] = value
            elif 'satelliteCumulativePDR' in name:
                stats_dict[node_id]['satellite']['pdr'] = value
            elif 'satelliteCumulativeJitter' in name:
                stats_dict[node_id]['satellite']['jitter'] = value
            elif 'satelliteTotalBytes' in name:
                stats_dict[node_id]['satellite']['bytes'] = value
            
            # Cellular metrics
            elif 'cellularCumulativeThroughput' in name:
                stats_dict[node_id]['cellular']['throughput'] = value
            elif 'cellularCumulativeRTT' in name:
                stats_dict[node_id]['cellular']['rtt'] = value
            elif 'cellularCumulativePDR' in name:
                stats_dict[node_id]['cellular']['pdr'] = value
            elif 'cellularCumulativeJitter' in name:
                stats_dict[node_id]['cellular']['jitter'] = value
            elif 'cellularTotalBytes' in name:
                stats_dict[node_id]['cellular']['bytes'] = value
            
            # Total
            elif 'totalBytesTransferred' in name:
                stats_dict[node_id]['total_bytes'] = value
    
    return stats_dict


# *********************************************************************************** #

def print_summary_table(stats_dict, config_name):
    """
    Prints a formatted summary table of cumulative statistics.
    """
    print(f"\n{'='*80}")
    print(f" CUMULATIVE STATISTICS SUMMARY — {config_name}")
    print(f"{'='*80}\n")
    
    for node_id in sorted(stats_dict.keys()):
        stats = stats_dict[node_id]
        
        print(f"Node [{node_id}]:")
        print(f"  {'Metric':<25} {'Satellite':<20} {'Cellular':<20}")
        print(f"  {'-'*65}")
        
        # Throughput
        sat_thr = stats['satellite']['throughput'] / 1000  # bps -> kbps
        cell_thr = stats['cellular']['throughput'] / 1000
        print(f"  {'Throughput (kbps)':<25} {sat_thr:>18.2f}  {cell_thr:>18.2f}")
        
        # RTT
        sat_rtt = stats['satellite']['rtt'] * 1000  # s -> ms
        cell_rtt = stats['cellular']['rtt'] * 1000
        print(f"  {'RTT (ms)':<25} {sat_rtt:>18.2f}  {cell_rtt:>18.2f}")
        
        # PDR
        sat_pdr = stats['satellite']['pdr'] * 100  # ratio -> %
        cell_pdr = stats['cellular']['pdr'] * 100
        print(f"  {'PDR (%)':<25} {sat_pdr:>18.2f}  {cell_pdr:>18.2f}")
        
        # Jitter
        sat_jit = stats['satellite']['jitter'] * 1000  # s -> ms
        cell_jit = stats['cellular']['jitter'] * 1000
        print(f"  {'Jitter (ms)':<25} {sat_jit:>18.2f}  {cell_jit:>18.2f}")
        
        # Bytes
        sat_bytes = stats['satellite']['bytes'] / 1024  # B -> KB
        cell_bytes = stats['cellular']['bytes'] / 1024
        print(f"  {'Bytes Transferred (KB)':<25} {sat_bytes:>18.2f}  {cell_bytes:>18.2f}")
        
        # Total
        total_bytes = stats['total_bytes'] / 1024  # B -> KB
        print(f"  {'Total Bytes (KB)':<25} {total_bytes:>18.2f}")
        print()


# *********************************************************************************** #

def plot_comparison_charts(stats_dict, config_name, output_dir):
    """
    Creates comparison charts for satellite vs cellular metrics.
    """
    # Aggregate data across all nodes
    sat_thr = np.mean([s['satellite']['throughput'] for s in stats_dict.values()])
    cell_thr = np.mean([s['cellular']['throughput'] for s in stats_dict.values()])
    
    sat_rtt = np.mean([s['satellite']['rtt'] for s in stats_dict.values()])
    cell_rtt = np.mean([s['cellular']['rtt'] for s in stats_dict.values()])
    
    sat_pdr = np.mean([s['satellite']['pdr'] for s in stats_dict.values()])
    cell_pdr = np.mean([s['cellular']['pdr'] for s in stats_dict.values()])
    
    sat_bytes = np.sum([s['satellite']['bytes'] for s in stats_dict.values()])
    cell_bytes = np.sum([s['cellular']['bytes'] for s in stats_dict.values()])
    
    # Create figure with 2x2 subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f'Satellite vs Cellular — Cumulative Statistics — {config_name}', 
                 fontsize=16, fontweight='bold')
    
    # 1. Throughput comparison
    ax = axes[0, 0]
    x = ['Satellite', 'Cellular']
    y = [sat_thr/1000, cell_thr/1000]  # bps -> kbps
    colors = ['#2196F3', '#4CAF50']
    bars = ax.bar(x, y, color=colors, edgecolor='black', linewidth=1.5)
    ax.set_ylabel('Throughput (kbps)', fontsize=11)
    ax.set_title('Average Throughput', fontsize=12, fontweight='bold')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    
    # Add value labels on bars
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.1f}',
                ha='center', va='bottom', fontsize=10)
    
    # 2. RTT comparison
    ax = axes[0, 1]
    y = [sat_rtt*1000, cell_rtt*1000]  # s -> ms
    bars = ax.bar(x, y, color=colors, edgecolor='black', linewidth=1.5)
    ax.set_ylabel('RTT (ms)', fontsize=11)
    ax.set_title('Average RTT', fontsize=12, fontweight='bold')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.1f}',
                ha='center', va='bottom', fontsize=10)
    
    # 3. PDR comparison
    ax = axes[1, 0]
    y = [sat_pdr*100, cell_pdr*100]  # ratio -> %
    bars = ax.bar(x, y, color=colors, edgecolor='black', linewidth=1.5)
    ax.set_ylabel('PDR (%)', fontsize=11)
    ax.set_title('Packet Delivery Ratio', fontsize=12, fontweight='bold')
    ax.set_ylim(0, 105)
    ax.axhline(y=95, color='red', linestyle='--', alpha=0.7, label='Target 95%')
    ax.legend(loc='lower right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.1f}%',
                ha='center', va='bottom', fontsize=10)
    
    # 4. Bytes transferred comparison
    ax = axes[1, 1]
    y = [sat_bytes/1024, cell_bytes/1024]  # B -> KB
    bars = ax.bar(x, y, color=colors, edgecolor='black', linewidth=1.5)
    ax.set_ylabel('Data Transferred (KB)', fontsize=11)
    ax.set_title('Total Data Transferred', fontsize=12, fontweight='bold')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{height:.1f}',
                ha='center', va='bottom', fontsize=10)
    
    plt.tight_layout()
    
    # Save plot
    output_filename = f'{output_dir}/plot_cumulative_comparison_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    plt.close()
    print(f" [OK] Comparison plot saved: {output_filename}")


# *********************************************************************************** #

def plot_per_node_breakdown(stats_dict, config_name, output_dir):
    """
    Creates a breakdown plot showing satellite vs cellular usage per node.
    """
    if len(stats_dict) == 0:
        return
    
    node_ids = sorted(stats_dict.keys())
    sat_bytes = [stats_dict[n]['satellite']['bytes']/1024 for n in node_ids]  # KB
    cell_bytes = [stats_dict[n]['cellular']['bytes']/1024 for n in node_ids]
    
    fig, ax = plt.subplots(figsize=(14, 6))
    
    x = np.arange(len(node_ids))
    width = 0.35
    
    bars1 = ax.bar(x - width/2, sat_bytes, width, label='Satellite', 
                   color='#2196F3', edgecolor='black', linewidth=0.5)
    bars2 = ax.bar(x + width/2, cell_bytes, width, label='Cellular', 
                   color='#4CAF50', edgecolor='black', linewidth=0.5)
    
    ax.set_xlabel('Node Index', fontsize=12)
    ax.set_ylabel('Data Transferred (KB)', fontsize=12)
    ax.set_title(f'Data Transfer per Node (Satellite vs Cellular) — {config_name}', 
                 fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(node_ids)
    ax.legend()
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    
    output_filename = f'{output_dir}/plot_per_node_breakdown_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    plt.close()
    print(f" [OK] Per-node breakdown plot saved: {output_filename}")


# *********************************************************************************** #

def export_to_csv(stats_dict, config_name, output_dir):
    """
    Exports cumulative statistics to CSV for further analysis.
    """
    rows = []
    
    for node_id in sorted(stats_dict.keys()):
        stats = stats_dict[node_id]
        
        row = {
            'NodeID': node_id,
            'Config': config_name,
            'Satellite_Throughput_bps': stats['satellite']['throughput'],
            'Satellite_RTT_s': stats['satellite']['rtt'],
            'Satellite_PDR': stats['satellite']['pdr'],
            'Satellite_Jitter_s': stats['satellite']['jitter'],
            'Satellite_Bytes': stats['satellite']['bytes'],
            'Cellular_Throughput_bps': stats['cellular']['throughput'],
            'Cellular_RTT_s': stats['cellular']['rtt'],
            'Cellular_PDR': stats['cellular']['pdr'],
            'Cellular_Jitter_s': stats['cellular']['jitter'],
            'Cellular_Bytes': stats['cellular']['bytes'],
            'Total_Bytes': stats['total_bytes']
        }
        rows.append(row)
    
    df = pd.DataFrame(rows)
    
    output_filename = f'{output_dir}/cumulative_stats_{config_name}.csv'
    df.to_csv(output_filename, index=False)
    print(f" [OK] CSV exported: {output_filename}")


# *********************************************************************************** #

def main(filepath):
    """
    Main execution function.
    """
    # Extract config name from path
    config_name = filepath.split(os.sep)[-2]
    if config_name in [".", ".."]:
        config_name = "Simulation"
    
    print(f" Analyzing file: {filepath}")
    print(f" Configuration: {config_name}\n")
    
    # Load data
    df = load_cumulative_scalars(filepath)
    if df is None:
        return
    
    # Process data
    stats_dict = process_cumulative_data(df)
    
    if not stats_dict:
        print(" [WARN] No statistics to process")
        return
    
    # Output directory
    output_dir = filepath.rsplit(os.sep, 1)[0]
    
    # Print summary table
    print_summary_table(stats_dict, config_name)
    
    # Generate plots
    plot_comparison_charts(stats_dict, config_name, output_dir)
    plot_per_node_breakdown(stats_dict, config_name, output_dir)
    
    # Export CSV
    # export_to_csv(stats_dict, config_name, output_dir)
    
    print("\n [OK] Analysis complete!")


# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_cumulative_stats.py <path-file.sca>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    if not filepath.endswith('.sca'):
        print(" [WARN] Expected a .sca file, got:", filepath)
    
    try:
        main(filepath)
    except Exception as e:
        print(f" [ERR] Error during execution: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)