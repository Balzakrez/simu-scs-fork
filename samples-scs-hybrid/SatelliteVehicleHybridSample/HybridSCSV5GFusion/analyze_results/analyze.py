#!/usr/bin/env python3
"""
analyze.py - Run all analysis scripts for a single simulation run.

Usage:
    python analyze.py <path/to/results/0.vec>
"""

import sys
import os
import subprocess
import time
from datetime import datetime

SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))

SCRIPTS = [
    "plot_usage.py",
    "plot_switch.py",
    "plot_pdr.py",
    "plot_rtt.py",
    "plot_jitter.py",
    "plot_throughput.py",
    "plot_scores.py",
    "plot_energy_residual.py",
]

# Scripts that do NOT need the .sca file
NO_SCA = {"plot_scores.py", "plot_energy_residual.py"}


def main():
    if len(sys.argv) < 2 or not os.path.exists(sys.argv[1]):
        print("Usage: python analyze.py <path/to/0.vec>")
        sys.exit(1)

    vec_path = os.path.abspath(sys.argv[1])
    output_dir = os.path.dirname(vec_path)
    config_name = os.path.basename(output_dir) or "Simulation"

    # Infer .sca file
    sca_path = vec_path.replace(".vec", ".sca")
    if not os.path.exists(sca_path):
        candidates = [f for f in os.listdir(output_dir) if f.endswith(".sca")]
        sca_path = os.path.join(output_dir, candidates[0]) if candidates else None

    print(f"\n{'='*60}")
    print(f" Analysis Runner — {config_name}")
    print(f" .vec : {vec_path}")
    print(f" .sca : {sca_path or 'NOT FOUND'}")
    print(f"{'='*60}\n")

    results = []

    for script in SCRIPTS:
        script_path = os.path.join(SCRIPTS_DIR, script)
        needs_sca = script not in NO_SCA

        cmd: list[str] = [sys.executable, script_path, vec_path]

        # Build args
        if needs_sca and not sca_path:
            print(f">>> [SKIP] {script} — .sca file not found")
            results.append((script, -2, "", ".sca not found", 0.0, []))
            continue
        elif needs_sca and sca_path:
            cmd.append(sca_path)

        # Snapshot existing PNGs
        before = set(f for f in os.listdir(output_dir) if f.endswith(".png"))

        print(f">>> Running {script}...")
        t0 = time.time()
        proc = subprocess.run(cmd, capture_output=True, text=True)
        elapsed = time.time() - t0

        new_plots = sorted(set(f for f in os.listdir(output_dir) if f.endswith(".png")) - before)

        status = "OK" if proc.returncode == 0 else f"FAILED (rc={proc.returncode})"
        print(f"   {status}  |  {elapsed:.1f}s  |  {len(new_plots)} plot(s)")
        if proc.returncode != 0:
            for line in proc.stderr.strip().splitlines()[:3]:
                print(f"   {line}")

        results.append((script, proc.returncode, proc.stdout, proc.stderr, elapsed, new_plots))

    # --- Text report ---
    report_path = os.path.join(output_dir, f"report_{config_name}.txt")
    passed = sum(1 for _, rc, *_ in results if rc == 0)
    total_plots = sum(len(plots) for *_, plots in results)
    total_time = sum(t for _, _, _, _, t, _ in results)

    with open(report_path, "w", encoding="utf-8") as f:
        W = 72
        f.write("=" * W + "\n")
        f.write(f"  SIMULATION ANALYSIS REPORT — {config_name}\n")
        f.write("=" * W + "\n")
        f.write(f"  Generated : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"  Input     : {os.path.basename(vec_path)}\n")
        if sca_path:
            f.write(f"  Scalars   : {os.path.basename(sca_path)}\n")
        f.write("-" * W + "\n")
        f.write(f"  Scripts   : {passed}/{len(results)} OK    Failed: {len(results) - passed}\n")
        f.write(f"  Plots     : {total_plots} generated\n")
        f.write(f"  Runtime   : {total_time:.1f} s\n")
        f.write("=" * W + "\n")

        for script, rc, stdout, stderr, elapsed, plots in results:
            status = "OK" if rc == 0 else f"FAILED (rc={rc})"
            f.write(f"\n  [{status}]  {script}  ({elapsed:.1f}s)\n")
            f.write("-" * W + "\n")
            if plots:
                f.write("  Plots generated:\n")
                for p in plots:
                    f.write(f"    • {p}\n")
            else:
                f.write("  No plots generated.\n")
            if stdout.strip():
                f.write("\n  --- stdout ---\n")
                for line in stdout.strip().splitlines():
                    f.write(f"  {line}\n")
            if stderr.strip():
                f.write("\n  --- stderr ---\n")
                for line in stderr.strip().splitlines():
                    f.write(f"  {line}\n")

        f.write("\n" + "=" * W + "\n")

    print(f"\n{'='*60}")
    print(f" Done: {passed}/{len(results)} OK, {total_plots} plots")
    print(f" Report: {report_path}")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()