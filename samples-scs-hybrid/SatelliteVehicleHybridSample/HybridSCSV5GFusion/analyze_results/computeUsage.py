import os
import sys, re
import matplotlib.pyplot as plt
from omnetpp.scave import results

# *********************************************************************************** #

def extract_node_id(module_str):
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1)) 
    return -1

# *********************************************************************************** #

def plot(filepath: str, config_name: str) -> None:
   
    # FILTER_CELL = "*cellUsageTime:vector*"
    # FILTER_SAT = "*satUsageTime:vector*"
    COMBINED_FILTER = "*cellUsageTime:vector* OR *satUsageTime:vector*"

    print(f"Caricamento file: {filepath}...")
    results.set_inputs(filepath)

    print("Estrazione vettori...")

    df_vectors = results.get_vectors(COMBINED_FILTER, include_attrs=True)

    if df_vectors.empty:
        print("Nessun vettore trovato.")
    else:
        print(f"Trovati {len(df_vectors)} vettori.")
        # print(f"{df_vectors.head()}")
        # print(f"{df_vectors.columns}")
        # print(f"{df_vectors["source"]}")
        # print(f"{df_vectors[["source","vectime"]]}")
        # print(f"{df_vectors[["source","vecvalue"]]}")


        print("Elaborazione dati...\n")
        # Add a new column 'final_time' extracting the last value from 'vecvalue'
        df_vectors['final_time'] = df_vectors['vecvalue'].apply(lambda x: x[-1] if len(x) > 0 else 0.0)
        # Add a new column 'NodeID' extracting the node ID from 'module'
        df_vectors['NodeID'] = df_vectors['module'].apply(extract_node_id)

        usage_summary = df_vectors.pivot_table(
            index='NodeID', 
            columns='source', # Used "source" to differentiate between cellUsageSignal and satUsageSignal
            values='final_time',
            fill_value=0.0 # If a node never used an interface, set to 0
        )

        # Rename columns for clarity
        usage_summary = usage_summary.rename(columns={
            'cellUsageSignal': 'Cellular_Time_s', 
            'satUsageSignal': 'Satellite_Time_s'
        })

        # Calculate total time and percentages
        usage_summary['Total_Time_s'] = usage_summary['Cellular_Time_s'] + usage_summary['Satellite_Time_s']
        usage_summary['Cell_%'] = (usage_summary['Cellular_Time_s'] / usage_summary['Total_Time_s']) * 100
        usage_summary['Sat_%'] = (usage_summary['Satellite_Time_s'] / usage_summary['Total_Time_s']) * 100

        # Sort by NodeID
        usage_summary = usage_summary.sort_index()

        # Display the summary
        print(usage_summary)

        # Global Statistics
        global_avg_cell = usage_summary["Cellular_Time_s"].mean()
        global_avg_sat = usage_summary["Satellite_Time_s"].mean()
        print(f"\nGlobal Avg Cellular Time: {global_avg_cell}")
        print(f"Global Avg Satellite Time: {global_avg_sat}")

        # How many nodes have prefferred one interface over the other
        sat_lovers = usage_summary["Sat_%"] > usage_summary["Cell_%"]
        print(f"\nSattellite Lovers: {sat_lovers.sum()} nodes") # 1 True, 0 False
        cell_lovers = usage_summary["Sat_%"] < usage_summary["Cell_%"]
        print(f"Cellular Lovers: {cell_lovers.sum()} nodes") # 1 True, 0 False
        equal_lovers = usage_summary["Sat_%"] == usage_summary["Cell_%"]
        print(f"Equal Lovers: {equal_lovers.sum()} nodes") # 1 True, 0 False
        print(f"Total Nodes: {sat_lovers.sum() + cell_lovers.sum() + equal_lovers.sum()} nodes\n")
        
        # Plotting
        bar_plot_usage(usage_summary, config_name)
        pie_plot_usage(usage_summary, config_name)



