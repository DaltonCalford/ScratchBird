#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import time
from statistics import median


def run_cmd(cmd: list[str]) -> tuple[int, str, str, float]:
    start = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out, err = proc.communicate()
    dur = (time.time() - start) * 1000.0
    return proc.returncode, out, err, dur


def measure_binary(binary: str, args: list[str], runs: int = 3) -> dict:
    timings = []
    for _ in range(runs):
        rc, out, err, dur_ms = run_cmd([binary] + args)
        if rc != 0:
            raise RuntimeError(f"Command failed: {binary} rc={rc} err={err}\nout={out}")
        timings.append(dur_ms)
    return {
        "p50_ms": median(timings),
        "p95_ms": sorted(timings)[max(0, int(0.95 * len(timings)) - 1)],
        "runs": len(timings),
        "all_ms": timings,
    }


def collect_metrics(build_dir: str) -> dict:
    # Minimal metrics from running microbenchmarks
    perf_targets = {
        "heap_insert": os.path.join(build_dir, "perf_heap_insert"),
        "heap_scan": os.path.join(build_dir, "perf_heap_scan"),
        "index_ops": os.path.join(build_dir, "perf_index_ops"),
        "tpch_queries": os.path.join(build_dir, "perf_queries_tpch"),
    }

    metrics = {}
    for name, bin_path in perf_targets.items():
        if not os.path.exists(bin_path):
            # Skip if not built
            continue
        res = measure_binary(bin_path, [])
        metrics[name] = {
            "query_execution_time_p95_ms": res["p95_ms"],
            # Placeholders for future system measurements
            "tps": 0.0,
            "peak_mem_mb": 0.0,
            "disk_io_ops": 0.0,
            "cpu_pct": 0.0,
        }
    return metrics


def compare(baseline: dict, current: dict) -> int:
    exit_code = 0
    for key, cur in current.items():
        base = baseline.get(key)
        if not base:
            continue
        # Gates
        # query_execution_time_p95_ms: +10% => fail
        if base.get("query_execution_time_p95_ms", 0) > 0:
            limit = base["query_execution_time_p95_ms"] * 1.10
            if cur["query_execution_time_p95_ms"] > limit:
                print(f"FAIL {key}: p95 {cur['query_execution_time_p95_ms']:.2f}ms > {limit:.2f}ms (baseline {base['query_execution_time_p95_ms']:.2f})")
                exit_code = 1
        # tps: −5% => warn, −10% => fail
        if base.get("tps", 0) > 0 and cur.get("tps", 0) > 0:
            warn_lim = base["tps"] * 0.95
            fail_lim = base["tps"] * 0.90
            if cur["tps"] < fail_lim:
                print(f"FAIL {key}: tps {cur['tps']:.2f} < {fail_lim:.2f} (baseline {base['tps']:.2f})")
                exit_code = 1
            elif cur["tps"] < warn_lim:
                print(f"WARN {key}: tps {cur['tps']:.2f} < {warn_lim:.2f} (baseline {base['tps']:.2f})")
        # peak_mem_mb: +20% => fail
        if base.get("peak_mem_mb", 0) > 0 and cur.get("peak_mem_mb", 0) > 0:
            limit = base["peak_mem_mb"] * 1.20
            if cur["peak_mem_mb"] > limit:
                print(f"FAIL {key}: peak_mem {cur['peak_mem_mb']:.2f}MB > {limit:.2f}MB (baseline {base['peak_mem_mb']:.2f})")
                exit_code = 1
    return exit_code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=os.path.join(os.getcwd(), "../../build"))
    ap.add_argument("--baseline", default=os.path.join(os.getcwd(), "../../resource/perf_baselines/perf_baselines.json"))
    ap.add_argument("--out", default="perf_current.json")
    args = ap.parse_args()

    metrics = collect_metrics(args.build)
    with open(args.out, "w") as f:
        json.dump(metrics, f, indent=2)

    # Compare to baseline if present
    if os.path.exists(args.baseline):
        with open(args.baseline) as f:
            baseline = json.load(f)
        rc = compare(baseline, metrics)
        sys.exit(rc)
    else:
        print("No baseline found; skipping comparison")
        sys.exit(0)


if __name__ == "__main__":
    main()

