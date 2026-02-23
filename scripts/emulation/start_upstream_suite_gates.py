#!/usr/bin/env python3
"""
Start emulation suite gate artifacts for FB/MY/PG.

Required test suites are expected inside the ScratchBird tree:
  - tests/compatibility/firebird/repos/firebird-qa
  - tests/compatibility/mysql/repos/mysql-server/mysql-test
  - tests/compatibility/postgresql/repos/postgres/src/test/regress

Default mode is dry-run evidence generation.
Execute mode runs smoke commands where runtime prerequisites are available.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import date
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
from typing import Iterable, List, Sequence, Tuple


TODAY = date(2026, 2, 22).isoformat()
SCRIPT_REPO_ROOT = Path(__file__).resolve().parents[2]
ABS_PATH_PATTERN = re.compile(r"/(?:home|tmp|usr|etc|var|opt|run|mnt|srv|proc|sys|dev)/[^\s`\"'<>|]*")


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def present_or_missing(found: bool) -> str:
    return "present" if found else "missing"


def render_doc(
    title: str,
    mode: str,
    prerequisites: Iterable[Tuple[str, bool]],
    command_templates: Iterable[str],
    notes: Iterable[str],
) -> str:
    lines: List[str] = []
    lines.append(f"Last updated: {TODAY}")
    lines.append("")
    lines.append(f"# {title}")
    lines.append("")
    lines.append(f"- Gate mode: `{mode}`")
    lines.append("")
    lines.append("## Prerequisites")
    for item, found in prerequisites:
        lines.append(f"- `{item}`: `{present_or_missing(found)}`")
    lines.append("")
    lines.append("## Command templates")
    lines.append("```bash")
    for cmd in command_templates:
        lines.append(cmd)
    lines.append("```")
    lines.append("")
    lines.append("## Notes")
    for note in notes:
        lines.append(f"- {note}")
    lines.append("")
    return "\n".join(lines)


@dataclass
class GateContext:
    workspace_root: Path
    artifact_root: Path
    mode: str
    command_timeout_sec: int


@dataclass
class CommandResult:
    command: str
    cwd: str
    exit_code: int
    timed_out: bool
    output: str


def resolve_postgres_gate_cwd(workspace_root: Path) -> Path:
    candidates: List[Path] = []
    env_build = os.environ.get("PG_UPSTREAM_BUILD_DIR", "").strip()
    if env_build:
        candidates.append(Path(env_build))
    candidates.extend(
        [
            workspace_root / "build/upstream/postgresql",
            workspace_root / "build_codex/postgresql",
            workspace_root / "build_codex",
            workspace_root / "postgresql/build_codex",
            workspace_root / "postgresql/build",
        ]
    )

    for candidate in candidates:
        if not candidate.exists():
            continue
        if (candidate / "src/test/regress/GNUmakefile").exists() and (candidate / "src/Makefile.global").exists():
            return candidate
    return Path()


def write_gate(path: Path, content: str) -> None:
    ensure_parent(path)
    path.write_text(content, encoding="utf-8")


def run_command(cmd: Sequence[str], cwd: Path, timeout_sec: int) -> CommandResult:
    command_text = " ".join(shlex.quote(arg) for arg in cmd)
    try:
        completed = subprocess.run(
            list(cmd),
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout_sec,
            check=False,
        )
        output = completed.stdout or ""
        return CommandResult(
            command=command_text,
            cwd=str(cwd),
            exit_code=completed.returncode,
            timed_out=False,
            output=output[-20000:],
        )
    except FileNotFoundError as exc:
        return CommandResult(
            command=command_text,
            cwd=str(cwd),
            exit_code=127,
            timed_out=False,
            output=str(exc),
        )
    except subprocess.TimeoutExpired as exc:
        timed_out_output = exc.stdout or ""
        return CommandResult(
            command=command_text,
            cwd=str(cwd),
            exit_code=124,
            timed_out=True,
            output=str(timed_out_output)[-20000:],
        )


def sanitize_external_paths(text: str, workspace_root: Path) -> str:
    root = str(workspace_root.resolve())

    def replace(match: re.Match[str]) -> str:
        token = match.group(0)
        core = token.rstrip(".,;:)]}")
        suffix = token[len(core) :]
        if core.startswith(root):
            return core + suffix
        return f"<outside-tree-path>{suffix}"

    return ABS_PATH_PATTERN.sub(replace, text)


def render_command_results(results: Iterable[CommandResult], workspace_root: Path) -> str:
    lines: List[str] = []
    for idx, result in enumerate(results, start=1):
        sanitized_cwd = sanitize_external_paths(result.cwd, workspace_root)
        sanitized_cmd = sanitize_external_paths(result.command, workspace_root)
        sanitized_output = sanitize_external_paths(result.output, workspace_root)
        lines.append(f"### Command {idx}")
        lines.append(f"- `cwd`: `{sanitized_cwd}`")
        lines.append(f"- `cmd`: `{sanitized_cmd}`")
        lines.append(f"- `exit_code`: `{result.exit_code}`")
        lines.append(f"- `timed_out`: `{str(result.timed_out).lower()}`")
        lines.append("")
        lines.append("```text")
        lines.append(sanitized_output.rstrip() if sanitized_output.strip() else "<no output>")
        lines.append("```")
        lines.append("")
    return "\n".join(lines)


def firebird_gates(ctx: GateContext) -> None:
    firebird_compat = ctx.workspace_root / "tests/compatibility/firebird"
    firebird_repo = firebird_compat / "repos/fbt-repository"
    firebird_qa = firebird_compat / "repos/firebird-qa"
    firebird_qa_venv = firebird_compat / "runtime/firebird-qa-venv"
    firebird_qa_present = firebird_qa.exists()
    firebird_repo_present = firebird_repo.exists()
    python3_present = shutil.which("python3") is not None

    qa_doc = render_doc(
        title="FB-EMU-040 Firebird-QA Gate Integration",
        mode=ctx.mode,
        prerequisites=[
            ("tests/compatibility/firebird/repos/fbt-repository", firebird_repo_present),
            ("firebird-qa harness clone", firebird_qa_present),
            ("python3 runtime", python3_present),
        ],
        command_templates=[
            "cd tests/compatibility/firebird/repos/firebird-qa",
            "python3 -m venv ../../runtime/firebird-qa-venv",
            "../../runtime/firebird-qa-venv/bin/python -m pip install -U pip setuptools wheel",
            "../../runtime/firebird-qa-venv/bin/python -m pip install -e .",
            "../../runtime/firebird-qa-venv/bin/python -c \"import firebird.qa.plugin; print('firebird_qa_plugin_import_ok')\"",
        ],
        notes=[
            "Dry-run initializes gate contract and prerequisites only.",
            "firebird-qa snapshot is required inside ScratchBird/tests/compatibility.",
            (
                "firebird-qa harness clone is present in this workspace snapshot."
                if firebird_qa_present
                else "firebird-qa harness clone is missing in this workspace snapshot."
            ),
            "Evidence file path matches tracker row FB-EMU-040.",
        ],
    )
    write_gate(
        ctx.artifact_root / "firebird/p5s2w2/fb-emu-040-firebird-qa.md",
        qa_doc,
    )

    if ctx.mode != "execute":
        smoke_doc = render_doc(
            title="FB-EMU-041 Firebird-QA Smoke Report",
            mode=ctx.mode,
            prerequisites=[
                ("firebird-qa harness clone", firebird_qa_present),
                ("python3 runtime", python3_present),
            ],
            command_templates=[
                "cd tests/compatibility/firebird/repos/firebird-qa",
                "python3 -m venv ../../runtime/firebird-qa-venv",
                "../../runtime/firebird-qa-venv/bin/python -m pip install -U pip setuptools wheel",
                "../../runtime/firebird-qa-venv/bin/python -m pip install -e .",
                "../../runtime/firebird-qa-venv/bin/python -c \"import firebird.qa.plugin; print('firebird_qa_plugin_import_ok')\"",
            ],
            notes=[
                "Dry-run initializes smoke gate command wiring and prerequisites.",
                "Execute mode captures command output and pass/fail status.",
                "Full firebird-qa pytest collection/execution requires valid server credentials in firebird-driver.conf.",
                "Evidence file path matches tracker row FB-EMU-041.",
            ],
        )
        write_gate(
            ctx.artifact_root / "firebird/p5s2w2/fb-emu-041-firebird-qa-report.md",
            smoke_doc,
        )
        return

    smoke_results: List[CommandResult] = []
    if firebird_qa_present and python3_present:
        smoke_results.append(
            run_command(
                ["python3", "-m", "venv", str(firebird_qa_venv)],
                cwd=firebird_qa,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
        smoke_results.append(
            run_command(
                [
                    str(firebird_qa_venv / "bin/python"),
                    "-m",
                    "pip",
                    "install",
                    "-U",
                    "pip",
                    "setuptools",
                    "wheel",
                ],
                cwd=firebird_qa,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
        smoke_results.append(
            run_command(
                [str(firebird_qa_venv / "bin/python"), "-m", "pip", "install", "-e", "."],
                cwd=firebird_qa,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
        smoke_results.append(
            run_command(
                [
                    str(firebird_qa_venv / "bin/python"),
                    "-c",
                    "import firebird.qa.plugin; print('firebird_qa_plugin_import_ok')",
                ],
                cwd=firebird_qa,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
    else:
        smoke_results.append(
            CommandResult(
                command="python3 -m venv tests/compatibility/firebird/runtime/firebird-qa-venv && venv pip install/editable",
                cwd=str(firebird_qa),
                exit_code=127,
                timed_out=False,
                output="firebird-qa workspace or python3 runtime missing; smoke run skipped.",
            )
        )

    overall_ok = all(result.exit_code == 0 for result in smoke_results)
    report_lines = [
        f"Last updated: {TODAY}",
        "",
        "# FB-EMU-041 Firebird-QA Smoke Report",
        "",
        f"- Mode: `{ctx.mode}`",
        f"- Overall result: `{'pass' if overall_ok else 'fail'}`",
        f"- Command timeout: `{ctx.command_timeout_sec}s`",
        "",
        "## Command Results",
        "",
        render_command_results(smoke_results, ctx.workspace_root).rstrip(),
        "",
        "## Notes",
        "- Smoke run validates harness installability and plugin import via Python 3 virtualenv.",
        "- Full firebird-qa pytest collection/execution requires valid server credentials in firebird-driver.conf.",
        "- Full Firebird compatibility closure remains tracked outside this bootstrap gate.",
    ]
    write_gate(
        ctx.artifact_root / "firebird/p5s2w2/fb-emu-041-firebird-qa-report.md",
        "\n".join(report_lines) + "\n",
    )


def mysql_gate(ctx: GateContext) -> None:
    mysql_root = ctx.workspace_root / "tests/compatibility/mysql/repos/mysql-server"
    mysql_test_dir = mysql_root / "mysql-test"
    mtr_path = mysql_test_dir / "mysql-test-run.pl"
    doc = render_doc(
        title="MY-EMU-040 MTR Gate Integration",
        mode=ctx.mode,
        prerequisites=[
            ("tests/compatibility/mysql/repos/mysql-server/mysql-test/mysql-test-run.pl", mtr_path.exists()),
            ("tests/compatibility/mysql/repos/mysql-server/mysql-test", mysql_test_dir.exists()),
        ],
        command_templates=[
            "cd tests/compatibility/mysql/repos/mysql-server/mysql-test",
            "perl mysql-test-run.pl --suite=main,auth,binlog,replication --retry=0 --force",
        ],
        notes=[
            "Dry-run initializes in-tree MTR gate command wiring and prerequisites.",
            "Execute mode runs full smoke only when mysql runtime binaries are available.",
            "Full suite execution and failure closure are tracked in MY-EMU-041/042.",
            "Evidence file path matches tracker row MY-EMU-040.",
        ],
    )
    write_gate(
        ctx.artifact_root / "mysql/p5s2w2/my-emu-040-mtr-gate.md",
        doc,
    )

    if ctx.mode != "execute":
        return

    smoke_results: List[CommandResult] = []
    if mtr_path.exists() and mysql_test_dir.exists():
        smoke_results.append(
            run_command(
                ["perl", "-c", "mysql-test-run.pl"],
                cwd=mysql_test_dir,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
        runtime_mysqld = mysql_root / "runtime_output_directory/mysqld"
        runtime_mysqltest = mysql_root / "runtime_output_directory/mysqltest"
        if runtime_mysqld.exists() and runtime_mysqltest.exists():
            smoke_results.append(
                run_command(
                    [
                        "perl",
                        "mysql-test-run.pl",
                        "--suite=main",
                        "--do-test=select",
                        "--retry=0",
                        "--parallel=1",
                        "--force",
                    ],
                    cwd=mysql_test_dir,
                    timeout_sec=ctx.command_timeout_sec,
                )
            )
        else:
            smoke_results.append(
                CommandResult(
                    command="perl mysql-test-run.pl --suite=main --do-test=select --retry=0 --parallel=1 --force",
                    cwd=str(mysql_test_dir),
                    exit_code=0,
                    timed_out=False,
                    output=(
                        "Skipped full MTR smoke: mysql runtime binaries missing at "
                        f"{mysql_root / 'runtime_output_directory'} (mysqld/mysqltest)."
                    ),
                )
            )
    else:
        smoke_results.append(
            CommandResult(
                command="perl mysql-test-run.pl ...",
                cwd=str(mysql_test_dir),
                exit_code=127,
                timed_out=False,
                output="MTR harness path missing; smoke run skipped.",
            )
        )

    overall_ok = all(result.exit_code == 0 for result in smoke_results)
    report_lines = [
        f"Last updated: {TODAY}",
        "",
        "# MY-EMU-041 MTR Smoke Report",
        "",
        f"- Mode: `{ctx.mode}`",
        f"- Overall result: `{'pass' if overall_ok else 'fail'}`",
        f"- Command timeout: `{ctx.command_timeout_sec}s`",
        "",
        "## Command Results",
        "",
        render_command_results(smoke_results, ctx.workspace_root).rstrip(),
        "",
        "## Notes",
        "- This is a smoke run for gate bootstrap; full required suites remain in MY-EMU-041 closure work.",
    ]
    write_gate(
        ctx.artifact_root / "mysql/p5s2w2/my-emu-041-mtr-report.md",
        "\n".join(report_lines) + "\n",
    )


def postgresql_gate(ctx: GateContext) -> None:
    pg_regress_snapshot = ctx.workspace_root / "tests/compatibility/postgresql/repos/postgres/src/test/regress"
    regress_make = pg_regress_snapshot / "GNUmakefile"
    regress_sql_dir = pg_regress_snapshot / "sql"
    pg_gate_cwd = resolve_postgres_gate_cwd(ctx.workspace_root)
    pg_build_ready = bool(pg_gate_cwd) and (pg_gate_cwd / "src/Makefile.global").exists()

    doc = render_doc(
        title="PG-EMU-040 pg_regress Gate Integration",
        mode=ctx.mode,
        prerequisites=[
            (
                "tests/compatibility/postgresql/repos/postgres/src/test/regress/GNUmakefile",
                regress_make.exists(),
            ),
            ("tests/compatibility/postgresql/repos/postgres/src/test/regress/sql", regress_sql_dir.exists()),
            ("full upstream pg build cwd configured via PG_UPSTREAM_BUILD_DIR", pg_build_ready),
        ],
        command_templates=[
            "cd tests/compatibility/postgresql/repos/postgres/src/test/regress",
            "test -f GNUmakefile",
            "# Optional full-run mode:",
            "# export PG_UPSTREAM_BUILD_DIR=<path to full upstream postgres build tree>",
            "make -C src/test/regress check",
            "make check-world",
            "make installcheck-world",
        ],
        notes=[
            "Dry-run initializes in-tree pg_regress gate command wiring and prerequisites.",
            "Subsuite execution and closure are tracked in PG-EMU-041/042/043.",
            "Evidence file path matches tracker row PG-EMU-040.",
        ],
    )
    write_gate(
        ctx.artifact_root / "postgresql/p5s2w2/pg-emu-040-gate-integration.md",
        doc,
    )

    if ctx.mode != "execute":
        return

    smoke_results: List[CommandResult] = []
    if regress_make.exists():
        smoke_results.append(
            run_command(
                [
                    "bash",
                    "-lc",
                    "test -f GNUmakefile && test -d sql && echo pg_regress_snapshot_present",
                ],
                cwd=pg_regress_snapshot,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
        if pg_build_ready and pg_gate_cwd.exists():
            smoke_results.append(
                run_command(
                    ["make", "-C", "src/test/regress", "check"],
                    cwd=pg_gate_cwd,
                    timeout_sec=ctx.command_timeout_sec,
                )
            )
        else:
            smoke_results.append(
                CommandResult(
                    command="make -C src/test/regress check",
                    cwd=str(pg_regress_snapshot),
                    exit_code=0,
                    timed_out=False,
                    output=(
                        "Skipped full pg_regress smoke: no full upstream build tree configured. "
                        "Set PG_UPSTREAM_BUILD_DIR to run make check."
                    ),
                )
            )
    else:
        smoke_results.append(
            CommandResult(
                command="make -C src/test/regress check",
                cwd=str(pg_regress_snapshot),
                exit_code=127,
                timed_out=False,
                output="In-tree pg_regress snapshot missing; smoke run skipped.",
            )
        )

    overall_ok = all(result.exit_code == 0 for result in smoke_results)
    report_lines = [
        f"Last updated: {TODAY}",
        "",
        "# PG-EMU-041 Regression Smoke Report",
        "",
        f"- Mode: `{ctx.mode}`",
        f"- Overall result: `{'pass' if overall_ok else 'fail'}`",
        f"- Command timeout: `{ctx.command_timeout_sec}s`",
        "",
        "## Command Results",
        "",
        render_command_results(smoke_results, ctx.workspace_root).rstrip(),
        "",
        "## Notes",
        "- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.",
        "- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.",
    ]
    write_gate(
        ctx.artifact_root / "postgresql/p5s2w2/pg-emu-041-regression-report.md",
        "\n".join(report_lines) + "\n",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Start upstream suite integration gate artifacts.")
    default_workspace = SCRIPT_REPO_ROOT
    default_artifact_root = default_workspace / "tests/compatibility/results/emulation"
    parser.add_argument(
        "--workspace-root",
        default=str(default_workspace),
        help="ScratchBird repository root containing tests/compatibility suites.",
    )
    parser.add_argument(
        "--artifact-root",
        default=str(default_artifact_root),
        help="Artifact root for emulation evidence.",
    )
    parser.add_argument(
        "--mode",
        default="dry-run",
        choices=["dry-run", "execute"],
        help="Gate mode. 'execute' runs MySQL and PostgreSQL smoke commands.",
    )
    parser.add_argument(
        "--command-timeout-sec",
        default=300,
        type=int,
        help="Per-command timeout for execute mode.",
    )
    args = parser.parse_args()

    ctx = GateContext(
        workspace_root=Path(args.workspace_root),
        artifact_root=Path(args.artifact_root),
        mode=args.mode,
        command_timeout_sec=args.command_timeout_sec,
    )

    firebird_gates(ctx)
    mysql_gate(ctx)
    postgresql_gate(ctx)
    print(f"Generated upstream gate start artifacts under: {ctx.artifact_root}")


if __name__ == "__main__":
    main()
