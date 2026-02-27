#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import os
import statistics
import time
from pathlib import Path
from typing import Dict, List, Mapping, Tuple

from common_io import ensure_dir, load_structured, write_csv, write_json
from db_adapters import resolve_client_binary, resolve_sql_path, run_sql_file


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Performance/stress harness.")
    p.add_argument("--engines", required=True, help="engines.yaml/json")
    p.add_argument("--config", required=True, help="perf_workloads.yaml/json")
    p.add_argument("--out-dir", required=True, help="output directory")
    p.add_argument(
        "--workspace-root",
        default=None,
        help="Verification workspace root (default: parent of scripts dir)",
    )
    p.add_argument(
        "--repo-root",
        default=None,
        help="Repository clone root. Defaults to SB_VERIFY_REPO_ROOT or <workspace-root>/repos",
    )
    return p.parse_args()


def resolve_paths(args: argparse.Namespace) -> Tuple[Path, Path]:
    workspace_root = (
        Path(args.workspace_root).resolve()
        if args.workspace_root
        else Path(__file__).resolve().parents[1]
    )
    repo_root = (
        Path(args.repo_root).resolve()
        if args.repo_root
        else Path(os.environ.get("SB_VERIFY_REPO_ROOT", str(workspace_root / "repos"))).resolve()
    )
    return workspace_root, repo_root


def lane_for_engine(engine: Mapping[str, object]) -> str:
    if engine.get("lane"):
        return str(engine["lane"])
    if engine.get("emulates"):
        return str(engine["emulates"])
    if engine.get("engine"):
        return str(engine["engine"])
    return str(engine.get("mode", "unknown"))