# *********************************************************************************** #
def bar_plot_usage(usage_summary, config_name):
    print("Generating plot...")
    # Setup the plot
    fig, ax = plt.subplots(figsize=(14, 7))
    
    # Data for plotting
    node_ids = usage_summary.index
    cell_pct = usage_summary['Cell_%']
    sat_pct = usage_summary['Sat_%']
    
    # Bar plot
    ax.bar(node_ids, cell_pct, label='Cellular', color='#4CAF50', edgecolor='black', width=0.8)
    ax.bar(node_ids, sat_pct, bottom=cell_pct, label='Satellite', color='#2196F3', edgecolor='black', width=0.8)
    
    # Decorations
    ax.set_xlabel('Node Index', fontsize=12)
    ax.set_ylabel('Time Usage (%)', fontsize=12)
    ax.set_title(f'Interface Usage Distribution - {config_name}', fontsize=14)
    ax.set_ylim(0, 100)
    ax.set_xlim(min(node_ids)-1, max(node_ids)+1)
    
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)
    
    # Line for mean values
    mean_sat = sat_pct.mean()
    ax.axhline(mean_sat, color='blue', linestyle='--', linewidth=2, label=f'Mean Sat: {mean_sat:.1f}%')
    ax.legend()
    
    plt.tight_layout()
    
    # Save plot
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/plot_interface_usage_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    print(f"[OK] Saved plot: {output_filename}")

# *********************************************************************************** #

def pie_plot_usage(usage_summary, config_name):
    # Aggregate Calculation (Sum of all nodes)
    if 'Cellular_Time_s' in usage_summary.columns:
        total_cell_time = usage_summary['Cellular_Time_s'].sum()
    else:
        total_cell_time = 0.0

    if 'Satellite_Time_s' in usage_summary.columns:
        total_sat_time = usage_summary['Satellite_Time_s'].sum()
    else:
        total_sat_time = 0.0
    
    total_global_time = total_cell_time + total_sat_time
    
    # Check for empty data 
    if total_global_time <= 0:
        print("[WARN] Total accumulated time is 0. Cannot generate Pie Chart.")
        return

    print("Generating pie chart...")

    # Plotting
    fig, ax = plt.subplots(figsize=(9, 9))
    
    # Data configuration
    sizes = [total_cell_time, total_sat_time]
    labels = ['Cellular', 'Satellite']
    colors = ['#4CAF50', '#2196F3'] # Green (Cell), Blue (Sat)
    explode = (0.1, 0)  # Slightly "explode" the Cellular slice for visual effect

    # Creation of the Pie Chart
    pie_result = ax.pie(
        sizes, 
        explode=explode, 
        labels=labels, 
        colors=colors,
        autopct='%1.1f%%',  # Show percentage with 1 decimal place
        shadow=True, 
        startangle=90,
        textprops={'fontsize': 14}
    )

    # Unpacking the result manually
    wedges = pie_result[0]
    texts = pie_result[1]
    if len(pie_result) > 2:
        autotexts = pie_result[2]
        # Styling inner percentage text
        plt.setp(autotexts, size=14, weight="bold", color="white")
    
    # Title
    ax.set_title(f'Interface Usage Distribution\n{config_name}', fontsize=16, fontweight='bold')
    
    plt.tight_layout()
    
    # Save to file
    dir_name = filepath.rsplit(os.sep, 1)[0] # Assuming config name is the parent directory name
    print(f" Saving plot for {config_name} in directory: {dir_name}")
    output_filename = f'{dir_name}/pie_interface_usage_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    plt.close() # Close figure to free memory
    print(f"[OK] Saved pie chart: {output_filename}")

# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python script.py <path-file.vec>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    config_name = filepath.split(os.sep)[-2]  # Assuming config name is the parent directory name
    if config_name == "." or config_name == "..":
        config_name = "Simulation"

    print(f"\n  Analyzing file: {filepath}")
    print(f"  Config name: {config_name}\n")
   
    plot(filepath, config_name)

    print("\n  Done!")