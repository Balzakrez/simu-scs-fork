#!/usr/bin/env python3
"""
plot_energy_residual.py - Energy analysis for EnergyAwareStrategy simulation results.

Produces three plots:
  1. Residual Energy over time     — one line per node + bold mean line
  2. Energy Efficiency over time   — one line per node + bold mean line
                                     (1.0 = cellular, 0.2 = satellite)
  3. Energy Consumption Breakdown  — bar chart: satellite vs cellular (scalars)

Signals (from EnergyAwareStrategy):
  Vectors : residualEnergySignal, energyEfficiencySignal   (.vec)
  Scalars : satelliteEnergyConsumed, cellularEnergyConsumed,
            satelliteTotalBytes, cellularTotalBytes         (.sca)

Usage:
    python plot_energy_residual.py <path-to-results.vec>
    python plot_energy_residual.py <path-to-results.vec> --critical 30

Color palette: Wong (2011) colorblind-safe, standard for scientific publications.
"""

import sys
import os
import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, FIG_DOUBLE, BLUE, ORANGE, GREEN, SKY_BLUE, VERMILLION, BLACK

# ============================================================================
# Utility helpers
# ============================================================================

def extract_node_id(module_str):
    m = re.search(r"node\[(\d+)\]", module_str)
    return int(m.group(1)) if m else -1


def save_fig(fig, out_path):
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  [OK] Saved: {out_path}")


def compute_mean_line(df):
    """Interpolates all node vectors onto a common time grid and returns (times, mean)."""
    all_times = np.sort(np.unique(np.concatenate(df["vectime"].values)))
    matrix = []
    for _, row in df.iterrows():
        t = np.array(row["vectime"])
        v = np.array(row["vecvalue"])
        if len(t) > 0:
            matrix.append(np.interp(all_times, t, v, left=np.nan, right=np.nan))
    if not matrix:
        return None, None
    M = np.array(matrix)
    with np.errstate(invalid="ignore"):
        return all_times, np.nanmean(M, axis=0)


# ============================================================================
# Plot 1 – Residual Energy over time
# ============================================================================

def plot_residual_energy(vecfile, config_name, out_dir, critical_threshold):
    print("\n--- Plot 1: Residual Energy over time ---")
    results.set_inputs(vecfile)

    # df = results.get_vectors("module=~*interfaceManager* AND name=~*residualEnergy*")
    df = results.get_vectors("module=~*energyStorage* AND name=~*residualEnergy*")
    if df.empty:
        print("  [WARN] No residualEnergy vectors found. Skipping.")
        return

    df["NodeID"] = df["module"].apply(extract_node_id)
    df = df[df["NodeID"] != -1]
    print(f"  Found {len(df)} nodes.")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    # One thin line per node (same color, very transparent)
    for _, row in df.iterrows():
        ax.plot(np.array(row["vectime"]), np.array(row["vecvalue"]),
                linewidth=0.6, alpha=0.25, color=BLUE)

    # Bold mean line
    times, mean = compute_mean_line(df)
    if mean is not None and times is not None:
        ax.plot(times, mean, color=BLUE, linewidth=2.0, linestyle="-",
                label=f"Mean residual energy ({len(df)} nodes)")

    if critical_threshold > 0:
        ax.axhline(y=critical_threshold, color=VERMILLION, linestyle="--",
                   linewidth=1.5, label=f"Critical threshold ({critical_threshold} J)")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Residual Energy (J)")
    ax.set_title(f"Residual Energy — {config_name}")
    ax.legend()
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_energy_residual_{config_name}.png"))


# ============================================================================
# Plot 2 – Energy Efficiency over time
# ============================================================================

def plot_energy_efficiency(vecfile, config_name, out_dir):
    """
    Energy efficiency of the active interface over time.
    Score: 1.0 = cellular (cheap), 0.2 = satellite (5× more expensive).
    """
    print("\n--- Plot 2: Energy Efficiency over time ---")
    results.set_inputs(vecfile)

    df = results.get_vectors("*energyEfficiency*")
    if df.empty:
        print("  [WARN] No energyEfficiency vectors found. Skipping.")
        return

    df["NodeID"] = df["module"].apply(extract_node_id)
    df = df[df["NodeID"] != -1]
    print(f"  Found {len(df)} nodes.")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    for _, row in df.iterrows():
        ax.plot(np.array(row["vectime"]), np.array(row["vecvalue"]),
                linewidth=0.6, alpha=0.25, color=ORANGE)

    times, mean = compute_mean_line(df)
    if mean is not None and times is not None:
        ax.plot(times, mean, color=ORANGE, linewidth=2.0, linestyle="-",
                label=f"Mean energy efficiency ({len(df)} nodes)")

    ax.axhline(y=1.0, color=GREEN,  linestyle=":",  linewidth=1.4,label="Cellular  (score = 1.0)")
    ax.axhline(y=0.2, color=SKY_BLUE,   linestyle="-.", linewidth=1.4,label="Satellite  (score = 0.2)")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Energy Efficiency (0–1)")
    ax.set_ylim(-0.05, 1.15)
    ax.set_title(f"Energy Efficiency of Active Interface — {config_name}")
    ax.legend()
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_energy_efficiency_{config_name}.png"))


