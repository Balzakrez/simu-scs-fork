#!/usr/bin/env python3
"""
plot_all_scores.py - Unified QoS and Utility score analysis.

Produces up to four plots depending on available signals:
  1. QoS Score over time       — IQR band + mean line + threshold
                                  (available for QoSBasedStrategy and EnergyAwareStrategy)
  2. Utility Score over time   — IQR band + mean line + threshold
                                  (available for EnergyAwareStrategy only)
  3. Utility vs QoS comparison — mean utility vs mean QoS on same axes,
                                  shaded gap shows energy component contribution
  4. Per-node mean QoS bar     — bar chart of per-node mean QoS score

Signals read from .vec file:
  qosScoreSignal     — emitted by QoSBasedStrategy and EnergyAwareStrategy
  utilityScoreSignal — emitted by EnergyAwareStrategy only
  lastActiveInterface — for switch event counting

Statistics printed to stdout:
  - Global mean QoS Score (per-node average)
  - Global mean Utility Score (per-node average, if available)
  - Total switch counts (→ Satellite, → Cellular)

Usage:
    python plot_all_scores.py <results.vec>
    python plot_all_scores.py <results.vec> --qos-threshold 0.70 --utility-threshold 0.65

Color palette: Wong (2011) colorblind-safe.
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


def load_vectors(signal_filter):
    """Load vectors matching filter, add NodeID, drop unmatched rows."""
    df = results.get_vectors(signal_filter)
    if df.empty:
        return df
    df["NodeID"] = df["module"].apply(extract_node_id)
    return df[df["NodeID"] != -1].copy()


def compute_aggregates(df):
    """
    Interpolate all node vectors onto a common time grid.
    Returns (times, mean, p25, p75) or (None, None, None, None) if no data.
    """
    all_times = np.sort(np.unique(np.concatenate(df["vectime"].values)))
    matrix = []
    for _, row in df.iterrows():
        t = np.array(row["vectime"])
        v = np.array(row["vecvalue"])
        if len(t) > 0:
            matrix.append(np.interp(all_times, t, v, left=np.nan, right=np.nan))
    if not matrix:
        return None, None, None, None
    M = np.array(matrix)
    with np.errstate(invalid="ignore"):
        return (
            all_times,
            np.nanmean(M, axis=0),
            np.nanpercentile(M, 25, axis=0),
            np.nanpercentile(M, 75, axis=0),
        )


def per_node_mean(df):
    """Compute per-node mean and return (list_of_means, global_mean)."""
    means = [float(np.nanmean(np.array(row["vecvalue"]))) for _, row in df.iterrows()]
    return means, float(np.mean(means))


def count_switch_events(vecfile):
    """Count total vertical handovers from lastActiveInterface vector."""
    df = load_vectors("*lastActiveInterface*")
    to_sat, to_cell = 0, 0
    if df.empty:
        return to_sat, to_cell
    for _, row in df.iterrows():
        vals = np.array(row["vecvalue"])
        for i in range(1, len(vals)):
            if vals[i] != vals[i - 1]:
                if vals[i] == 1:
                    to_sat += 1
                else:
                    to_cell += 1
    return to_sat, to_cell


# ============================================================================
# Shared time-series plot helper
# ============================================================================

def _plot_score_timeseries(df, config_name, out_dir, threshold,
                           color, iqr_color, label, ylabel, title,
                           filename_prefix, cache_key, cache,
                           with_iqr=True):
    """
    Generic time-series plot for any score signal.
    Generates two files: one with IQR band, one without (clean mean only).
    """
    times, mean, p25, p75 = compute_aggregates(df)
    if times is None or mean is None or p25 is None or p75 is None:
        print("  [WARN] No data to plot.")
        return

    cache[cache_key] = (times, mean)
    _, global_mean = per_node_mean(df)
    print(f"  Global Mean {label} (per-node): {global_mean:.4f}")

    for show_iqr in (True, False):
        fig, ax = plt.subplots(figsize=FIG_SINGLE)

        if show_iqr:
            ax.fill_between(times, p25, p75, alpha=0.25, color=iqr_color,
                            label="IQR (25–75th)")

        ax.plot(times, mean, color=color, linewidth=2.0, label=f"Mean QoS score ({len(df)} nodes)")
        ax.axhline(y=threshold, color=VERMILLION, linestyle="--", linewidth=1.5,
                   label=f"Threshold = {threshold}")

        ax.set_xlabel("Time (s)")
        ax.set_ylabel(ylabel)
        ax.set_ylim(-0.02, 1.08)
        ax.set_title(f"{title} - {config_name}")
        ax.legend(loc="lower right")
        ax.grid(True, alpha=0.3, linestyle="--")
        plt.tight_layout()

        suffix = "iqr" if show_iqr else "clean"
        save_fig(fig, os.path.join(out_dir, f"{filename_prefix}_{suffix}_{config_name}.png"))


# ============================================================================
# Plot 1 — QoS Score over time
# ============================================================================

def plot_qos_score(df_qos, config_name, out_dir, threshold, cache):
    """
    Mean line + IQR band (and clean variant) for qosScoreSignal.
    Available for both QoSBasedStrategy and EnergyAwareStrategy.
    """
    print("\n--- Plot 1: QoS Score over time ---")
    print(f"  Found {len(df_qos)} nodes.")

    _plot_score_timeseries(
        df=df_qos, config_name=config_name, out_dir=out_dir,
        threshold=threshold,
        color=BLUE, iqr_color=SKY_BLUE,
        label="QoS Score",
        ylabel="QoS Score (0–1)",
        title="QoS Score",
        filename_prefix="plot_qos_score",
        cache_key="qos", cache=cache,
    )


# ============================================================================
# Plot 2 — Utility Score over time  (EnergyAwareStrategy only)
# ============================================================================

def plot_utility_score(df_util, config_name, out_dir, threshold, cache):
    """
    Mean line + IQR band (and clean variant) for utilityScoreSignal.
    """
    print("\n--- Plot 2: Utility Score over time ---")
    print(f"  Found {len(df_util)} nodes.")

    _plot_score_timeseries(
        df=df_util, config_name=config_name, out_dir=out_dir,
        threshold=threshold,
        color=ORANGE, iqr_color=ORANGE,
        label="Utility Score",
        ylabel="Utility Score (0–1)",
        title=f"Utility Score - {config_name}",
        filename_prefix="plot_utility_score",
        cache_key="utility", cache=cache,
    )


# ============================================================================
# Plot 3 — Utility vs QoS comparison  (EnergyAwareStrategy only)
# ============================================================================

def plot_score_comparison(config_name, out_dir, threshold, cache):
    """
    Overlays mean utility and mean QoS on the same axes.
    The shaded gap shows the net contribution of the energy component:
      U > Q  →  energy component helps (cellular in use, cheap)
      U < Q  →  energy component hurts (satellite in use, expensive)
    """
    print("\n--- Plot 3: Utility vs QoS comparison ---")

    if "utility" not in cache or "qos" not in cache:
        print("  [WARN] Missing utility or QoS data. Skipping.")
        return

    t_u, mean_u = cache["utility"]
    t_q, mean_q = cache["qos"]

    if t_u is None or mean_u is None or t_q is None or mean_q is None:
        print("  [WARN] Missing data for comparison plot. Skipping.")
        return

    # Align onto utility time grid if grids differ
    if not np.array_equal(t_u, t_q):
        mean_q = np.interp(t_u, t_q, mean_q, left=np.nan, right=np.nan)
    times = t_u

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.plot(times, mean_u, color=ORANGE, linewidth=2.0, linestyle="-", label="Utility score $U$")
    ax.plot(times, mean_q, color=BLUE, linewidth=2.0, linestyle="--", label="QoS score $Q$")

    ax.fill_between(times, mean_q, mean_u, where=(mean_u >= mean_q), 
                    alpha=0.15, color=GREEN, interpolate=True,
                    label="$U > Q$: energy helps (cellular)")
                    
    ax.fill_between(times, mean_u, mean_q, where=(mean_u < mean_q),
                    alpha=0.15, color=SKY_BLUE, interpolate=True,
                    label="$U < Q$: energy hurts (satellite)")

    ax.axhline(y=threshold, color=BLACK, linestyle=":", linewidth=1.2,
               label=f"Threshold = {threshold}")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Score (0–1)")
    ax.set_ylim(-0.02, 1.08)
    ax.set_title(f"Utility vs. QoS Score - {config_name}")
    ax.legend()
    ax.grid(True, alpha=0.3, linestyle="--")
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_score_comparison_{config_name}.png"))


# ============================================================================
# Plot 4 — Per-node QoS Score over time (one line per node)
# ============================================================================

def plot_qos_per_node_timeseries(df_qos, config_name, out_dir, threshold, cache):
    """
    One thin line per node + bold mean line for qosScoreSignal.
    Useful to spot individual nodes with persistently low QoS.
    """
    print("\n--- Plot 4: Per-node QoS Score over time ---")

    times, mean, _, _ = compute_aggregates(df_qos)
    if times is None or mean is None:
        print("  [WARN] No data to plot.")
        return

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    for _, row in df_qos.iterrows():
        ax.plot(np.array(row["vectime"]), np.array(row["vecvalue"]), linewidth=0.6, alpha=0.20, color=SKY_BLUE)

    ax.plot(times, mean, color=BLUE, linewidth=2.0, label=f"Mean QoS score ({len(df_qos)} nodes)")
    ax.axhline(y=threshold, color=VERMILLION, linestyle="--", linewidth=1.5, label=f"Threshold = {threshold}")

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("QoS Score (0–1)")
    ax.set_ylim(-0.02, 1.08)
    ax.set_title(f"QoS Score - {config_name}")
    ax.legend(loc="lower right")
    ax.grid(True, alpha=0.3, linestyle="--")
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_qos_per_node_time_{config_name}.png"))


# ============================================================================
# Plot 5 — Per-node mean QoS bar chart
# ============================================================================

def plot_qos_per_node_bar(df_qos, config_name, out_dir, threshold):
    """
    Bar chart of per-node mean QoS score with global mean line.
    Bars are colored by quality: green (>= threshold), orange (>= 0.5), red (< 0.5).
    """
    print("\n--- Plot 4: Per-node mean QoS bar chart ---")

    node_means, global_mean = per_node_mean(df_qos)
    node_ids = df_qos["NodeID"].tolist()

    # Sort by node ID
    pairs = sorted(zip(node_ids, node_means))
    node_ids, node_means = zip(*pairs)

    colors = [
        GREEN if m >= threshold else ORANGE if m >= 0.5 else VERMILLION
        for m in node_means
    ]

    fig, ax = plt.subplots(figsize=FIG_SINGLE)

    ax.bar(node_ids, node_means, color=colors, edgecolor="black", width=0.8, alpha=0.8)
    ax.axhline(y=global_mean, color=BLUE, linestyle="-", linewidth=2,
               label=f"Global mean: {global_mean:.3f}")
    ax.axhline(y=threshold, color=VERMILLION, linestyle="--", linewidth=1.5,
               label=f"Threshold = {threshold}")

    ax.set_xlabel("Node Index")
    ax.set_ylabel("Mean QoS Score")
    ax.set_ylim(0, 1.1)
    ax.set_xlim(min(node_ids) - 1, max(node_ids) + 1)
    ax.set_title(f"Mean QoS Score per Node: {global_mean:.3f} - {config_name}")
    ax.legend(loc="lower right")
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    plt.tight_layout()

    save_fig(fig, os.path.join(out_dir, f"plot_qos_per_node_bar_{config_name}.png"))


# ============================================================================
# Entry point
# ============================================================================

def main():
    apply_style()

    parser = argparse.ArgumentParser(
        description="Unified QoS and Utility score analysis for hybrid TN-NTN simulations."
    )
    parser.add_argument("filepath", help="Path to .vec results file")
    parser.add_argument("--qos-threshold", type=float, default=0.70,
                        help="minQosScore threshold line (default: 0.70)")
    parser.add_argument("--utility-threshold", type=float, default=0.70,
                        help="minUtilityScore threshold line (default: 0.70)")
    args = parser.parse_args()

    base        = args.filepath.rsplit(".", 1)[0]
    vecfile     = base + ".vec"
    config_name = args.filepath.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"
    out_dir = args.filepath.rsplit(os.sep, 1)[0]

    print(f"Configuration     : {config_name}")
    print(f"QoS threshold     : {args.qos_threshold}")
    print(f"Utility threshold : {args.utility_threshold}")

    results.set_inputs(vecfile)

    # Load signals
    df_qos  = load_vectors("*qosScore*")
    df_util = load_vectors("*utilityScore*")

    if df_qos.empty:
        print("\n[ERR] No qosScore vectors found. Check that the strategy emits qosScoreSignal.")
        sys.exit(1)

    # Count switches
    to_sat, to_cell = count_switch_events(vecfile)
    print(f"\nTotal switches: {to_sat} → Satellite, {to_cell} → Cellular")

    cache = {}

    try:
        # Plot 1: QoS over time with IQR — always
        plot_qos_score(df_qos, config_name, out_dir, args.qos_threshold, cache)

        # Plot 4: Per-node QoS timeseries — always
        plot_qos_per_node_timeseries(df_qos, config_name, out_dir, args.qos_threshold, cache)

        # Plot 5: Per-node QoS bar chart — always
        plot_qos_per_node_bar(df_qos, config_name, out_dir, args.qos_threshold)

        # Plots 2 & 3: only if utilityScore is available (EnergyAwareStrategy)
        if not df_util.empty:
            plot_utility_score(df_util, config_name, out_dir, args.utility_threshold, cache)
            plot_score_comparison(config_name, out_dir, args.utility_threshold, cache)
        else:
            print("\n[INFO] No utilityScore vectors found — skipping Plots 2 & 3.")
            print("[INFO] These plots are only available for EnergyAwareStrategy.")

    except Exception as exc:
        import traceback
        print(f"\n[ERR] {exc}")
        traceback.print_exc()
        sys.exit(1)

    print("\nDone!")


if __name__ == "__main__":
    main()