def percentile(values: List[int], p: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = int(round((len(ordered) - 1) * p))
    return float(ordered[idx])


def run_workers(
    engine: Mapping[str, object],
    binary: str,
    run_sql: Path,
    run_dir: Path,
    timeout_seconds: int,
    concurrency: int,
) -> Tuple[List[Mapping[str, object]], int]:
    ensure_dir(run_dir)
    results: List[Mapping[str, object]] = []

    def worker(worker_id: int) -> Mapping[str, object]:
        res = run_sql_file(
            engine=engine,
            binary=binary,
            sql_file=run_sql,
            output_dir=run_dir,
            stage=f"run_w{worker_id}",
            timeout_seconds=timeout_seconds,
        )
        d = res.__dict__
        d["worker_id"] = worker_id
        return d

    start = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as pool:
        futs = [pool.submit(worker, i) for i in range(concurrency)]
        for fut in concurrent.futures.as_completed(futs):
            results.append(fut.result())
    wall_ms = int((time.time() - start) * 1000)
    return results, wall_ms


def evaluate_thresholds(metrics: Mapping[str, float], thresholds: Mapping[str, float]) -> Tuple[bool, str]:
    min_tps = float(thresholds.get("min_tps", 0.0))
    max_p95 = float(thresholds.get("max_p95_ms", 1e15))
    max_error_rate = float(thresholds.get("max_error_rate", 1.0))

    reasons: List[str] = []
    if metrics["throughput_tps"] < min_tps:
        reasons.append(f"throughput_tps {metrics['throughput_tps']:.3f} < {min_tps:.3f}")
    if metrics["latency_p95_ms"] > max_p95:
        reasons.append(f"latency_p95_ms {metrics['latency_p95_ms']:.3f} > {max_p95:.3f}")
    if metrics["error_rate"] > max_error_rate:
        reasons.append(f"error_rate {metrics['error_rate']:.6f} > {max_error_rate:.6f}")

    if reasons:
        return False, "; ".join(reasons)
    return True, ""


def main() -> None:
    args = parse_args()
    workspace_root, repo_root = resolve_paths(args)
    engines_cfg = load_structured(Path(args.engines))
    perf_cfg = load_structured(Path(args.config))
    out_dir = Path(args.out_dir)

    timeout_seconds = int(perf_cfg.get("defaults", {}).get("timeout_seconds", 300))
    enabled = [e for e in engines_cfg.get("engines", []) if e.get("enabled", True)]
    workloads = perf_cfg.get("workloads", [])

    run_id = time.strftime("%Y%m%d_%H%M%S")
    run_dir = out_dir / run_id
    ensure_dir(run_dir / "raw")

    rows: List[Dict[str, object]] = []
    fail_rows: List[Dict[str, object]] = []
    summary = {"total_runs": 0, "passed": 0, "failed": 0}

    for wl in workloads:
        workload_id = str(wl["id"])
        lanes = set(wl.get("lanes", []))
        thresholds = wl.get("thresholds", {})
        operations_per_worker = int(wl.get("operations_per_worker", 1))
        conc_levels = [int(x) for x in wl.get("concurrency", [1])]

        for engine in enabled:
            lane = lane_for_engine(engine)
            if lanes and lane not in lanes:
                continue

            binary = resolve_client_binary(engine, repo_root)
            setup_sql = resolve_sql_path(str(wl["setup_sql"][lane]), workspace_root)
            run_sql = resolve_sql_path(str(wl["run_sql"][lane]), workspace_root)
            teardown_sql = resolve_sql_path(str(wl["teardown_sql"][lane]), workspace_root)

            engine_dir = run_dir / "raw" / workload_id / str(engine["id"])
            ensure_dir(engine_dir)

            setup_res = run_sql_file(engine, binary, setup_sql, engine_dir, "setup", timeout_seconds)
            write_json(engine_dir / "setup.json", setup_res.__dict__)
            if setup_res.status != "ok":
                fail_rows.append(
                    {
                        "workload_id": workload_id,
                        "engine_id": engine["id"],
                        "concurrency": 0,
                        "reason": f"setup failed: {setup_res.message}",
                    }
                )
                summary["failed"] += 1
                summary["total_runs"] += 1
                run_sql_file(engine, binary, teardown_sql, engine_dir, "teardown_after_setup_fail", timeout_seconds)
                continue

            for concurrency in conc_levels:
                summary["total_runs"] += 1
                worker_results, wall_ms = run_workers(
                    engine=engine,
                    binary=binary,
                    run_sql=run_sql,
                    run_dir=engine_dir / f"c{concurrency}",
                    timeout_seconds=timeout_seconds,
                    concurrency=concurrency,
                )
                latencies = [int(r.get("elapsed_ms", 0)) for r in worker_results]
                failures = [r for r in worker_results if r.get("status") != "ok"]
                success_count = len(worker_results) - len(failures)
                total_ops = operations_per_worker * success_count
                throughput_tps = (total_ops / (wall_ms / 1000.0)) if wall_ms > 0 else 0.0
                error_rate = (len(failures) / len(worker_results)) if worker_results else 1.0

                metrics: Dict[str, float] = {
                    "throughput_tps": float(throughput_tps),
                    "latency_p50_ms": percentile(latencies, 0.50),
                    "latency_p95_ms": percentile(latencies, 0.95),
                    "latency_p99_ms": percentile(latencies, 0.99),
                    "latency_avg_ms": float(statistics.mean(latencies)) if latencies else 0.0,
                    "error_rate": float(error_rate),
                }

                passed, reason = evaluate_thresholds(metrics, thresholds)
                if passed:
                    summary["passed"] += 1
                else:
                    summary["failed"] += 1
                    fail_rows.append(
                        {
                            "workload_id": workload_id,
                            "engine_id": engine["id"],
                            "concurrency": concurrency,
                            "reason": reason,
                        }
                    )

                row = {
                    "workload_id": workload_id,
                    "engine_id": engine["id"],
                    "lane": lane,
                    "concurrency": concurrency,
                    "operations_per_worker": operations_per_worker,
                    "successful_workers": success_count,
                    "failed_workers": len(failures),
                    "wall_ms": wall_ms,
                    "result": "pass" if passed else "fail",
                    "reason": reason,
                    **{k: round(v, 6) for k, v in metrics.items()},
                }
                rows.append(row)
                write_json(engine_dir / f"c{concurrency}.summary.json", {"metrics": row, "worker_results": worker_results})

            teardown_res = run_sql_file(engine, binary, teardown_sql, engine_dir, "teardown", timeout_seconds)
            write_json(engine_dir / "teardown.json", teardown_res.__dict__)
            if teardown_res.status != "ok":
                summary["total_runs"] += 1
                summary["failed"] += 1
                fail_rows.append(
                    {
                        "workload_id": workload_id,
                        "engine_id": engine["id"],
                        "concurrency": -1,
                        "reason": f"teardown failed: {teardown_res.message}",
                    }
                )

    write_csv(run_dir / "perf_summary.csv", [summary])
    write_csv(run_dir / "perf_all.csv", rows)
    write_csv(
        run_dir / "perf_throughput.csv",
        [{k: r[k] for k in ("workload_id", "engine_id", "lane", "concurrency", "throughput_tps")} for r in rows],
    )
    write_csv(
        run_dir / "perf_latency.csv",
        [
            {
                k: r[k]
                for k in (
                    "workload_id",
                    "engine_id",
                    "lane",
                    "concurrency",
                    "latency_p50_ms",
                    "latency_p95_ms",
                    "latency_p99_ms",
                    "latency_avg_ms",
                )
            }
            for r in rows
        ],
    )
    write_csv(
        run_dir / "perf_resource.csv",
        [
            {
                "workload_id": r["workload_id"],
                "engine_id": r["engine_id"],
                "lane": r["lane"],
                "concurrency": r["concurrency"],
                "cpu_pct": "",
                "rss_mb": "",
                "io_mb_s": "",
            }
            for r in rows
        ],
    )
    write_csv(run_dir / "perf_threshold_failures.csv", fail_rows)
    write_json(run_dir / "perf_results.json", {"summary": summary, "rows": rows, "threshold_failures": fail_rows})


if __name__ == "__main__":
    main()
