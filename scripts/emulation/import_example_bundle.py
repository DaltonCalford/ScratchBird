#!/usr/bin/env python3
"""
Import ScratchBird example script bundle into:
1) Native SB v3 schema tree (public.examples.<engine>.<script_schema>)
2) Emulated engine parsers for firebird/mysql/postgresql (native/original scripts)

The importer is deterministic and writes per-script logs + summary evidence.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import time
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


CORE_ENGINES = {"firebird", "mysql", "postgresql"}


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def slug_for_path(path: str, max_base: int = 24) -> str:
    base = Path(path).stem.lower()
    base = re.sub(r"[^a-z0-9]+", "_", base).strip("_")
    if not base:
        base = "script"
    base = base[:max_base]
    digest = hashlib.sha1(path.encode("utf-8")).hexdigest()[:10]
    return f"s_{base}_{digest}"


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def normalize_line_endings(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def sanitize_sql(
    text: str,
    lane: str,
    skip_schema_directives: bool = False,
) -> str:
    lines = normalize_line_endings(text).split("\n")
    out: List[str] = []
    in_copy_stdin = False

    for line in lines:
        stripped = line.strip()
        lowered = stripped.lower()

        if in_copy_stdin:
            if stripped == r"\.":
                in_copy_stdin = False
            continue

        # Universal non-SQL/meta lines that should not execute in import mode.
        if not stripped:
            out.append(line)
            continue
        if stripped.startswith("#"):
            continue
        if stripped.startswith("//"):
            continue
        if stripped.startswith("\\"):
            continue
        if lowered.startswith("source "):
            continue
        if lowered.startswith("input "):
            continue
        if lowered.startswith("show version"):
            continue
        if lowered.startswith("set sql dialect"):
            continue
        if lowered == "quit" or lowered == "quit;":
            continue
        if re.match(r"(?i)^copy\b.*\bfrom\s+stdin\b", stripped):
            in_copy_stdin = True
            continue

        if skip_schema_directives and re.match(r"(?i)^create\s+schema\b", stripped):
            continue
        if skip_schema_directives and re.match(r"(?i)^set\s+schema\b", stripped):
            continue

        if lane == "emulation_firebird":
            if re.match(r"(?i)^create\s+database\b", stripped):
                continue
            if re.match(r"(?i)^connect\b", stripped):
                continue

        if lane == "emulation_mysql":
            if re.match(r"(?i)^delimiter\b", stripped):
                continue
            if re.match(r"(?i)^set\s+names\b", stripped):
                continue
            if re.match(r"(?i)^set\s+@{1,2}", stripped):
                continue
            if re.match(r"(?i)^use\b", stripped):
                continue
            if re.match(r"(?i)^create\s+database\b", stripped):
                continue
            if re.match(r"(?i)^drop\s+database\b", stripped):
                continue

        out.append(line)

    return "\n".join(out).rstrip() + "\n"


@dataclass
class LaneResult:
    status: str
    return_code: Optional[int]
    timed_out: bool
    duration_ms: int
    log_path: Optional[str]
    work_script_path: Optional[str]
    reason: Optional[str]


def run_with_log(
    cmd: Sequence[str],
    log_path: Path,
    timeout_sec: int,
    extra_env: Optional[Dict[str, str]] = None,
) -> Tuple[int, bool, int]:
    ensure_parent(log_path)
    start = time.monotonic()
    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)

    header = (
        f"[import-example-bundle] utc={utc_now()}\n"
        f"[import-example-bundle] timeout_sec={timeout_sec}\n"
        f"[import-example-bundle] command={shlex.join(cmd)}\n\n"
    )

    try:
        proc = subprocess.run(
            list(cmd),
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            env=env,
            check=False,
        )
        duration_ms = int((time.monotonic() - start) * 1000)
        body = (proc.stdout or "") + (proc.stderr or "")
        log_path.write_text(header + body, encoding="utf-8", errors="replace")
        return proc.returncode, False, duration_ms
    except subprocess.TimeoutExpired as exc:
        duration_ms = int((time.monotonic() - start) * 1000)
        stdout = exc.stdout or ""
        stderr = exc.stderr or ""
        body = (
            "[import-example-bundle] TIMEOUT EXPIRED\n"
            f"[import-example-bundle] elapsed_ms={duration_ms}\n\n"
            f"{stdout}{stderr}"
        )
        log_path.write_text(header + body, encoding="utf-8", errors="replace")
        return 124, True, duration_ms


def log_contains_transient_connection_error(log_path: Path) -> bool:
    try:
        text = log_path.read_text(encoding="utf-8", errors="replace").lower()
    except OSError:
        return False
    needles = (
        "connection reset by peer",
        "recv() failed",
        "connection failed: connection failed",
        "connection failed",
    )
    return any(n in text for n in needles)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def write_text(path: Path, text: str) -> None:
    ensure_parent(path)
    path.write_text(text, encoding="utf-8")


def load_manifest(path: Path) -> List[Dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        return [row for row in reader]


def lane_skip(reason: str) -> LaneResult:
    return LaneResult(
        status="skipped",
        return_code=None,
        timed_out=False,
        duration_ms=0,
        log_path=None,
        work_script_path=None,
        reason=reason,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Import example script bundle.")
    parser.add_argument("--bundle-root", required=True, help="Bundle root directory")
    parser.add_argument("--output-root", required=True, help="Output evidence directory")
    parser.add_argument("--manifest", default="", help="Override manifest.csv path")
    parser.add_argument("--timeout-sec", type=int, default=90)
    parser.add_argument("--strict-native-core", action="store_true")
    parser.add_argument("--strict-emulation", action="store_true")
    parser.add_argument("--max-scripts", type=int, default=0, help="Debug limiter")

    parser.add_argument("--native-isql", required=True)
    parser.add_argument("--native-host", required=True)
    parser.add_argument("--native-port", required=True)
    parser.add_argument("--native-db", required=True)
    parser.add_argument("--native-user", required=True)
    parser.add_argument("--native-password", required=True)

    parser.add_argument("--pg-isql", default="")
    parser.add_argument("--pg-host", default="127.0.0.1")
    parser.add_argument("--pg-port", default="16432")
    parser.add_argument("--pg-db", default="main")
    parser.add_argument("--pg-user", default="pg_admin")
    parser.add_argument("--pg-password", default="")

    parser.add_argument("--my-isql", default="")
    parser.add_argument("--my-host", default="127.0.0.1")
    parser.add_argument("--my-port", default="16306")
    parser.add_argument("--my-db", default="examples_mysql_emulated")
    parser.add_argument("--my-user", default="root")
    parser.add_argument("--my-password", default="")

    parser.add_argument("--fb-isql", default="")
    parser.add_argument("--fb-work-db-root", default="")

    args = parser.parse_args()

    bundle_root = Path(args.bundle_root).resolve()
    output_root = Path(args.output_root).resolve()
    manifest_path = Path(args.manifest).resolve() if args.manifest else bundle_root / "manifest.csv"
    if not manifest_path.exists():
        print(f"ERROR: manifest not found: {manifest_path}", file=sys.stderr)
        return 2

    mechanical_rows = [
        row
        for row in load_manifest(manifest_path)
        if row.get("conversion_mode") == "mechanical_sql_transform"
    ]
    if args.max_scripts > 0:
        mechanical_rows = mechanical_rows[: args.max_scripts]

    lanes = [
        "native_v3",
        "emulation_firebird",
        "emulation_mysql",
        "emulation_postgresql",
    ]

    lane_stats: Dict[str, Dict[str, int]] = {
        lane: {"ok": 0, "fail": 0, "skipped": 0, "total": 0} for lane in lanes
    }
    lane_engine_stats: Dict[str, Dict[str, Dict[str, int]]] = defaultdict(
        lambda: {lane: {"ok": 0, "fail": 0, "skipped": 0, "total": 0} for lane in lanes}
    )

    records: List[Dict[str, object]] = []

    native_isql = Path(args.native_isql)
    if not native_isql.exists():
        print(f"ERROR: native isql not found: {native_isql}", file=sys.stderr)
        return 2

    pg_isql = Path(args.pg_isql) if args.pg_isql else None
    my_isql = Path(args.my_isql) if args.my_isql else None
    fb_isql = Path(args.fb_isql) if args.fb_isql else None

    if args.fb_work_db_root:
        fb_work_db_root = Path(args.fb_work_db_root)
    else:
        fb_work_db_root = output_root / "emulated" / "firebird" / "db"
    fb_work_db_root.mkdir(parents=True, exist_ok=True)

    emu_mysql_db = args.my_db

    for row in mechanical_rows:
        engine = row["engine"].strip().lower()
        order = int(row["order"])
        rel_source = row["source_path"]
        original_copy_path = Path(row["original_copy_path"])
        converted_path = Path(row["sb_v3_2_path"])

        script_slug = slug_for_path(str(converted_path))
        rec: Dict[str, object] = {
            "order": order,
            "engine": engine,
            "source_path": rel_source,
            "original_copy_path": str(original_copy_path),
            "converted_path": str(converted_path),
        }

        # ------------------------------------------------------------------
        # Native v3 converted script lane
        # ------------------------------------------------------------------
        lane = "native_v3"
        lane_stats[lane]["total"] += 1
        lane_engine_stats[engine][lane]["total"] += 1

        if not converted_path.exists():
            lane_result = lane_skip("converted_script_missing")
        else:
            converted_text = read_text(converted_path)
            converted_body = sanitize_sql(
                converted_text,
                lane=lane,
                skip_schema_directives=True,
            )
            target_schema = f"public.examples.{engine}"
            prelude = (
                "CREATE SCHEMA IF NOT EXISTS public.examples;\n"
                f"CREATE SCHEMA IF NOT EXISTS {target_schema};\n"
                f"SET SCHEMA {target_schema};\n\n"
            )
            native_script = output_root / "work" / lane / engine / f"{script_slug}.sql"
            write_text(native_script, prelude + converted_body)

            native_log = output_root / "logs" / lane / engine / f"{script_slug}.log"
            native_cmd = [
                str(native_isql),
                args.native_db,
                "--mode=local-ipc",
                "--ipc-method=tcp",
                "-H",
                args.native_host,
                "-p",
                args.native_port,
                "-U",
                args.native_user,
                "-P",
                args.native_password,
                "-q",
                "-b",
                "-f",
                str(native_script),
            ]
            rc, timed_out, duration_ms = run_with_log(
                native_cmd,
                native_log,
                args.timeout_sec,
            )
            if rc != 0 and not timed_out and log_contains_transient_connection_error(native_log):
                time.sleep(0.2)
                rc, timed_out, retry_ms = run_with_log(
                    native_cmd,
                    native_log,
                    args.timeout_sec,
                )
                duration_ms += retry_ms
            status = "ok" if rc == 0 and not timed_out else "fail"
            lane_result = LaneResult(
                status=status,
                return_code=rc,
                timed_out=timed_out,
                duration_ms=duration_ms,
                log_path=str(native_log),
                work_script_path=str(native_script),
                reason=None if status == "ok" else ("timeout" if timed_out else "nonzero_exit"),
            )

        lane_stats[lane][lane_result.status] += 1
        lane_engine_stats[engine][lane][lane_result.status] += 1
        rec[lane] = lane_result.__dict__

        # ------------------------------------------------------------------
        # Engine emulation lanes (native/original scripts)
        # ------------------------------------------------------------------
        if engine not in CORE_ENGINES:
            for skip_lane in ("emulation_firebird", "emulation_mysql", "emulation_postgresql"):
                lane_stats[skip_lane]["total"] += 1
                lane_engine_stats[engine][skip_lane]["total"] += 1
                s = lane_skip("engine_not_emulated")
                lane_stats[skip_lane][s.status] += 1
                lane_engine_stats[engine][skip_lane][s.status] += 1
                rec[skip_lane] = s.__dict__
            records.append(rec)
            continue

        # Firebird emulation
        lane = "emulation_firebird"
        lane_stats[lane]["total"] += 1
        lane_engine_stats[engine][lane]["total"] += 1
        if engine != "firebird":
            s = lane_skip("not_firebird_script")
        elif not fb_isql or not fb_isql.exists():
            s = lane_skip("fb_isql_missing")
        elif not original_copy_path.exists():
            s = lane_skip("original_script_missing")
        else:
            body = sanitize_sql(read_text(original_copy_path), lane=lane)
            firebird_db_file = fb_work_db_root / f"{script_slug}.sbdb"
            firebird_driver_db = output_root / "emulated" / "firebird" / "driver" / f"{script_slug}.driver.sbdb"
            firebird_db_file.parent.mkdir(parents=True, exist_ok=True)
            firebird_driver_db.parent.mkdir(parents=True, exist_ok=True)
            firebird_run = output_root / "work" / lane / engine / f"{script_slug}.sql"
            prelude = (
                f"CREATE DATABASE '{firebird_db_file}';\n"
                f"CONNECT '{firebird_db_file}';\n\n"
            )
            write_text(firebird_run, prelude + body)
            fb_log = output_root / "logs" / lane / engine / f"{script_slug}.log"
            fb_cmd = [
                str(fb_isql),
                str(firebird_driver_db),
                "-q",
                "-f",
                str(firebird_run),
            ]
            rc, timed_out, duration_ms = run_with_log(fb_cmd, fb_log, args.timeout_sec)
            if rc != 0 and not timed_out and log_contains_transient_connection_error(fb_log):
                time.sleep(0.2)
                rc, timed_out, retry_ms = run_with_log(fb_cmd, fb_log, args.timeout_sec)
                duration_ms += retry_ms
            status = "ok" if rc == 0 and not timed_out else "fail"
            s = LaneResult(
                status=status,
                return_code=rc,
                timed_out=timed_out,
                duration_ms=duration_ms,
                log_path=str(fb_log),
                work_script_path=str(firebird_run),
                reason=None if status == "ok" else ("timeout" if timed_out else "nonzero_exit"),
            )
        lane_stats[lane][s.status] += 1
        lane_engine_stats[engine][lane][s.status] += 1
        rec[lane] = s.__dict__

        # MySQL emulation
        lane = "emulation_mysql"
        lane_stats[lane]["total"] += 1
        lane_engine_stats[engine][lane]["total"] += 1
        if engine != "mysql":
            s = lane_skip("not_mysql_script")
        elif not my_isql or not my_isql.exists():
            s = lane_skip("my_isql_missing")
        elif not original_copy_path.exists():
            s = lane_skip("original_script_missing")
        else:
            body = sanitize_sql(read_text(original_copy_path), lane=lane)
            mysql_run = output_root / "work" / lane / engine / f"{script_slug}.sql"
            prelude = (
                f"CREATE DATABASE IF NOT EXISTS `{emu_mysql_db}`;\n"
                f"USE `{emu_mysql_db}`;\n\n"
            )
            write_text(mysql_run, prelude + body)
            my_log = output_root / "logs" / lane / engine / f"{script_slug}.log"
            my_cmd = [
                str(my_isql),
                "-h",
                args.my_host,
                "-P",
                args.my_port,
                "-u",
                args.my_user,
                f"-p{args.my_password}",
                "-q",
                "-f",
                str(mysql_run),
            ]
            rc, timed_out, duration_ms = run_with_log(my_cmd, my_log, args.timeout_sec)
            if rc != 0 and not timed_out and log_contains_transient_connection_error(my_log):
                time.sleep(0.2)
                rc, timed_out, retry_ms = run_with_log(my_cmd, my_log, args.timeout_sec)
                duration_ms += retry_ms
            status = "ok" if rc == 0 and not timed_out else "fail"
            s = LaneResult(
                status=status,
                return_code=rc,
                timed_out=timed_out,
                duration_ms=duration_ms,
                log_path=str(my_log),
                work_script_path=str(mysql_run),
                reason=None if status == "ok" else ("timeout" if timed_out else "nonzero_exit"),
            )
        lane_stats[lane][s.status] += 1
        lane_engine_stats[engine][lane][s.status] += 1
        rec[lane] = s.__dict__

        # PostgreSQL emulation
        lane = "emulation_postgresql"
        lane_stats[lane]["total"] += 1
        lane_engine_stats[engine][lane]["total"] += 1
        if engine != "postgresql":
            s = lane_skip("not_postgresql_script")
        elif not pg_isql or not pg_isql.exists():
            s = lane_skip("pg_isql_missing")
        elif not original_copy_path.exists():
            s = lane_skip("original_script_missing")
        else:
            body = sanitize_sql(read_text(original_copy_path), lane=lane)
            pg_run = output_root / "work" / lane / engine / f"{script_slug}.sql"
            write_text(pg_run, body)
            pg_log = output_root / "logs" / lane / engine / f"{script_slug}.log"
            pg_cmd = [
                str(pg_isql),
                "-h",
                args.pg_host,
                "-p",
                args.pg_port,
                "-U",
                args.pg_user,
                "-d",
                args.pg_db,
                "-q",
                "-f",
                str(pg_run),
            ]
            rc, timed_out, duration_ms = run_with_log(
                pg_cmd,
                pg_log,
                args.timeout_sec,
                extra_env={"PGPASSWORD": args.pg_password},
            )
            if rc != 0 and not timed_out and log_contains_transient_connection_error(pg_log):
                time.sleep(0.2)
                rc, timed_out, retry_ms = run_with_log(
                    pg_cmd,
                    pg_log,
                    args.timeout_sec,
                    extra_env={"PGPASSWORD": args.pg_password},
                )
                duration_ms += retry_ms
            status = "ok" if rc == 0 and not timed_out else "fail"
            s = LaneResult(
                status=status,
                return_code=rc,
                timed_out=timed_out,
                duration_ms=duration_ms,
                log_path=str(pg_log),
                work_script_path=str(pg_run),
                reason=None if status == "ok" else ("timeout" if timed_out else "nonzero_exit"),
            )
        lane_stats[lane][s.status] += 1
        lane_engine_stats[engine][lane][s.status] += 1
        rec[lane] = s.__dict__

        records.append(rec)

    summary = {
        "generated_at_utc": utc_now(),
        "bundle_root": str(bundle_root),
        "manifest_path": str(manifest_path),
        "mechanical_entries": len(mechanical_rows),
        "strict_native_core": args.strict_native_core,
        "strict_emulation": args.strict_emulation,
        "lane_stats": lane_stats,
        "lane_engine_stats": lane_engine_stats,
        "records": records,
    }

    output_root.mkdir(parents=True, exist_ok=True)
    summary_json = output_root / "SUMMARY.json"
    summary_json.write_text(json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8")

    md_lines = [
        "# Example Bundle Import Summary",
        "",
        f"- Generated (UTC): `{summary['generated_at_utc']}`",
        f"- Bundle root: `{bundle_root}`",
        f"- Mechanical scripts processed: `{len(mechanical_rows)}`",
        "",
        "## Lane Totals",
        "",
        "| Lane | OK | Fail | Skipped | Total |",
        "|---|---:|---:|---:|---:|",
    ]
    for lane_name in lanes:
        stats = lane_stats[lane_name]
        md_lines.append(
            f"| `{lane_name}` | {stats['ok']} | {stats['fail']} | {stats['skipped']} | {stats['total']} |"
        )

    summary_md = output_root / "SUMMARY.md"
    summary_md.write_text("\n".join(md_lines) + "\n", encoding="utf-8")

    strict_fail_native = 0
    strict_fail_emu = 0

    # Hard-gate mode: any lane failure in scoped verification surfaces fails the run.
    if args.strict_native_core:
        strict_fail_native = lane_stats["native_v3"]["fail"]

    if args.strict_emulation:
        strict_fail_emu = (
            lane_stats["emulation_firebird"]["fail"]
            + lane_stats["emulation_mysql"]["fail"]
            + lane_stats["emulation_postgresql"]["fail"]
        )

    if strict_fail_native > 0 or strict_fail_emu > 0:
        print(
            f"FAIL: strict gates failed native_failures={strict_fail_native} "
            f"emulation_failures={strict_fail_emu}. "
            f"See {summary_json}",
            file=sys.stderr,
        )
        return 1

    print(f"PASS: example bundle import completed. Summary: {summary_json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