# ============================================================================
# Plot 3 – Final Energy Consumption Breakdown (scalars)
# ============================================================================

def plot_energy_breakdown(scafile, config_name, out_dir):
    print("\n--- Plot 3: Energy Consumption Breakdown ---")
    results.set_inputs(scafile)

    SCALARS = [
        "satelliteEnergyConsumed",
        "cellularEnergyConsumed",
        "satelliteTotalBytes",
        "cellularTotalBytes",
    ]
    dfs = {}
    for s in SCALARS:
        df_s = results.get_scalars(f"*{s}*")
        if not df_s.empty:
            dfs[s] = df_s
        else:
            print(f"  [WARN] Scalar '{s}' not found.")

    if "satelliteEnergyConsumed" not in dfs or "cellularEnergyConsumed" not in dfs:
        print("  [WARN] Missing energy scalars. Skipping.")
        return

    sat_energy   = dfs["satelliteEnergyConsumed"]["value"].sum()
    cell_energy  = dfs["cellularEnergyConsumed"]["value"].sum()
    total_energy = sat_energy + cell_energy
    sat_bytes    = dfs["satelliteTotalBytes"]["value"].sum()  if "satelliteTotalBytes"  in dfs else 0
    cell_bytes   = dfs["cellularTotalBytes"]["value"].sum()   if "cellularTotalBytes"   in dfs else 0

    print(f"  Satellite : {sat_energy:.4f} J  ({sat_bytes/1e6:.2f} MB)")
    print(f"  Cellular  : {cell_energy:.4f} J  ({cell_bytes/1e6:.2f} MB)")
    print(f"  Total     : {total_energy:.4f} J")

    labels   = ["Satellite", "Cellular"]
    colors = [SKY_BLUE, GREEN]  # [Satellite, Cellular]
    energies = [sat_energy, cell_energy]
    bytes_mb = [sat_bytes / 1e6, cell_bytes / 1e6]

    fig, (ax_e, ax_b) = plt.subplots(1, 2, figsize=FIG_DOUBLE)

    # Energy bars
    bars = ax_e.bar(labels, energies, color=colors, edgecolor=BLACK,
                    linewidth=0.6, width=0.40)
    for bar, val in zip(bars, energies):
        pct = f"\n({val/total_energy*100:.1f}%)" if total_energy > 0 else ""
        ax_e.text(bar.get_x() + bar.get_width() / 2,
                  val + max(energies) * 0.02,
                  f"{val:.4f} J{pct}",
                  ha="center", va="bottom", fontsize=9)
    ax_e.set_ylabel("Estimated Energy (J)")
    ax_e.set_title("Energy Cost by Interface")
    ax_e.set_ylim(0, max(energies) * 1.35 + 1e-9)

    # Bytes bars
    total_mb = sum(bytes_mb)
    bars2 = ax_b.bar(labels, bytes_mb, color=colors, edgecolor=BLACK,
                     linewidth=0.6, width=0.40)
    for bar, val in zip(bars2, bytes_mb):
        pct = f"\n({val/total_mb*100:.1f}%)" if total_mb > 0 else ""
        ax_b.text(bar.get_x() + bar.get_width() / 2,
                  val + max(bytes_mb) * 0.02,
                  f"{val:.2f} MB{pct}",
                  ha="center", va="bottom")
    ax_b.set_ylabel("Data Transferred (MB)")
    ax_b.set_title("Data Volume by Interface")
    ax_b.set_ylim(0, max(bytes_mb) * 1.35 + 1e-9)

    fig.suptitle(f"Energy Consumption Breakdown — {config_name}", fontweight="bold")
    plt.tight_layout()
    save_fig(fig, os.path.join(out_dir, f"plot_energy_breakdown_{config_name}.png"))


# ============================================================================
# Entry point
# ============================================================================

def main():
    apply_style()

    parser = argparse.ArgumentParser()
    parser.add_argument("filepath", help="Path to .vec or .sca results file")
    parser.add_argument("--critical", type=float, default=30.0,
                        help="Critical energy threshold in Joules (default: 30)")
    args = parser.parse_args()

    base        = args.filepath.rsplit(".", 1)[0]
    vecfile     = base + ".vec"
    scafile     = base + ".sca"
    config_name = args.filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"
    out_dir = args.filepath.rsplit(os.sep, 1)[0]

    print(f"Configuration : {config_name}")

    try:
        plot_residual_energy(vecfile, config_name, out_dir, args.critical)
        plot_energy_efficiency(vecfile, config_name, out_dir)
        plot_energy_breakdown(scafile, config_name, out_dir)
    except Exception as exc:
        import traceback
        print(f"\n[ERR] {exc}")
        traceback.print_exc()
        sys.exit(1)

    print("\nDone!")


if __name__ == "__main__":
    main()