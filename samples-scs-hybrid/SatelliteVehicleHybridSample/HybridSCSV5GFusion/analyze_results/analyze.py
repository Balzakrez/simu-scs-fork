#!/usr/bin/env python3
"""
analyze.py - Run all analysis scripts for a single simulation run.

Calls all analysis scripts, collects their outputs (plots + stdout),
and generates a single HTML report with all results.

Usage:
    python analyze.py <path/to/results/0.vec>

    The .sca file is inferred from the same directory as the .vec file.
    All scripts must be in the same directory as this script.
"""

import sys
import os
import subprocess
import time
from datetime import datetime


# ============================================================================
# Configuration — adjust paths if scripts live elsewhere
# ============================================================================

SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))

# (script_name, requires_vec, requires_sca)
SCRIPTS = [
    ("plot_energy_residual.py", True, False),
    ("plot_scores.py", True, False),
    ("plot_jitter.py", True, False),
    ("plot_pdr.py", True, True),
    ("plot_qos.py", True, False),
    ("plot_usage.py", True, True),
    ("plot_rtt.py", True, True),
    ("plot_switch.py", True, True),
    ("plot_throughput.py", True, True),
]


# ============================================================================
# Helpers
# ============================================================================

def infer_sca(vec_path):
    base = os.path.splitext(vec_path)[0]
    sca = base + ".sca"
    if os.path.exists(sca):
        return sca
    # Fallback: any .sca in the same directory
    d = os.path.dirname(vec_path)
    candidates = [f for f in os.listdir(d) if f.endswith('.sca')]
    if candidates:
        return os.path.join(d, candidates[0])
    return None


def run_script(script_name, args):
    """
    Runs a script as a subprocess and returns (returncode, stdout, stderr, duration_s).
    """
    script_path = os.path.join(SCRIPTS_DIR, script_name)
    if not os.path.exists(script_path):
        return -1, "", f"Script not found: {script_path}", 0.0

    cmd = [sys.executable, script_path] + args
    t0 = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - t0
    return proc.returncode, proc.stdout, proc.stderr, elapsed


def collect_new_plots(output_dir, before_files):
    """Returns list of image files created after before_files snapshot."""
    current = set(
        f for f in os.listdir(output_dir)
        if f.lower().endswith(('.png', '.jpg', '.jpeg'))
    )
    return sorted(current - before_files)


# ============================================================================
# HTML report generation
# ============================================================================

def build_text_report(config_name, results, output_path, vec_path, sca_path):
    """
    Builds a plain-text report of the analysis run.
    results: list of dicts {script, args, returncode, stdout, stderr, duration, plots}
    """
    total  = len(results)
    passed = sum(1 for r in results if r['returncode'] == 0)
    failed = total - passed
    total_plots   = sum(len(r['plots']) for r in results)
    total_runtime = sum(r['duration'] for r in results)
    now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    W = 72   # line width

    lines = []
    lines.append('=' * W)
    lines.append(f"  SIMULATION ANALYSIS REPORT — {config_name}")
    lines.append('=' * W)
    lines.append(f"  Generated : {now}")
    lines.append(f"  Input     : {os.path.basename(vec_path)}")
    if sca_path:
        lines.append(f"  Scalars   : {os.path.basename(sca_path)}")
    lines.append('-' * W)
    lines.append(f"  Scripts   : {passed}/{total} OK    Failed: {failed}")
    lines.append(f"  Plots     : {total_plots} generated")
    lines.append(f"  Runtime   : {total_runtime:.1f} s")
    lines.append('=' * W)

    for r in results:
        rc       = r['returncode']
        status   = 'OK' if rc == 0 else f'FAILED (rc={rc})'
        lines.append('')
        lines.append(f"  [{status}]  {r['script']}  ({r['duration']:.1f}s)")
        lines.append('-' * W)

        if r['plots']:
            lines.append("  Plots generated:")
            for p in r['plots']:
                lines.append(f"    • {os.path.basename(p)}")
        else:
            lines.append("  No plots generated.")

        if r['stdout'].strip():
            lines.append("")
            lines.append("  --- stdout ---")
            for line in r['stdout'].strip().splitlines():
                lines.append(f"  {line}")

        if r['stderr'].strip():
            lines.append("")
            lines.append("  --- stderr ---")
            for line in r['stderr'].strip().splitlines():
                lines.append(f"  {line}")

    lines.append('')
    lines.append('=' * W)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')


# ============================================================================
# Main
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Usage: python run_analysis.py <path/to/0.vec>")
        sys.exit(1)

    vec_path = sys.argv[1]
    if not os.path.exists(vec_path):
        print(f"[ERR] File not found: {vec_path}")
        sys.exit(1)

    sca_path = infer_sca(vec_path)
    output_dir = os.path.dirname(os.path.abspath(vec_path))

    config_name = vec_path.split(os.sep)[-2]
    if config_name in (".", ".."):
        config_name = "Simulation"

    print(f"\n{'='*60}")
    print(f" Analysis Runner — {config_name}")
    print(f"{'='*60}")
    print(f" .vec : {vec_path}")
    print(f" .sca : {sca_path or 'NOT FOUND'}")
    print(f"{'='*60}\n")

    all_results = []

    for script_name, needs_vec, needs_sca in SCRIPTS:
        print(f"▶  Running {script_name}...")

        args = []
        if needs_vec:
            args.append(vec_path)
        if needs_sca:
            if sca_path:
                args.append(sca_path)
            else:
                print(f"   [SKIP] {script_name} requires .sca but none found.")
                all_results.append({
                    'script': script_name, 'args': args,
                    'returncode': -2, 'stdout': '', 'stderr': '.sca file not found',
                    'duration': 0.0, 'plots': []
                })
                continue
            
        # Snapshot existing plots before running
        before = set(f for f in os.listdir(output_dir) if f.lower().endswith('.png'))

        rc, stdout, stderr, elapsed = run_script(script_name, args)

        # Collect new plots
        new_plot_names = collect_new_plots(output_dir, before)
        new_plot_paths = [os.path.join(output_dir, p) for p in new_plot_names]

        status = "OK" if rc == 0 else f"FAILED (rc={rc})"
        print(f"   {status}  |  {elapsed:.1f}s  |  {len(new_plot_paths)} plot(s)")
        if rc != 0 and stderr:
            # Print first 3 lines of stderr for quick diagnosis
            for line in stderr.strip().splitlines()[:3]:
                print(f"   {line}")

        all_results.append({
            'script':     script_name,
            'args':       args,
            'returncode': rc,
            'stdout':     stdout,
            'stderr':     stderr,
            'duration':   elapsed,
            'plots':      new_plot_paths,
        })

    # Generate text report
    report_path = os.path.join(output_dir, f"report_{config_name}.txt")
    print(f"\nGenerating report: {report_path}")
    build_text_report(config_name, all_results, report_path, vec_path, sca_path or "")

    # Summary
    passed = sum(1 for r in all_results if r['returncode'] == 0)
    total_plots = sum(len(r['plots']) for r in all_results)
    print(f"\n{'='*60}")
    print(f" Done: {passed}/{len(all_results)} scripts OK, {total_plots} plots, report saved.")
    print(f" Open: {report_path}")
    print(f"{'='*60}\n")


if __name__ == '__main__':
    main()