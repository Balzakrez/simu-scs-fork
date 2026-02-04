#!/usr/bin/env python3
import sys, os, re
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results

def extract_node_id(module_str):
    """
    Extracts the node ID from the module string (e.g., 'Network.node[5].app[0]').
    Returns the integer ID or -1 if not found.
    """
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1

# *********************************************************************************** #

def plot_pdr_per_node(filepath, config_name):
    """
    Computes and plots PDR per node using omnetpp.scave.results.
    PDR = Rx / Tx * 100.
    """
    
    # 1. Load Data
    # Filter directly the vectors of interest
    print(f"Loading vectors from {filepath}...")
    results.set_inputs(filepath)
    
    # Scave Filter: search for vectors named exactly pingTxSeq or pingRxSeq
    filter_expression = "*pingTxSeq:vector* OR *pingRxSeq:vector*"
    df = results.get_vectors(filter_expression)

    print(df.head())
    print(df.columns)
    print(df[["name"]])

    if df.empty:
        print(" [WARN] No pingTxSeq/pingRxSeq data found.")
        return

    # 2. Data Processing
    # Extract Node ID from the module name
    df['NodeID'] = df['module'].apply(extract_node_id)
    
    # Remove invalid nodes (if ID is -1)
    df = df[df['NodeID'] != -1]

    # The packet count is the length of the vector values
    df['PacketCount'] = df['vecvalue'].apply(len)

    # Create a Pivot Table:
    # Index: NodeID
    # Columns: name (pingTxSeq, pingRxSeq)
    # Values: PacketCount
    pdr_table = df.pivot_table(
        index='NodeID', 
        columns='name', 
        values='PacketCount', 
        fill_value=0 # If a node has no Tx or Rx, set to 0
    )

    print(pdr_table.head())
    print(pdr_table.columns)

    # Verify that both columns exist
    if 'pingTxSeq:vector' not in pdr_table.columns:
        print(" [WARN] Missing pingTxSeq data.")
        return
    if 'pingRxSeq:vector' not in pdr_table.columns:
        # If only Rx is missing, assume 0 received packets (column of zeros)
        pdr_table['pingRxSeq:vector'] = 0

    # Compute PDR
    # Handle division by zero if Tx is 0
    pdr_table['PDR'] = pdr_table.apply(
        lambda row: (row['pingRxSeq:vector'] / row['pingTxSeq:vector'] * 100) if row['pingTxSeq:vector'] > 0 else 0.0, 
        axis=1
    )

    # Sort by NodeID
    pdr_table = pdr_table.sort_index()

    # Prepare data for plotting
    node_indices = pdr_table.index.tolist()
    pdr_values = pdr_table['PDR'].tolist()

    if not node_indices:
        print(" [WARN] No valid nodes found after processing.")
        return

    # 3. Plotting
    fig, ax = plt.subplots(figsize=(14, 6))

    # Conditional coloring based on PDR value
    colors = []
    for p in pdr_values:
        if p >= 95:
            colors.append('#4CAF50') # Green (High Quality)
        elif p >= 80:
            colors.append('#FF9800') # Orange (Medium Quality)
        else:
            colors.append('#F44336') # Red (Low Quality)

    ax.bar(node_indices, pdr_values, color=colors, edgecolor='black', linewidth=0.5, width=0.8)

    # Threshold line (95%)
    ax.axhline(y=95, color='red', linestyle='--', alpha=0.7, label='Threshold 95%')

    # Mean line
    mean_pdr = np.mean(pdr_values)
    ax.axhline(y=mean_pdr, color='blue', linestyle='-', alpha=0.7, linewidth=2, label=f'Mean: {mean_pdr:.1f}%')

    # Chart decorations
    ax.set_xlabel('Node Index', fontsize=12)
    ax.set_ylabel('PDR (%)', fontsize=12)
    ax.set_title(f'PDR per node — {config_name} (mean={mean_pdr:.1f}%)', fontsize=14)
    ax.set_ylim(0, 105)
    ax.set_xlim(min(node_indices)-1, max(node_indices)+1)
    
    ax.legend(loc='lower left') # Placed bottom-right to avoid covering high bars
    ax.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()

    # Save plot
    output_filename = f'plot_pdr_{config_name}.png'
    plt.savefig(output_filename, dpi=150)
    plt.close()
    print(f" [OK] Plot saved: {output_filename}")


