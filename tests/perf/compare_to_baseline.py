#!/usr/bin/env python3
import argparse
import json
import sys


def compare(baseline_path: str, current_path: str) -> int:
    with open(baseline_path) as f:
        baseline = json.load(f)
    with open(current_path) as f:
        current = json.load(f)

    exit_code = 0
    for key, cur in current.items():
        base = baseline.get(key)
        if not base:
            continue
        # p95 +10% fail
        if base.get("query_execution_time_p95_ms", 0) > 0:
            limit = base["query_execution_time_p95_ms"] * 1.10
            if cur["query_execution_time_p95_ms"] > limit:
                print(f"FAIL {key}: p95 {cur['query_execution_time_p95_ms']:.2f} > {limit:.2f} (base {base['query_execution_time_p95_ms']:.2f})")
                exit_code = 1
        # tps warn/fail
        if base.get("tps", 0) > 0 and cur.get("tps", 0) > 0:
            warn_lim = base["tps"] * 0.95
            fail_lim = base["tps"] * 0.90
            if cur["tps"] < fail_lim:
                print(f"FAIL {key}: tps {cur['tps']:.2f} < {fail_lim:.2f}")
                exit_code = 1
            elif cur["tps"] < warn_lim:
                print(f"WARN {key}: tps {cur['tps']:.2f} < {warn_lim:.2f}")
        # peak_mem +20% fail
        if base.get("peak_mem_mb", 0) > 0 and cur.get("peak_mem_mb", 0) > 0:
            limit = base["peak_mem_mb"] * 1.20
            if cur["peak_mem_mb"] > limit:
                print(f"FAIL {key}: peak_mem {cur['peak_mem_mb']:.2f} > {limit:.2f}")
                exit_code = 1
    return exit_code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--current", required=True)
    args = ap.parse_args()
    sys.exit(compare(args.baseline, args.current))


if __name__ == "__main__":
    main()

