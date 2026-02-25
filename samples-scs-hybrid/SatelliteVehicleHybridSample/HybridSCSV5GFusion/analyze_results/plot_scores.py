#!/usr/bin/env python3
"""
plot_scores.py - Utility & QoS score analysis for EnergyAwareStrategy results.

Produces three plots:
  1. Utility Score over time  — one line per node + bold mean line
                                 with minUtilityScore threshold
  2. QoS Score over time      — one line per node + bold mean line
  3. Utility vs QoS           — mean utility vs mean QoS on the same axes,
                                 to show the contribution of the energy component

Signals (from EnergyAwareStrategy):
  Vectors : utilityScoreSignal, qosScoreSignal   (.vec)

Usage:
    python plot_scores.py <path-to-results.vec>
    python plot_scores.py <path-to-results.vec> --threshold 0.70

Color palette: Wong (2011) colorblind-safe, standard for scientific publications.
"""

import sys
import os
import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
from omnetpp.scave import results
from utils import apply_style, FIG_SINGLE, BLUE, GREEN, ORANGE, VERMILLION, BLACK, SKY_BLUE


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
# Plot 1 – Utility Score over time
# ============================================================================

def plot_utility_score(vecfile, config_name, out_dir, threshold, mean_cache):
    """
    Temporal evolution of the utility score that drives switching decisions.
    The strategy switches interface when the score drops below minUtilityScore.
    """
    print("\n--- Plot 1: Utility Score over time ---")
    results.set_inputs(vecfile)

    df = results.get_vectors("*utilityScore*")
    if df.empty:
        print("  [WARN] No utilityScore vectors found. Skipping.")
        return

    df["NodeID"] = df["module"].apply(extract_node_id)
    df = df[df["NodeID"] != -1]
    print(f"  Found {len(df)} nodes.")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    for _, row in df.iterrows():
        ax.plot(np.array(row["vectime"]), np.array(row["vecvalue"]),
                linewidth=0.6, alpha=0.25, color=BLUE)

    times, mean = compute_mean_line(df)
    if mean is not None and times is not None:
        ax.plot(times, mean, color=BLUE, linewidth=2.0, linestyle="-",
                label=f"Mean utility score ({len(df)} nodes)")
        mean_cache["utility"] = (times, mean)

    ax.axhline(y=threshold, color=VERMILLION, linestyle="--", linewidth=1.5,
               label=f"Threshold $\\tau$ = {threshold}")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Utility Score (0–1)")
    ax.set_ylim(-0.02, 1.08)
    ax.set_title(f"Utility Score — {config_name}")
    ax.legend()
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_utility_score_{config_name}.png"))


# ============================================================================
# Plot 2 – QoS Score over time
# ============================================================================

def plot_qos_score(vecfile, config_name, out_dir, mean_cache):
    """
    Temporal evolution of the QoS component (RTT + PDR + Jitter combined score).
    """
    print("\n--- Plot 2: QoS Score over time ---")
    results.set_inputs(vecfile)

    df = results.get_vectors("*qosScore*")
    if df.empty:
        print("  [WARN] No qosScore vectors found. Skipping.")
        return

    df["NodeID"] = df["module"].apply(extract_node_id)
    df = df[df["NodeID"] != -1]
    print(f"  Found {len(df)} nodes.")

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    for _, row in df.iterrows():
        ax.plot(np.array(row["vectime"]), np.array(row["vecvalue"]),linewidth=0.6, alpha=0.25, color=SKY_BLUE)

    times, mean = compute_mean_line(df)
    if mean is not None and times is not None:
        ax.plot(times, mean, color=BLUE, linewidth=2.0, linestyle="-",label=f"Mean QoS score ({len(df)} nodes)")
        mean_cache["qos"] = (times, mean)

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("QoS Score (0–1)")
    ax.set_ylim(-0.02, 1.08)
    ax.set_title(f"QoS Score — {config_name}")
    ax.legend()
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_qos_score_{config_name}.png"))


# ============================================================================
# Plot 3 – Utility vs QoS on the same axes
# ============================================================================

def plot_score_comparison(config_name, out_dir, threshold, mean_cache):
    """
    Overlays mean utility and mean QoS on the same axes.

    The gap between the two lines shows the net contribution of the energy
    component in the utility function:
      - Utility > QoS  →  energy component helps  (cellular in use)
      - Utility < QoS  →  energy component hurts   (satellite in use)
    """
    print("\n--- Plot 3: Utility vs QoS comparison ---")

    if "utility" not in mean_cache or "qos" not in mean_cache:
        print("  [WARN] Missing data for comparison plot. Skipping.")
        return

    t_u, mean_u = mean_cache["utility"]
    t_q, mean_q = mean_cache["qos"]

    # Align QoS onto utility time grid if grids differ
    if not np.array_equal(t_u, t_q):
        mean_q = np.interp(t_u, t_q, mean_q, left=np.nan, right=np.nan)
    times = t_u

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    colors = [SKY_BLUE, GREEN]  # [Satellite, Cellular]

    # Two distinct lines: solid vs dashed, two colorblind-safe colors
    ax.plot(times, mean_u, color=BLUE,   linewidth=2.0, linestyle="-",
            label="Utility score $U$")
    ax.plot(times, mean_q, color=ORANGE, linewidth=2.0, linestyle="--",
            label="QoS score $Q$")

    # Shade gap: green when energy helps, red-orange when it hurts
    ax.fill_between(times, mean_q, mean_u, where=(mean_u >= mean_q),
                    alpha=0.15, color=colors[1], interpolate=True,
                    label="$U > Q$: energy component helps (cellular)")
    ax.fill_between(times, mean_u, mean_q, where=(mean_u < mean_q),
                    alpha=0.15, color=colors[0], interpolate=True,
                    label="$U < Q$: energy component hurts (satellite)")

    ax.axhline(y=threshold, color=BLACK, linestyle=":", linewidth=1.2,
               label=f"Threshold $\\tau$ = {threshold}")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Score (0–1)")
    ax.set_ylim(-0.02, 1.08)
    ax.set_title(f"Utility vs. QoS Score — {config_name}")
    ax.legend()
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_score_comparison_{config_name}.png"))


# ============================================================================
# Entry point
# ============================================================================

def main():
    apply_style()

    parser = argparse.ArgumentParser()
    parser.add_argument("filepath", help="Path to .vec results file")
    parser.add_argument("--threshold", type=float, default=0.70,
                        help="minUtilityScore threshold (default: 0.70)")
    args = parser.parse_args()

    base        = args.filepath.rsplit(".", 1)[0]
    vecfile     = base + ".vec"
    config_name = args.filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"
    out_dir = args.filepath.rsplit(os.sep, 1)[0]

    print(f"Configuration : {config_name}")
    print(f"Threshold     : {args.threshold}")

    mean_cache = {}

    try:
        plot_utility_score(vecfile, config_name, out_dir, args.threshold, mean_cache)
        plot_qos_score(vecfile, config_name, out_dir, mean_cache)
        plot_score_comparison(config_name, out_dir, args.threshold, mean_cache)
    except Exception as exc:
        import traceback
        print(f"\n[ERR] {exc}")
        traceback.print_exc()
        sys.exit(1)

    print("\nDone!")


if __name__ == "__main__":
    main()