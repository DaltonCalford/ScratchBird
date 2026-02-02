#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import subprocess
import sys


def parse_features(value):
    if not value:
        return set()
    return {token.strip() for token in value.split(",") if token.strip()}


def filter_manifest(manifest, features):
    tests = []
    for test in manifest.get("tests", []):
        requires = set(test.get("requires", []))
        if requires and not requires.issubset(features):
            continue
        tests.append(test)
    filtered = dict(manifest)
    filtered["tests"] = tests
    return filtered


def main():
    parser = argparse.ArgumentParser(description="Run SBWP driver conformance harness")
    parser.add_argument("--manifest", default="docs/fixtures/driver_conformance_manifest.json")
    parser.add_argument("--dsn", default=os.environ.get("SB_CONFORMANCE_DSN", ""))
    parser.add_argument("--adapter", default=os.environ.get("SB_CONFORMANCE_ADAPTER", "sbdriver-conformance"))
    parser.add_argument("--features", default=os.environ.get("SB_CONFORMANCE_FEATURES", ""))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    with open(args.manifest, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    features = parse_features(args.features)
    filtered = filter_manifest(manifest, features)

    if args.dry_run:
        json.dump(filtered, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    adapter_path = shutil.which(args.adapter)
    if not adapter_path:
        sys.stderr.write(f"Adapter not found on PATH: {args.adapter}\n")
        return 2

    if not args.dsn:
        sys.stderr.write("Missing DSN. Set --dsn or SB_CONFORMANCE_DSN.\n")
        return 2

    env = dict(os.environ)
    env["SB_CONFORMANCE_DSN"] = args.dsn
    env["SB_CONFORMANCE_FEATURES"] = args.features
    payload = json.dumps(filtered).encode("utf-8")

    result = subprocess.run([adapter_path], input=payload, env=env, check=False)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