# *********************************************************************************** #

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_pdr_scave.py <path-file.vec>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    # Attempt to extract config name from folder path, otherwise use generic name
    config_name = filepath.split(os.sep)[0]
    if config_name == "." or config_name == "..":
        config_name = "Simulation"

    print(f" Analyzing file: {filepath}")
    print(f" Configuration: {config_name}\n")
    
    try:
        plot_pdr_per_node(filepath, config_name)
    except Exception as e:
        print(f" [ERR] Error during execution: {e}")
        import traceback
        traceback.print_exc()

    print("\n Done!")



# #!/usr/bin/env python3
# import sys
# import numpy as np
# import utils as ut
# import matplotlib.pyplot as plt


# def plot_pdr_per_node(vectors, config_name):
#     """
#     Compute and plot PDR per node.
#     PDR = Packet Delivery Ratio = packets received / packets sent
#     How we compute it from the .vec:
#     - pingTxSeq counts packets sent (count of the vector)
#     - pingRxSeq counts packets received (count of the vector)
#     - PDR = len(pingRxSeq) / len(pingTxSeq)
#     """

#     tx_vectors = ut.find_vectors_by_name(vectors, r'^pingTxSeq$')
#     rx_vectors = ut.find_vectors_by_name(vectors, r'^pingRxSeq$')
#     if not tx_vectors or not rx_vectors:
#         print("  [WARN] Nessun dato pingTxSeq/pingRxSeq trovato")
#         return
#     # tx_counts = { node_idx, <tx count> } for mapping node -> TX count
#     tx_counts = {}
#     for vec in tx_vectors:
#         node_idx = ut.extract_node_index(vec['module'])
#         if node_idx is not None:
#             tx_counts[node_idx] = len(vec['times'])  # Numero di pacchetti inviati
#     # rx_counts = { node_idx, <rx count> } for mapping node -> RX count
#     rx_counts = {}
#     for vec in rx_vectors:
#         node_idx = ut.extract_node_index(vec['module'])
#         if node_idx is not None:
#             rx_counts[node_idx] = len(vec['times'])  # Numero di pacchetti ricevuti
#     # Compute PDR per each node
#     node_indices = []
#     pdr_values = []
#     for node_idx in sorted(tx_counts.keys()):
#         tx = tx_counts[node_idx]
#         rx = rx_counts.get(node_idx, 0)
#         pdr = (rx / tx * 100) if tx > 0 else 0
#         node_indices.append(node_idx)
#         pdr_values.append(pdr)
    
#     # Plot
#     fig, ax = plt.subplots(figsize=(14, 5))
    
#     # Bar chart with color based on PDR (green if high, red if low)
#     colors = ['#4CAF50' if p >= 95 else '#FF9800' if p >= 80 else '#F44336' for p in pdr_values]
#     bars = ax.bar(node_indices, pdr_values, color=colors, edgecolor='black', linewidth=0.5)
    
#     # Reference line at 95%
#     ax.axhline(y=95, color='red', linestyle='--', alpha=0.7, label='Threshold 95%')
    
#     # Mean line
#     mean_pdr = float(np.mean(pdr_values))
#     ax.axhline(y=mean_pdr, color='blue', linestyle='-', alpha=0.7, linewidth=2, label=f'Mean: {mean_pdr:.1f}%')
#     ax.set_xlabel('Node')
#     ax.set_ylabel('PDR (%)')
#     ax.set_title(f'PDR per node — {config_name}  (mean={mean_pdr:.1f}%)')
#     ax.set_ylim(0, 105)
#     ax.legend()
#     ax.grid(axis='y', alpha=0.3)
#     plt.tight_layout()
    
#     # Save the plot
#     output_file = f'plot_pdr_{config_name}.png'
#     plt.savefig(output_file, dpi=150)
#     plt.close()
#     print(f"  [OK] Plot saved: {output_file}")

# # ==============================================================================
# # MAIN
# # ==============================================================================
# if __name__ == '__main__':
#     if len(sys.argv) < 2:
#         print("Usage: python pdrVec.py <path-file.vec>")
#         print("Example: python pdrVec.py results/CellularOnlyBaseline/0.vec")
#         sys.exit(1)
#     filepath = sys.argv[1]
#     config_name = filepath.split("/")[0]
   
#     print(f"  Parsing file: {filepath}")
#     print(f"  Configuration name: {config_name}")
#     vectors = ut.parse_vec(filepath)

    
#     print(f"  Vectors found: {len(vectors)}")

    
    
#     # 4. Generate the plots
#     print("\n  Generating plots...")
#     plot_pdr_per_node(vectors, config_name)

#     print("\n  Done!")