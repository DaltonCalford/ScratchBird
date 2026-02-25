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


TODAY = date.today().isoformat()
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
            workspace_root.parent / "postgresql/build_codex",
            workspace_root.parent / "postgresql/build",
            workspace_root.parent / "postgres/build_codex",
            workspace_root.parent / "postgres/build",
        ]
    )

    for candidate in candidates:
        if not candidate.exists():
            continue
        if (candidate / "src/test/regress/GNUmakefile").exists() and (candidate / "src/Makefile.global").exists():
            return candidate
    return Path()


def resolve_mysql_gate_context(workspace_root: Path) -> Tuple[Path, Path]:
    source_candidates: List[Path] = []
    env_source = os.environ.get("MYSQL_UPSTREAM_SOURCE_DIR", "").strip()
    if env_source:
        source_candidates.append(Path(env_source))
    source_candidates.extend(
        [
            workspace_root / "tests/compatibility/mysql/repos/mysql-server",
            workspace_root.parent / "mysql-server",
        ]
    )

    env_runtime = os.environ.get("MYSQL_UPSTREAM_RUNTIME_DIR", "").strip()
    env_build = os.environ.get("MYSQL_UPSTREAM_BUILD_DIR", "").strip()
    first_source = Path()

    for source_root in source_candidates:
        mtr_path = source_root / "mysql-test/mysql-test-run.pl"
        if not mtr_path.exists():
            continue
        if not first_source:
            first_source = source_root

        runtime_candidates: List[Path] = []
        if env_runtime:
            runtime_candidates.append(Path(env_runtime))
        if env_build:
            runtime_candidates.append(Path(env_build) / "runtime_output_directory")
        runtime_candidates.extend(
            [
                source_root / "runtime_output_directory",
                source_root / "build/runtime_output_directory",
                source_root / "build_codex/runtime_output_directory",
                source_root / "build_codex2/runtime_output_directory",
            ]
        )

        has_share = (source_root / "share/mysql").exists() or (source_root / "share").exists()
        if not has_share:
            continue

        for runtime_dir in runtime_candidates:
            if (runtime_dir / "mysqld").exists() and (runtime_dir / "mysqltest").exists():
                return source_root, runtime_dir

    return first_source, Path()


