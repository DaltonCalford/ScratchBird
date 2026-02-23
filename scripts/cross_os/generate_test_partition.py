#!/usr/bin/env python3
"""Generate portable vs linux_only test partition from CTest metadata."""

from __future__ import annotations

import argparse
import csv
import json
import pathlib
import re
import subprocess
import sys
from typing import Iterable

LINUX_ONLY_NAME_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"^UnixSocketTest\."), "skip reason: unix_socket_ipc_requires_af_unix"),
    (re.compile(r"^TSAN_"), "skip reason: tsan_linux_toolchain_only"),
]


def _extract_labels(properties: Iterable[dict]) -> list[str]:
    labels: list[str] = []
    for prop in properties:
        if prop.get("name") != "LABELS":
            continue
        value = prop.get("value")
        if isinstance(value, list):
            labels.extend(str(v) for v in value if str(v))
        elif isinstance(value, str) and value:
            labels.extend(x for x in value.split(";") if x)
    return sorted(set(labels))


def _classify(test_name: str, labels: list[str]) -> tuple[str, str]:
    if "linux_only" in labels:
        reason = "linux_only label present"
        for label in labels:
            if label.startswith("skip_"):
                reason = label.replace("skip_", "skip reason: ", 1)
                break
        return ("linux_only", reason)
    for regex, reason in LINUX_ONLY_NAME_PATTERNS:
        if regex.search(test_name):
            return ("linux_only", reason)
    return ("portable", "")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        default="build/linux-gcc-debug",
        help="CMake build directory to inspect (default: build/linux-gcc-debug)",
    )
    parser.add_argument(
        "--output",
        default="artifacts/cross_os/p6s2w3/xos-050-test-partition.csv",
        help="Output CSV path",
    )
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parents[2]
    build_dir = (repo_root / args.build_dir).resolve()
    output_path = (repo_root / args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = ["ctest", "--show-only=json-v1"]
    proc = subprocess.run(
        cmd,
        cwd=build_dir,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        return proc.returncode

    payload = json.loads(proc.stdout)
    tests = payload.get("tests", [])

    with output_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "test_name",
                "labels",
                "bucket",
                "skip_reason",
            ]
        )
        for test in sorted(tests, key=lambda t: t.get("name", "")):
            test_name = test.get("name", "")
            labels = _extract_labels(test.get("properties", []))
            bucket, reason = _classify(test_name, labels)
            writer.writerow(
                [
                    test_name,
                    ";".join(labels),
                    bucket,
                    reason,
                ]
            )

    print(f"wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
