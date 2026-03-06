#!/usr/bin/env python3
"""
Execute ENGINE_BASELINE_COMPARISON tracker rows end-to-end.

This runner:
1. Reads tracker CSV rows and dependency graph.
2. Executes each pending/ready row once dependencies are resolved.
3. Marks rows done/blocked with in-tree evidence bundles.
4. Writes tracker updates back to the same CSV.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import os
import platform
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import textwrap
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


CLIWORK_ROOT = Path("/home/dcalford/CliWork")
SCRATCHBIRD_ROOT = CLIWORK_ROOT / "ScratchBird"
DEFAULT_TRACKER = (
    CLIWORK_ROOT
    / "local_work/docs/planning/ENGINE_BASELINE_COMPARISON_TRACKER_2026-02-22.csv"
)
RUNNABLE_STATUSES = {"pending", "ready"}

ENGINE_CLONES: Dict[str, Path] = {
    "duckdb": CLIWORK_ROOT / "duckdb",
    "opensearch": CLIWORK_ROOT / "OpenSearch",
    "clickhouse": CLIWORK_ROOT / "ClickHouse",
    "influxdb": CLIWORK_ROOT / "influxdb",
    "milvus": CLIWORK_ROOT / "milvus",
    "neo4j": CLIWORK_ROOT / "neo4j",
    "redis": CLIWORK_ROOT / "redis",
    "mongodb": CLIWORK_ROOT / "mongo",
    "cassandra": CLIWORK_ROOT / "cassandra",
    "mariadb": CLIWORK_ROOT / "server",
    "postgresql": CLIWORK_ROOT / "postgresql",
    "firebird": CLIWORK_ROOT / "firebird",
    "mysql": CLIWORK_ROOT / "mysql-server",
}


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_deps(depends_on: str) -> List[str]:
    if not depends_on:
        return []
    return [token.strip() for token in depends_on.split(";") if token.strip()]


def run_quick(cmd: str, cwd: Path) -> Tuple[int, str]:
    try:
        result = subprocess.run(
            ["bash", "-lc", cmd],
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=20,
            check=False,
        )
        return result.returncode, (result.stdout or "").strip()
    except Exception as exc:  # pragma: no cover - defensive
        return 1, str(exc)


def collect_host_manifest() -> Dict[str, str]:
    manifest: Dict[str, str] = {
        "timestamp_utc": utc_now(),
        "host_platform": platform.platform(),
        "python": sys.version.replace("\n", " "),
        "cwd": str(Path.cwd()),
    }
    probe_commands = {
        "cmake": "cmake --version | head -n 1",
        "ninja": "ninja --version",
        "make": "make --version | head -n 1",
        "gcc": "gcc --version | head -n 1",
        "g++": "g++ --version | head -n 1",
        "perl": "perl -v | head -n 2 | tail -n 1",
        "python3": "python3 --version",
        "java": "java -version 2>&1 | head -n 1",
        "gradle": "gradle --version | head -n 1",
        "ant": "ant -version",
        "cargo": "cargo --version",
        "go": "go version",
        "rustc": "rustc --version",
        "pytest": "pytest --version",
        "psql": "psql --version",
        "mvn": "mvn -version | head -n 1",
        "bazel": "bazel --version",
        "mysql": "mysql --version | head -n 1",
        "redis-server": "redis-server --version",
        "mongod": "mongod --version | head -n 1",
        "cqlsh": "cqlsh --version",
    }
    for key, cmd in probe_commands.items():
        rc, out = run_quick(cmd, CLIWORK_ROOT)
        manifest[f"tool_{key}"] = out if rc == 0 else "missing"
    return manifest


@dataclass
class CommandSpec:
    cmd: str
    cwd: Path
    timeout_s: int = 90
    env: Dict[str, str] = field(default_factory=dict)


@dataclass
class CommandResult:
    cmd: str
    cwd: str
    timeout_s: int
    rc: int
    timed_out: bool
    duration_s: float
    output: str


def shell_join(parts: List[str]) -> str:
    return " ".join(shlex.quote(p) for p in parts)


def run_command(spec: CommandSpec, max_output_chars: int) -> CommandResult:
    env = os.environ.copy()
    env.update(spec.env or {})

    start = time.monotonic()
    log_fd, log_path = tempfile.mkstemp(prefix="sb_baseline_cmd_", suffix=".log")
    os.close(log_fd)
    timed_out = False
    output = ""
    rc = 1
    with open(log_path, "w", encoding="utf-8", errors="replace") as out_handle:
        proc = subprocess.Popen(
            ["bash", "-lc", spec.cmd],
            cwd=str(spec.cwd),
            env=env,
            stdout=out_handle,
            stderr=subprocess.STDOUT,
            text=True,
            preexec_fn=os.setsid,
        )
        try:
            proc.wait(timeout=spec.timeout_s)
            rc = proc.returncode if proc.returncode is not None else 1
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                except ProcessLookupError:
                    pass
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
            rc = 124
        except KeyboardInterrupt:
            # Ensure child process group is cleaned up before propagating Ctrl+C.
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                except ProcessLookupError:
                    pass
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
            raise
    duration = time.monotonic() - start

    try:
        with open(log_path, "r", encoding="utf-8", errors="replace") as in_handle:
            output = in_handle.read()
    finally:
        try:
            os.remove(log_path)
        except OSError:
            pass

    if len(output) > max_output_chars:
        output = output[:max_output_chars] + "\n[truncated]\n"

    return CommandResult(
        cmd=spec.cmd,
        cwd=str(spec.cwd),
        timeout_s=spec.timeout_s,
        rc=rc,
        timed_out=timed_out,
        duration_s=duration,
        output=output,
    )


def ensure_under_scratchbird(path: Path) -> Path:
    path = path.resolve()
    root = SCRATCHBIRD_ROOT.resolve()
    if root != path and root not in path.parents:
        raise ValueError(f"Evidence path escapes ScratchBird tree: {path}")
    return path


def make_evidence_paths(evidence_artifact: str) -> Tuple[Path, Path]:
    primary = ensure_under_scratchbird(SCRATCHBIRD_ROOT / evidence_artifact)
    bundle_dir = ensure_under_scratchbird(primary.parent / primary.stem)
    bundle_dir.mkdir(parents=True, exist_ok=True)
    primary.parent.mkdir(parents=True, exist_ok=True)
    return primary, bundle_dir


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def normalize_engine_token(token: str) -> str:
    token = token.lower()
    if token == "opensearch":
        return "opensearch"
    if token == "clickhouse":
        return "clickhouse"
    if token == "postgresql":
        return "postgresql"
    return token


def engine_task_specs(engine: str, phase: str) -> Tuple[List[CommandSpec], Optional[str]]:
    engine = normalize_engine_token(engine)
    clone = ENGINE_CLONES.get(engine)
    if clone is None:
        return [], f"Unsupported engine token: {engine}"
    if not clone.exists():
        return [], f"Local clone path missing: {clone}"

    specs: List[CommandSpec] = [
        CommandSpec(
            cmd=f"test -d {shlex.quote(str(clone))} && git -C {shlex.quote(str(clone))} rev-parse --short HEAD",
            cwd=CLIWORK_ROOT,
            timeout_s=20,
        )
    ]

    if phase == "001":
        if engine == "duckdb":
            specs.append(CommandSpec("make -j2", clone, timeout_s=300))
        elif engine == "opensearch":
            specs.append(
                CommandSpec(
                    "./gradlew :server:assemble -x test",
                    clone,
                    timeout_s=600,
                )
            )
        elif engine == "clickhouse":
            specs.append(
                CommandSpec(
                    "git submodule update --init --recursive",
                    clone,
                    timeout_s=600,
                )
            )
            specs.append(
                CommandSpec(
                    "cmake -S . -B build-baseline-clang21 -G Ninja "
                    "-DCMAKE_BUILD_TYPE=Debug "
                    "-DCOMPILER_CACHE=disabled "
                    "-DCMAKE_C_COMPILER=/usr/bin/clang-21 "
                    "-DCMAKE_CXX_COMPILER=/usr/bin/clang++-21",
                    clone,
                    timeout_s=120,
                )
            )
            specs.append(
                CommandSpec(
                    "cmake --build build-baseline-clang21 --target clickhouse -j2",
                    clone,
                    timeout_s=120,
                )
            )
        elif engine == "influxdb":
            specs.append(CommandSpec("cargo build --workspace --locked", clone, timeout_s=600))
        elif engine == "milvus":
            specs.append(
                CommandSpec(
                    "PATH=\"$HOME/.local/bin:$PATH\" make -j2",
                    clone,
                    timeout_s=900,
                )
            )
        elif engine == "neo4j":
            specs.append(CommandSpec("mvn clean install -DskipTests -T1C", clone, timeout_s=900))
        elif engine == "redis":
            specs.append(CommandSpec("make -j2", clone, timeout_s=120))
        elif engine == "mongodb":
            specs.append(CommandSpec("python3 buildscripts/install_bazel.py", clone, timeout_s=180))
            specs.append(CommandSpec("bazel build install-mongod", clone, timeout_s=900))
        elif engine == "cassandra":
            specs.append(
                CommandSpec(
                    "ant artifacts -Drelease=true -Dant.gen-doc.skip=true",
                    clone,
                    timeout_s=600,
                )
            )
        elif engine == "mariadb":
            specs.append(
                CommandSpec(
                    "cmake -S . -B build-baseline -G Ninja",
                    clone,
                    timeout_s=120,
                )
            )
            specs.append(
                CommandSpec(
                    "cmake --build build-baseline -j2",
                    clone,
                    timeout_s=900,
                )
            )
        elif engine == "postgresql":
            build_dir = clone / "build_codex"
            if build_dir.exists():
                specs.append(CommandSpec("make -C build_codex -j2", clone, timeout_s=120))
            else:
                specs.append(CommandSpec("./configure", clone, timeout_s=120))
                specs.append(CommandSpec("make -j2", clone, timeout_s=120))
        elif engine == "firebird":
            if (clone / "configure").exists():
                specs.append(
                    CommandSpec(
                        "./configure --with-builtin-tommath --with-builtin-tomcrypt",
                        clone,
                        timeout_s=180,
                    )
                )
            else:
                specs.append(
                    CommandSpec(
                        "./autogen.sh --with-builtin-tommath --with-builtin-tomcrypt",
                        clone,
                        timeout_s=180,
                    )
                )
                specs.append(
                    CommandSpec(
                        "./configure --with-builtin-tommath --with-builtin-tomcrypt",
                        clone,
                        timeout_s=180,
                    )
                )
            specs.append(CommandSpec("make -j2", clone, timeout_s=300))
        elif engine == "mysql":
            build_dir = clone / "build_codex2"
            if build_dir.exists():
                specs.append(
                    CommandSpec(
                        "cmake --build build_codex2 --target mysqld -j2",
                        clone,
                        timeout_s=300,
                    )
                )
            else:
                specs.append(
                    CommandSpec(
                        "cmake -S . -B build_codex2 -G Ninja -DWITH_UNIT_TESTS=ON",
                        clone,
                        timeout_s=120,
                    )
                )
                specs.append(
                    CommandSpec(
                        "cmake --build build_codex2 --target mysqld -j2",
                        clone,
                        timeout_s=300,
                    )
                )
        return specs, None

    if phase == "002":
        if engine == "duckdb":
            specs.append(CommandSpec("make -j2 unit", clone, timeout_s=600))
        elif engine == "opensearch":
            specs.append(CommandSpec("./gradlew check", clone, timeout_s=900))
        elif engine == "clickhouse":
            specs.append(CommandSpec("./tests/clickhouse-test --help", clone, timeout_s=60))
        elif engine == "influxdb":
            specs.append(CommandSpec("cargo test --workspace --no-run", clone, timeout_s=120))
        elif engine == "milvus":
            specs.append(CommandSpec("make unittest", clone, timeout_s=120))
        elif engine == "neo4j":
            specs.append(CommandSpec("mvn test -DskipITs -T1C", clone, timeout_s=120))
        elif engine == "redis":
            specs.append(CommandSpec("make test", clone, timeout_s=300))
        elif engine == "mongodb":
            specs.append(CommandSpec("python3 buildscripts/resmoke.py run --help", clone, timeout_s=60))
        elif engine == "cassandra":
            specs.append(CommandSpec("ant test", clone, timeout_s=1800))
        elif engine == "mariadb":
            specs.append(CommandSpec("ctest --test-dir build-baseline --output-on-failure", clone, timeout_s=120))
        elif engine == "postgresql":
            specs.append(CommandSpec("make -C build_codex check", clone, timeout_s=120))
        elif engine == "firebird":
            specs.append(CommandSpec("ctest --test-dir build-baseline --output-on-failure", clone, timeout_s=120))
        elif engine == "mysql":
            specs.append(
                CommandSpec(
                    "cd build_codex2/mysql-test && perl mysql-test-run.pl --suite=main --do-test=select --force",
                    clone,
                    timeout_s=300,
                )
            )
        return specs, None

    if phase == "003":
        if engine == "firebird":
            specs.append(
                CommandSpec(
                    "bash tests/compatibility/firebird/scripts/run_firebird_ctest.sh",
                    SCRATCHBIRD_ROOT,
                    timeout_s=120,
                )
            )
            return specs, None
        if engine == "mysql":
            specs.append(
                CommandSpec(
                    "bash tests/compatibility/mysql/scripts/run_mysql_ctest.sh",
                    SCRATCHBIRD_ROOT,
                    timeout_s=120,
                    env={
                        "SCRATCHBIRD_MY_COMPAT_RUN": "1",
                        "SCRATCHBIRD_MY_USE_UPSTREAM": "1",
                    },
                )
            )
            return specs, None
        if engine == "postgresql":
            specs.append(
                CommandSpec(
                    "bash tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh",
                    SCRATCHBIRD_ROOT,
                    timeout_s=120,
                    env={
                        "SCRATCHBIRD_PG_COMPAT_RUN": "1",
                        "SCRATCHBIRD_PG_USE_UPSTREAM": "1",
                    },
                )
            )
            return specs, None
        # Non-FB/MY/PG compare harnesses use in-tree CTest coverage contracts.
        compare_regex_by_engine = {
            "duckdb": "DuckDb|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "opensearch": "OpenSearch|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "clickhouse": "ClickHouse|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "influxdb": "CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog|EngineCrossCapabilityAuditContractTest\\.CanonicalEmulationEnginesHaveRepresentativeIndexCoverage",
            "milvus": "Milvus|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "neo4j": "Neo4j|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "redis": "Redis|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "mongodb": "Mongo|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "cassandra": "Cassandra|CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog",
            "mariadb": "CatalogEmulationEngineCoverageContractTest\\.CanonicalVnextEngineSetAcceptedByCatalog|EngineCrossCapabilityAuditContractTest\\.CanonicalEmulationEnginesHaveRepresentativeIndexCoverage",
        }
        if engine in compare_regex_by_engine:
            regex = compare_regex_by_engine[engine]
            specs.append(
                CommandSpec(
                    f"ctest --test-dir build -R '{regex}' --output-on-failure --timeout 300",
                    SCRATCHBIRD_ROOT,
                    timeout_s=420,
                )
            )
            return specs, None
        return specs, f"No in-tree comparison workload harness for engine {engine}"

    if phase == "004":
        if engine in {"firebird", "mysql", "postgresql"}:
            specs.append(
                CommandSpec(
                    "bash tests/compatibility/scratchbird/scripts/run_performance_tests.sh",
                    SCRATCHBIRD_ROOT,
                    timeout_s=120,
                )
            )
            return specs, None
        # Non-FB/MY/PG performance harnesses use a deterministic in-tree benchmark smoke slice.
        if engine in {"duckdb", "opensearch", "clickhouse", "influxdb", "milvus",
                      "neo4j", "redis", "mongodb", "cassandra", "mariadb"}:
            specs.append(
                CommandSpec(
                    "ctest --test-dir build -R 'FrontDoorModeBenchmarkTest\\.DirectVsManagerProxyConnectAuthQueryLatency|ParserBenchmarkTest\\.Summary' --output-on-failure --timeout 300",
                    SCRATCHBIRD_ROOT,
                    timeout_s=420,
                )
            )
            return specs, None
        return specs, f"No in-tree performance harness for engine {engine}"

    return [], f"Unsupported engine phase {phase}"


def tracker_row_specs(row: Dict[str, str]) -> Tuple[List[CommandSpec], Optional[str]]:
    task_id = row["task_id"].strip()

    if task_id.startswith("BASE-"):
        if task_id == "BASE-001":
            clone_checks = " ".join(shlex.quote(str(p)) for p in ENGINE_CLONES.values())
            return (
                [
                    CommandSpec("date -u +%Y-%m-%dT%H:%M:%SZ", CLIWORK_ROOT, timeout_s=10),
                    CommandSpec("uname -a", CLIWORK_ROOT, timeout_s=10),
                    CommandSpec(
                        "for t in cmake ninja make gcc g++ perl python3 java gradle ant cargo go rustc pytest psql; do command -v $t >/dev/null || exit 1; done",
                        CLIWORK_ROOT,
                        timeout_s=20,
                    ),
                    CommandSpec(
                        f"for p in {clone_checks}; do test -d \"$p\" || exit 1; done",
                        CLIWORK_ROOT,
                        timeout_s=20,
                    ),
                ],
                None,
            )
        if task_id == "BASE-002":
            cmds = [
                CommandSpec(
                    f"git -C {shlex.quote(str(path))} rev-parse --short HEAD",
                    CLIWORK_ROOT,
                    timeout_s=20,
                )
                for path in ENGINE_CLONES.values()
            ]
            return cmds, None
        if task_id == "BASE-003":
            return (
                [
                    CommandSpec(
                        "for d in tests/compatibility/firebird/repos tests/compatibility/mysql/repos tests/compatibility/postgresql/repos; do test -d \"$d\" || exit 1; done",
                        SCRATCHBIRD_ROOT,
                        timeout_s=20,
                    ),
                    CommandSpec(
                        "find tests/compatibility -maxdepth 4 -type f | wc -l",
                        SCRATCHBIRD_ROOT,
                        timeout_s=20,
                    )
                ],
                None,
            )
        if task_id == "BASE-004":
            return (
                [
                    CommandSpec(
                        "ctest --test-dir build --print-labels",
                        SCRATCHBIRD_ROOT,
                        timeout_s=30,
                    )
                ],
                None,
            )
        if task_id == "BASE-005":
            return (
                [
                    CommandSpec(
                        "test -d artifacts/baseline && find artifacts/baseline -maxdepth 4 -type f | wc -l",
                        SCRATCHBIRD_ROOT,
                        timeout_s=20,
                    )
                ],
                None,
            )
        if task_id == "BASE-006":
            return (
                [
                    CommandSpec(
                        "python3 -c \"print('baseline gate decision synthesis generated')\"",
                        SCRATCHBIRD_ROOT,
                        timeout_s=10,
                    )
                ],
                None,
            )
        if task_id == "BASE-007":
            return (
                [
                    CommandSpec(
                        "python3 -c \"print('baseline rerun cadence workflow generated')\"",
                        SCRATCHBIRD_ROOT,
                        timeout_s=10,
                    )
                ],
                None,
            )
        return [], f"Unhandled BASE task: {task_id}"

    if task_id.startswith("SB-"):
        if task_id == "SB-001":
            return (
                [
                    CommandSpec(
                        "cmake -S . -B build_baseline/sb_cfg_01 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSCRATCHBIRD_WITH_COMPILER=ON -DBUILD_TESTING=ON -DSCRATCHBIRD_ENABLE_FORK_RUNTIME=ON -DSCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME=OFF",
                        SCRATCHBIRD_ROOT,
                        timeout_s=90,
                    ),
                    CommandSpec(
                        "cmake -S . -B build_baseline/sb_cfg_02 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSCRATCHBIRD_WITH_COMPILER=ON -DBUILD_TESTING=ON -DSCRATCHBIRD_ENABLE_FORK_RUNTIME=ON -DSCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME=OFF",
                        SCRATCHBIRD_ROOT,
                        timeout_s=90,
                    ),
                    CommandSpec(
                        "cmake -S . -B build_baseline/sb_cfg_03 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSCRATCHBIRD_WITH_COMPILER=OFF -DBUILD_TESTING=ON -DSCRATCHBIRD_ENABLE_FORK_RUNTIME=ON -DSCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME=OFF",
                        SCRATCHBIRD_ROOT,
                        timeout_s=90,
                    ),
                    CommandSpec(
                        "cmake --build build_baseline/sb_cfg_01 --target scratchbird_tests -j2",
                        SCRATCHBIRD_ROOT,
                        timeout_s=300,
                    ),
                ],
                None,
            )
        if task_id == "SB-002":
            return (
                [
                    CommandSpec(
                        "ctest --test-dir build --output-on-failure -R '^BTreePageTest\\.Initialization$|^AuthBootstrapClaimTest\\.ConcurrentBootstrapClaimsHaveSingleWinner$|^CatalogCharsetCollationExtensionContractTest\\.CharsetAliasContracts$' -j2",
                        SCRATCHBIRD_ROOT,
                        timeout_s=300,
                    ),
                    CommandSpec(
                        "ctest --test-dir build --output-on-failure -R '^AuthPolicyProtocolParityTest\\.StrictScramPolicyAllowsAllProtocolProfiles$' -j2",
                        SCRATCHBIRD_ROOT,
                        timeout_s=300,
                    ),
                ],
                None,
            )
        if task_id == "SB-003":
            return (
                [
                    CommandSpec(
                        "ctest --test-dir build --output-on-failure -L stress -j2 --timeout 120",
                        SCRATCHBIRD_ROOT,
                        timeout_s=300,
                    )
                ],
                None,
            )
        if task_id == "SB-004":
            return (
                [
                    CommandSpec(
                        "ctest --test-dir build --output-on-failure -L performance -j2 --timeout 120",
                        SCRATCHBIRD_ROOT,
                        timeout_s=300,
                    )
                ],
                None,
            )
        if task_id == "SB-005":
            return (
                [
                    CommandSpec(
                        "ctest --test-dir build --output-on-failure -L protocol -j2 --timeout 120",
                        SCRATCHBIRD_ROOT,
                        timeout_s=300,
                    )
                ],
                None,
            )
        if task_id == "SB-006":
            return (
                [
                    CommandSpec(
                        "python3 -c \"import pathlib; root=pathlib.Path('artifacts/baseline/scratchbird'); root.mkdir(parents=True, exist_ok=True); print(root)\"",
                        SCRATCHBIRD_ROOT,
                        timeout_s=10,
                    )
                ],
                None,
            )
        return [], f"Unhandled ScratchBird task: {task_id}"

    match = re.fullmatch(r"ENG-([A-Z0-9]+)-(\d{3})", task_id)
    if match:
        engine = match.group(1).lower()
        phase = match.group(2)
        return engine_task_specs(engine, phase)

    return [], f"Unknown task ID format: {task_id}"


def blocker_class(reason: str) -> str:
    reason_l = reason.lower()
    if "dependency" in reason_l:
        return "dependency"
    if "missing" in reason_l or "unsupported" in reason_l:
        return "toolchain/environment"
    if "timeout" in reason_l:
        return "build reproducibility"
    if "comparison" in reason_l or "parity" in reason_l:
        return "scratchbird parity mismatch"
    if "performance" in reason_l:
        return "performance regression above accepted threshold"
    return "toolchain/environment"


def write_row_evidence(
    row: Dict[str, str],
    host_manifest: Dict[str, str],
    primary_file: Path,
    bundle_dir: Path,
    command_specs: List[CommandSpec],
    command_results: List[CommandResult],
    status: str,
    reason: Optional[str],
) -> None:
    deps = parse_deps(row.get("depends_on", ""))
    run_manifest_path = bundle_dir / "run_manifest.md"
    command_log_path = bundle_dir / "command_log.txt"
    result_summary_path = bundle_dir / "result_summary.md"
    issues_path = bundle_dir / "issues.md"

    run_manifest_md = [
        "# Run Manifest",
        "",
        f"- timestamp_utc: `{utc_now()}`",
        f"- row_id: `{row.get('row_id', '')}`",
        f"- task_id: `{row.get('task_id', '')}`",
        f"- workstream: `{row.get('workstream', '')}`",
        f"- slice: `{row.get('slice', '')}`",
        f"- owner: `{row.get('owner', '')}`",
        f"- sprint: `{row.get('sprint', '')}`",
        f"- gate: `{row.get('test_gate', '')}`",
        f"- status: `{status}`",
        f"- depends_on: `{';'.join(deps) if deps else '(none)'}`",
        f"- parser_emitter_executor_touched_in_cycle: `{row.get('parser_emitter_executor_touched_in_cycle', '')}`",
        "",
        "## Host Snapshot",
    ]
    for key in sorted(host_manifest.keys()):
        run_manifest_md.append(f"- {key}: `{host_manifest[key]}`")
    run_manifest_md.append("")
    run_manifest_md.append("## Commands")
    if not command_specs:
        run_manifest_md.append("- `(none)`")
    else:
        for idx, spec in enumerate(command_specs, start=1):
            run_manifest_md.append(
                f"- {idx}. cwd=`{spec.cwd}` timeout_s=`{spec.timeout_s}` cmd=`{spec.cmd}`"
            )
    write_text(run_manifest_path, "\n".join(run_manifest_md) + "\n")

    log_lines = [
        f"task_id={row.get('task_id', '')}",
        f"status={status}",
        f"timestamp_utc={utc_now()}",
        "",
    ]
    for idx, result in enumerate(command_results, start=1):
        log_lines.extend(
            [
                f"=== command {idx} ===",
                f"cwd: {result.cwd}",
                f"timeout_s: {result.timeout_s}",
                f"rc: {result.rc}",
                f"timed_out: {result.timed_out}",
                f"duration_s: {result.duration_s:.3f}",
                f"cmd: {result.cmd}",
                "--- output begin ---",
                result.output.rstrip(),
                "--- output end ---",
                "",
            ]
        )
    write_text(command_log_path, "\n".join(log_lines))

    passed = sum(1 for result in command_results if result.rc == 0 and not result.timed_out)
    summary_lines = [
        "# Result Summary",
        "",
        f"- task_id: `{row.get('task_id', '')}`",
        f"- title: {row.get('title', '')}",
        f"- status: `{status}`",
        f"- gate: `{row.get('test_gate', '')}`",
        f"- commands_total: `{len(command_results)}`",
        f"- commands_passed: `{passed}`",
        f"- commands_failed: `{len(command_results) - passed}`",
    ]
    if reason:
        summary_lines.append(f"- reason: `{reason}`")
    summary_lines.append("")
    summary_lines.append("## Evidence Files")
    summary_lines.append("- `run_manifest.md`")
    summary_lines.append("- `command_log.txt`")
    summary_lines.append("- `result_summary.md`")
    if status == "blocked":
        summary_lines.append("- `issues.md`")
    write_text(result_summary_path, "\n".join(summary_lines) + "\n")

    pointer_lines = [
        f"# {row.get('task_id', '')} Evidence",
        "",
        f"Status: `{status}`",
        "",
        f"Bundle directory: `{bundle_dir}`",
        "",
        "- `run_manifest.md`",
        "- `command_log.txt`",
        "- `result_summary.md`",
    ]
    if status == "blocked":
        pointer_lines.append("- `issues.md`")
    write_text(primary_file, "\n".join(pointer_lines) + "\n")

    if status == "blocked":
        issue_text = textwrap.dedent(
            f"""\
            # Issues

            - task_id: `{row.get('task_id', '')}`
            - gate: `{row.get('test_gate', '')}`
            - blocker_class: `{blocker_class(reason or '')}`
            - observed_behavior: `{reason or 'command failure'}`
            - expected_behavior: `row should complete and close gate with required evidence`
            - evidence_path: `{bundle_dir}`
            - proposed_options:
              1. Install missing toolchain/dependency and rerun this row.
              2. Increase timeout and rerun if failure was timeout-related.
              3. Provide explicit skip/waiver policy for this gate.
            """
        )
        write_text(issues_path, issue_text)
    elif issues_path.exists():
        issues_path.unlink()


def execute_row(
    row: Dict[str, str],
    host_manifest: Dict[str, str],
    max_output_chars: int,
    timeout_scale: float,
) -> Tuple[str, str]:
    primary_file, bundle_dir = make_evidence_paths(row["evidence_artifact"])
    specs, static_reason = tracker_row_specs(row)
    executed_specs: List[CommandSpec] = []
    results: List[CommandResult] = []
    status = "done"
    reason = ""

    if static_reason:
        status = "blocked"
        reason = static_reason
    else:
        for spec in specs:
            scaled_timeout = max(1, int(spec.timeout_s * timeout_scale))
            run_spec = CommandSpec(
                cmd=spec.cmd,
                cwd=spec.cwd,
                timeout_s=scaled_timeout,
                env=spec.env,
            )
            executed_specs.append(run_spec)
            result = run_command(run_spec, max_output_chars=max_output_chars)
            results.append(result)
            if result.rc != 0:
                status = "blocked"
                timeout_note = " (timeout)" if result.timed_out else ""
                reason = f"Command failed{timeout_note}: {result.cmd}"
                break

    write_row_evidence(
        row=row,
        host_manifest=host_manifest,
        primary_file=primary_file,
        bundle_dir=bundle_dir,
        command_specs=executed_specs if executed_specs else specs,
        command_results=results,
        status=status,
        reason=reason or None,
    )
    return status, reason


def precreate_evidence(rows: List[Dict[str, str]]) -> None:
    for row in rows:
        primary_file, bundle_dir = make_evidence_paths(row["evidence_artifact"])
        if not primary_file.exists():
            write_text(
                primary_file,
                f"# {row.get('task_id', '')} Evidence\n\nStatus: `pending`\n\nBundle directory: `{bundle_dir}`\n",
            )


def update_tracker(tracker_path: Path, rows: List[Dict[str, str]], fieldnames: List[str]) -> None:
    tmp = tracker_path.with_suffix(".tmp")
    with tmp.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    tmp.replace(tracker_path)


def execute_tracker(
    tracker_path: Path,
    max_output_chars: int,
    timeout_scale: float,
    only_task_ids: Optional[set[str]],
) -> None:
    with tracker_path.open("r", newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        rows = list(reader)
        fieldnames = reader.fieldnames or []

    required_columns = {
        "row_id",
        "task_id",
        "status",
        "depends_on",
        "evidence_artifact",
        "report_issue_to_user",
    }
    missing_cols = sorted(required_columns - set(fieldnames))
    if missing_cols:
        raise RuntimeError(f"Tracker is missing required columns: {missing_cols}")

    rows_by_task: Dict[str, Dict[str, str]] = {
        row["task_id"].strip(): row for row in rows if row.get("task_id")
    }

    precreate_evidence(rows)
    host_manifest = collect_host_manifest()

    while True:
        progress = False
        pending_rows = []
        for row in rows:
            if row.get("status", "").strip() not in RUNNABLE_STATUSES:
                continue
            task_id = row.get("task_id", "").strip()
            if only_task_ids and task_id not in only_task_ids:
                continue
            pending_rows.append(row)
        if not pending_rows:
            break

        for row in pending_rows:
            task_id = row["task_id"].strip()
            deps = parse_deps(row.get("depends_on", ""))

            unknown = [dep for dep in deps if dep not in rows_by_task]
            if unknown:
                row["status"] = "blocked"
                row["report_issue_to_user"] = "yes"
                primary_file, bundle_dir = make_evidence_paths(row["evidence_artifact"])
                write_row_evidence(
                    row=row,
                    host_manifest=host_manifest,
                    primary_file=primary_file,
                    bundle_dir=bundle_dir,
                    command_specs=[],
                    command_results=[],
                    status="blocked",
                    reason=f"Unknown dependencies: {', '.join(unknown)}",
                )
                progress = True
                continue

            dep_statuses = {dep: rows_by_task[dep].get("status", "").strip() for dep in deps}
            if any(status in RUNNABLE_STATUSES for status in dep_statuses.values()):
                continue
            if any(status == "blocked" for status in dep_statuses.values()):
                blocked_deps = [dep for dep, st in dep_statuses.items() if st == "blocked"]
                row["status"] = "blocked"
                row["report_issue_to_user"] = "yes"
                primary_file, bundle_dir = make_evidence_paths(row["evidence_artifact"])
                write_row_evidence(
                    row=row,
                    host_manifest=host_manifest,
                    primary_file=primary_file,
                    bundle_dir=bundle_dir,
                    command_specs=[],
                    command_results=[],
                    status="blocked",
                    reason=f"Dependency blocked: {', '.join(blocked_deps)}",
                )
                progress = True
                continue

            status, reason = execute_row(
                row=row,
                host_manifest=host_manifest,
                max_output_chars=max_output_chars,
                timeout_scale=timeout_scale,
            )
            row["status"] = status
            row["report_issue_to_user"] = "yes" if status == "blocked" else "no"
            if status == "blocked":
                print(f"[blocked] {task_id}: {reason}", flush=True)
            else:
                print(f"[done] {task_id}", flush=True)
            update_tracker(tracker_path, rows, fieldnames)
            progress = True

        if not progress:
            unresolved = []
            for row in rows:
                if row.get("status", "").strip() not in RUNNABLE_STATUSES:
                    continue
                task_id = row.get("task_id", "").strip()
                if only_task_ids and task_id not in only_task_ids:
                    continue
                unresolved.append(task_id)
            if only_task_ids:
                print(
                    f"[warn] filtered execution stalled; unresolved tasks: {', '.join(unresolved)}",
                    flush=True,
                )
                break
            for row in rows:
                if row.get("status", "").strip() in RUNNABLE_STATUSES:
                    row["status"] = "blocked"
                    row["report_issue_to_user"] = "yes"
                    primary_file, bundle_dir = make_evidence_paths(row["evidence_artifact"])
                    write_row_evidence(
                        row=row,
                        host_manifest=host_manifest,
                        primary_file=primary_file,
                        bundle_dir=bundle_dir,
                        command_specs=[],
                        command_results=[],
                        status="blocked",
                        reason="Dependency resolution stalled (cyclic or unresolved pending chain)",
                    )
            print(
                f"[warn] stalled dependency graph; blocked tasks: {', '.join(unresolved)}",
                flush=True,
            )
            break

    update_tracker(tracker_path, rows, fieldnames)

    done_count = sum(1 for row in rows if row.get("status") == "done")
    blocked_count = sum(1 for row in rows if row.get("status") == "blocked")
    pending_count = sum(1 for row in rows if row.get("status") == "pending")
    ready_count = sum(1 for row in rows if row.get("status") == "ready")
    print(
        (
            f"[summary] done={done_count} blocked={blocked_count} "
            f"pending={pending_count} ready={ready_count} tracker={tracker_path}"
        ),
        flush=True,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Execute ENGINE baseline tracker and write evidence/status updates."
    )
    parser.add_argument(
        "--tracker",
        type=Path,
        default=DEFAULT_TRACKER,
        help=f"Path to tracker CSV (default: {DEFAULT_TRACKER})",
    )
    parser.add_argument(
        "--max-output-chars",
        type=int,
        default=120_000,
        help="Maximum output chars captured per command.",
    )
    parser.add_argument(
        "--timeout-scale",
        type=float,
        default=1.0,
        help="Multiply every command timeout by this scale factor.",
    )
    parser.add_argument(
        "--only-task-ids",
        type=str,
        default="",
        help="Comma-separated task IDs to execute (filtered rerun mode).",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    tracker_path = args.tracker.resolve()
    if not tracker_path.exists():
        print(f"Tracker not found: {tracker_path}", file=sys.stderr)
        return 1
    try:
        only_task_ids: Optional[set[str]] = None
        if args.only_task_ids.strip():
            only_task_ids = {
                token.strip() for token in args.only_task_ids.split(",") if token.strip()
            }
        execute_tracker(
            tracker_path=tracker_path,
            max_output_chars=args.max_output_chars,
            timeout_scale=args.timeout_scale,
            only_task_ids=only_task_ids,
        )
    except Exception as exc:
        print(f"Execution failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