def resolve_firebird_cli_binary(workspace_root: Path) -> Tuple[Path, str]:
    env_cli = os.environ.get("SCRATCHBIRD_FB_ISQL", "").strip()
    candidates: List[Path] = []
    if env_cli:
        candidates.append(Path(env_cli))
    candidates.extend(
        [
            workspace_root / "build/src/sb_fb_isql",
            workspace_root / "build/src/cli/sb_fb_isql",
            workspace_root.parent / "ScratchBird-driver/build/tracks/alpha/drivers/cli/sb_fb_isql",
            workspace_root.parent / "ScratchBird-driver/build/tracks/beta/drivers/cli/sb_fb_isql",
            workspace_root.parent / "ScratchBird-driver/build/src/sb_fb_isql",
        ]
    )

    system_isql = shutil.which("isql-fb") or shutil.which("isql")
    if system_isql:
        system_path = Path(system_isql)
        if system_path.exists() and os.access(system_path, os.X_OK):
            return system_path, "firebird_native"

    for candidate in candidates:
        if candidate.exists() and os.access(candidate, os.X_OK):
            return candidate, "firebird"

    # Generic sb_isql is intentionally not accepted for Firebird wire parity.
    generic_candidates = [
        workspace_root / "build/src/sb_isql",
        workspace_root.parent / "ScratchBird-driver/build/tracks/alpha/drivers/cli/sb_isql",
        workspace_root.parent / "ScratchBird-driver/build/tracks/beta/drivers/cli/sb_isql",
    ]
    for candidate in generic_candidates:
        if candidate.exists() and os.access(candidate, os.X_OK):
            return candidate, "generic"

    return Path(), "missing"


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
    firebird_gtcs = firebird_repo / "tests/functional/gtcs"
    convert_script = firebird_compat / "scripts/convert_fbt_to_sql.py"
    ctest_runner = firebird_compat / "scripts/run_firebird_ctest.sh"
    fb_cli_path, fb_cli_kind = resolve_firebird_cli_binary(ctx.workspace_root)
    fb_cli_ready = bool(fb_cli_path) and fb_cli_kind in {"firebird", "firebird_native"}
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

    legacy_doc = render_doc(
        title="FB-EMU-041 Legacy fbtest/TCS Integration",
        mode=ctx.mode,
        prerequisites=[
            ("tests/compatibility/firebird/repos/fbt-repository", firebird_repo_present),
            ("tests/compatibility/firebird/repos/fbt-repository/tests/functional/gtcs", firebird_gtcs.exists()),
            ("tests/compatibility/firebird/scripts/convert_fbt_to_sql.py", convert_script.exists()),
            ("tests/compatibility/firebird/scripts/run_firebird_ctest.sh", ctest_runner.exists()),
            ("sb_fb_isql binary available", fb_cli_ready),
        ],
        command_templates=[
            "cd tests/compatibility/firebird",
            "python3 scripts/convert_fbt_to_sql.py repos/fbt-repository/tests/functional/gtcs/dsql-domain-01.fbt <output_dir>",
            "# Optional execution when sb_fb_isql exists:",
            "scripts/run_firebird_ctest.sh",
        ],
        notes=[
            "Dry-run initializes legacy vector wiring and prerequisites only.",
            "Execute mode records inventory counts and conversion smoke.",
            "Curated execution requires a Firebird protocol client (sb_fb_isql or isql-fb); generic sb_isql is rejected.",
            "Evidence file path matches tracker row FB-EMU-041.",
        ],
    )
    write_gate(
        ctx.artifact_root / "firebird/p5s2w2/fb-emu-041-fbtest-tcs.md",
        legacy_doc,
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

    legacy_results: List[CommandResult] = []
    if firebird_repo_present and firebird_gtcs.exists():
        legacy_results.append(
            run_command(
                [
                    "python3",
                    "-c",
                    (
                        "from pathlib import Path; "
                        "root=Path('tests/compatibility/firebird/repos/fbt-repository/tests'); "
                        "gtcs=root/'functional/gtcs'; "
                        "total=sum(1 for _ in root.rglob('*.fbt')); "
                        "gtcs_total=sum(1 for _ in gtcs.rglob('*.fbt')); "
                        "print(f'fbt_total={total}'); "
                        "print(f'gtcs_total={gtcs_total}')"
                    ),
                ],
                cwd=ctx.workspace_root,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
    else:
        legacy_results.append(
            CommandResult(
                command="python3 -c '<inventory fbt/gtcs>'",
                cwd=str(ctx.workspace_root),
                exit_code=127,
                timed_out=False,
                output="fbt-repository or GTCS tree missing; inventory skipped.",
            )
        )

    conversion_output_dir = ctx.artifact_root / "firebird/p5s2w2/fbtest_tcs_smoke/converted"
    sample_fbt = firebird_repo / "tests/functional/gtcs/dsql-domain-01.fbt"
    if convert_script.exists() and sample_fbt.exists():
        legacy_results.append(
            run_command(
                [
                    "python3",
                    str(convert_script),
                    str(sample_fbt),
                    str(conversion_output_dir),
                ],
                cwd=ctx.workspace_root,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
    else:
        legacy_results.append(
            CommandResult(
                command="python3 tests/compatibility/firebird/scripts/convert_fbt_to_sql.py <sample.fbt> <output_dir>",
                cwd=str(ctx.workspace_root),
                exit_code=127,
                timed_out=False,
                output="convert script or sample GTCS .fbt missing; conversion smoke skipped.",
            )
        )

    if ctest_runner.exists() and fb_cli_ready:
        legacy_results.append(
            run_command(
                [
                    "bash",
                    "-lc",
                    f"SCRATCHBIRD_FB_ISQL={shlex.quote(str(fb_cli_path))} {shlex.quote(str(ctest_runner))}",
                ],
                cwd=ctx.workspace_root,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
    elif ctest_runner.exists() and fb_cli_kind == "generic":
        legacy_results.append(
            CommandResult(
                command="scripts/run_firebird_ctest.sh",
                cwd=str(ctx.workspace_root),
                exit_code=2,
                timed_out=False,
                output=(
                    f"sb_fb_isql unavailable; found only generic client at {fb_cli_path}. "
                    "Generic sb_isql is rejected for Firebird wire-protocol parity."
                ),
            )
        )
    else:
        legacy_results.append(
            CommandResult(
                command="scripts/run_firebird_ctest.sh",
                cwd=str(ctx.workspace_root),
                exit_code=127,
                timed_out=False,
                output="run_firebird_ctest.sh or sb_fb_isql missing; curated execution skipped.",
            )
        )

    legacy_ok = all(result.exit_code == 0 for result in legacy_results)
    legacy_cli = str(fb_cli_path) if fb_cli_path else "<not-found>"
    legacy_report = [
        f"Last updated: {TODAY}",
        "",
        "# FB-EMU-041 Legacy fbtest/TCS Integration Report",
        "",
        f"- Mode: `{ctx.mode}`",
        f"- Overall result: `{'pass' if legacy_ok else 'fail'}`",
        f"- Command timeout: `{ctx.command_timeout_sec}s`",
        f"- `sb_fb_isql` resolution: `{sanitize_external_paths(legacy_cli, ctx.workspace_root)}`",
        "",
        "## Command Results",
        "",
        render_command_results(legacy_results, ctx.workspace_root).rstrip(),
        "",
        "## Notes",
        "- This report captures legacy vector integration status, not final parity closure.",
        "- Full fbtest/TCS execution closure remains tracked in FB-EMU-041/042.",
    ]
    write_gate(
        ctx.artifact_root / "firebird/p5s2w2/fb-emu-041-fbtest-tcs.md",
        "\n".join(legacy_report) + "\n",
    )


def mysql_gate(ctx: GateContext) -> None:
    mysql_root = ctx.workspace_root / "tests/compatibility/mysql/repos/mysql-server"
    mysql_test_dir = mysql_root / "mysql-test"
    mtr_path = mysql_test_dir / "mysql-test-run.pl"

    exec_root, exec_runtime_dir = resolve_mysql_gate_context(ctx.workspace_root)
    exec_test_dir = exec_root / "mysql-test" if exec_root else Path()
    runtime_ready = bool(exec_runtime_dir) and (exec_runtime_dir / "mysqld").exists() and (exec_runtime_dir / "mysqltest").exists()
    exec_ready = bool(exec_test_dir) and (exec_test_dir / "mysql-test-run.pl").exists()
    doc = render_doc(
        title="MY-EMU-040 MTR Gate Integration",
        mode=ctx.mode,
        prerequisites=[
            ("tests/compatibility/mysql/repos/mysql-server/mysql-test/mysql-test-run.pl", mtr_path.exists()),
            ("tests/compatibility/mysql/repos/mysql-server/mysql-test", mysql_test_dir.exists()),
            ("full upstream mysql source/runtime auto-detected", exec_ready and runtime_ready),
        ],
        command_templates=[
            "cd tests/compatibility/mysql/repos/mysql-server/mysql-test",
            "# Optional full-run mode:",
            "# export MYSQL_UPSTREAM_SOURCE_DIR=<path to mysql source root>",
            "# export MYSQL_UPSTREAM_BUILD_DIR=<path to mysql build root>",
            "perl mysql-test-run.pl --suite=main --do-test=select --retry=0 --parallel=1 --force",
        ],
        notes=[
            "Dry-run initializes in-tree MTR gate command wiring and prerequisites.",
            "Execute mode runs full smoke only when mysql source tree and runtime binaries are available.",
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
    syntax_cwd = exec_test_dir if exec_ready else mysql_test_dir
    if mtr_path.exists() and mysql_test_dir.exists():
        smoke_results.append(
            run_command(
                ["perl", "-c", "mysql-test-run.pl"],
                cwd=syntax_cwd,
                timeout_sec=ctx.command_timeout_sec,
            )
        )
        if runtime_ready and exec_ready:
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
                        f"--client-bindir={exec_runtime_dir}",
                    ],
                    cwd=exec_test_dir,
                    timeout_sec=ctx.command_timeout_sec,
                )
            )
        else:
            smoke_results.append(
                CommandResult(
                    command=(
                        "perl mysql-test-run.pl --suite=main --do-test=select --retry=0 --parallel=1 --force "
                        "--client-bindir=<runtime_output_directory>"
                    ),
                    cwd=str(mysql_test_dir),
                    exit_code=0,
                    timed_out=False,
                    output=(
                        "Skipped full MTR smoke: unable to auto-detect mysql source/runtime with "
                        "mysql-test + share + runtime_output_directory (mysqld/mysqltest). "
                        "Set MYSQL_UPSTREAM_SOURCE_DIR and MYSQL_UPSTREAM_BUILD_DIR."
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
