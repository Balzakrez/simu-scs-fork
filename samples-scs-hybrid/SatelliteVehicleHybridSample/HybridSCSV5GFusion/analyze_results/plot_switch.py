#!/usr/bin/env python3
import sys
import os
import re
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, BLUE, GREEN, ORANGE, VERMILLION

def extract_node_id(module_str):
    """Extracts the numeric node index from a module path string."""
    match = re.search(r"node\[(\d+)\]", module_str)
    if match:
        return int(match.group(1))
    return -1

def analyze_switch_counts(filepath, config_name):
    print(f"Load file: {filepath}...")
    results.set_inputs(filepath)

    # Filtriamo il vettore switchCount generato dal HybridInterfaceManager
    SWITCH_FILTER = "*switchCount:vector*"

    print("Extracting switch count vectors...")
    df = results.get_vectors(SWITCH_FILTER, include_attrs=True)

    if df.empty:
        print("[WARN] No 'switchCount' data found. Check the .vec file.")
        return

    # Estrazione degli ID e pulizia dei dati
    df['NodeID'] = df['module'].apply(extract_node_id)
    df = df[df['NodeID'] != -1]
    df = df.sort_values('NodeID')

    # Il numero totale di switch è l'ultimo valore registrato nel vettore
    df['TotalSwitches'] = df['vecvalue'].apply(lambda x: x[-1] if len(x) > 0 else 0)

    # Calcolo delle statistiche globali
    node_ids = df['NodeID'].tolist()
    switches = df['TotalSwitches'].tolist()
    
    total_network_switches = sum(switches)
    mean_switches = np.mean(switches)

    print(f"Totale switch nella rete: {int(total_network_switches)}")
    print(f"Media switch per nodo:    {mean_switches:.1f}")

    # Generazione del grafico
    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    # Colorazione semantica: 
    # Verde (0-2 switch: stabile), Arancione (3-10: instabile), Rosso (>10: ping-pong grave)
    colors = [
        GREEN if s <= 2 else ORANGE if s <= 10 else VERMILLION
        for s in switches
    ]

    ax.bar(node_ids, switches, color=colors, edgecolor='black', width=0.8, alpha=0.8)

    # Linea della media globale
    ax.axhline(y=mean_switches, color=BLUE, linestyle='-', linewidth=2, label=f'Mean: {mean_switches:.1f}')

    # Dettagli estetici del grafico
    ax.set_xlabel('Node Index')
    ax.set_ylabel('Number of Switches')
    ax.set_title(
        # f'Switch Count per Node [{config_name}]\n'
        f'Total Switches: {int(total_network_switches)} - {config_name}'
    )
    
    ax.set_xlim(min(node_ids) - 1, max(node_ids) + 1)
    
    # Adattamento dinamico dell'asse Y per evitare grafici schiacciati se i valori sono bassi
    max_y = max(switches) if switches else 10
    ax.set_ylim(0, max_y * 1.15 if max_y > 0 else 5)
    
    ax.legend(loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()

    # Salvataggio dell'immagine
    dir_name = filepath.rsplit(os.sep, 1)[0]
    output_filename = os.path.join(dir_name, f'plot_switches_{config_name}.png')
    plt.savefig(output_filename, dpi=150)
    plt.close()
    
    print(f"[OK] Grafico salvato in: {output_filename}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Uso: python plot_switches.py <path-file.vec>")
        sys.exit(1)

    filepath = sys.argv[1]
    apply_style()
    
    config_name = filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"

    print(f"\nAnalyze file: {filepath}")
    print(f"Configuration:   {config_name}\n")
   
    try:
        analyze_switch_counts(filepath, config_name)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()

    print("\nCompletato!")