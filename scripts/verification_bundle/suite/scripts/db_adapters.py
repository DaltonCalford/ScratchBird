#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import signal
import shutil
import subprocess
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Mapping, Tuple


@dataclass
class AdapterResult:
    status: str
    returncode: int
    sqlstate: str
    message: str
    stdout: str
    stderr: str
    elapsed_ms: int
    assert_lines: List[str]
    stdout_path: str
    stderr_path: str


def resolve_template(value: str, repo_root: Path) -> str:
    return value.replace("{repo_root}", str(repo_root))


def resolve_candidate_binary(candidates: List[str], repo_root: Path) -> str:
    for candidate in candidates:
        if not candidate:
            continue
        expanded = resolve_template(str(candidate), repo_root)
        p = Path(expanded)
        if p.is_file() and os.access(str(p), os.X_OK):
            return str(p)
        found = shutil.which(expanded)
        if found:
            return found
    raise RuntimeError("No runnable binary found in candidate list")


def resolve_connect(engine: Mapping[str, object]) -> Mapping[str, object]:
    connect: Dict[str, object] = dict(engine.get("connect", {}) or {})
    overrides: Mapping[str, object] = engine.get("env_overrides", {}) or {}
    for key, env_var in overrides.items():
        if not isinstance(env_var, str):
            continue
        env_val = os.environ.get(env_var)
        if env_val is not None and env_val != "":
            connect[key] = env_val
    return connect


def resolve_client_binary(engine: Mapping[str, object], repo_root: Path) -> str:
    candidates = list(engine.get("client_candidates", []) or [])
    if not candidates:
        client = str(engine.get("client", "")).strip()
        if client:
            candidates.append(client)
    try:
        return resolve_candidate_binary(candidates, repo_root)
    except RuntimeError as exc:
        raise RuntimeError(f"No runnable client found for engine: {engine.get('id')}") from exc


def resolve_sql_path(sql_ref: str, workspace_root: Path) -> Path:
    p = Path(sql_ref)
    if p.is_absolute():
        return p
    return (workspace_root / p).resolve()


def _extract_sqlstate(combined: str) -> str:
    m = re.search(r"\(([0-9A-Z]{5})\)", combined)
    if m:
        return m.group(1)
    m = re.search(r"\b([0-9A-Z]{5})\b", combined)
    if m:
        return m.group(1)
    return "00000"


def _extract_assert_lines(combined: str) -> List[str]:
    out: List[str] = []
    for raw in combined.splitlines():
        if "ASSERT|" not in raw:
            continue
        idx = raw.find("ASSERT|")
        payload = raw[idx:].strip()
        payload = re.sub(r"\s*\|$", "", payload)
        payload = re.sub(r"\s+$", "", payload)
        if payload:
            out.append(payload)
    return out


def _run_subprocess(
    cmd: List[str],
    env: Mapping[str, str],
    stdout_path: Path,
    stderr_path: Path,
    timeout_seconds: int,
    stdin_text: str | None = None,
) -> Tuple[int, str, str, int]:
    start = time.time()
    proc: subprocess.Popen[str] | None = None

    def _coerce_text(value: str | bytes | None) -> str:
        if value is None:
            return ""
        if isinstance(value, bytes):
            return value.decode("utf-8", errors="ignore")
        return value

    try:
        proc = subprocess.Popen(
            cmd,
            text=True,
            env=dict(env),
            stdin=subprocess.PIPE if stdin_text is not None else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        stdout_value, stderr_value = proc.communicate(input=stdin_text, timeout=timeout_seconds)
        rc = proc.returncode if proc.returncode is not None else 0
        stdout_text = _coerce_text(stdout_value)
        stderr_text = _coerce_text(stderr_value)
    except subprocess.TimeoutExpired as exc:
        if proc is not None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout_value, stderr_value = proc.communicate()
        else:
            stdout_value, stderr_value = exc.stdout, exc.stderr
        rc = 124
        stdout_text = _coerce_text(stdout_value if stdout_value is not None else exc.stdout)
        stderr_text = _coerce_text(stderr_value if stderr_value is not None else exc.stderr)
    elapsed_ms = int((time.time() - start) * 1000)
    stdout_path.write_text(stdout_text, encoding="utf-8", errors="ignore")
    stderr_path.write_text(stderr_text, encoding="utf-8", errors="ignore")
    return rc, stdout_text, stderr_text, elapsed_ms


def _firebird_database(connect: Mapping[str, object]) -> str:
    database = str(connect.get("database", "")).strip()
    host = str(connect.get("host", "")).strip()
    port = str(connect.get("port", "")).strip()
    if host and database:
        # Keep explicitly remote strings as-is:
        #   host:path
        #   host/port:path
        # Avoid rewriting Windows drive paths (C:\\path\\file.fdb).
        windows_drive = re.match(r"^[A-Za-z]:[\\\\/]", database) is not None
        already_remote = (":" in database and not database.startswith("/") and not windows_drive)
        if not already_remote:
            if port:
                return f"{host}/{port}:{database}"
            return f"{host}:{database}"
    return database


def run_sql_file(
    engine: Mapping[str, object],
    binary: str,
    sql_file: Path,
    output_dir: Path,
    stage: str,
    timeout_seconds: int,
) -> AdapterResult:
    connect = resolve_connect(engine)
    client = str(engine.get("client", "")).lower()
    env = dict(os.environ)
    run_id = uuid.uuid4().hex[:12]
    stdout_path = output_dir / f"{engine.get('id')}.{stage}.{run_id}.stdout.txt"
    stderr_path = output_dir / f"{engine.get('id')}.{stage}.{run_id}.stderr.txt"
    output_dir.mkdir(parents=True, exist_ok=True)

    if client == "sb_isql":
        host = str(connect.get("host", "127.0.0.1"))
        port = str(connect.get("port", "16092"))
        database = str(connect.get("database", "main"))
        user = str(connect.get("user", "SysArch"))
        password = str(connect.get("password", ""))
        mode = str(connect.get("mode", "local-ipc"))
        ipc_method = str(connect.get("ipc_method", "tcp"))
        sslmode = str(connect.get("sslmode", "disable"))
        out_file = output_dir / f"{engine.get('id')}.{stage}.{run_id}.query.out"
        cmd = [
            binary,
            database,
            f"--mode={mode}",
            f"--ipc-method={ipc_method}",
            f"--sslmode={sslmode}",
            "-H",
            host,
            "-p",
            port,
            "-U",
            user,
            "-P",
            password,
            "-b",
            "-f",
            str(sql_file),
            "-o",
            str(out_file),
            "-q",
        ]
        rc, stdout_text, stderr_text, elapsed = _run_subprocess(cmd, env, stdout_path, stderr_path, timeout_seconds)
        query_out = out_file.read_text(encoding="utf-8", errors="ignore") if out_file.exists() else ""
        combined = f"{query_out}\n{stdout_text}\n{stderr_text}"
    elif client == "scratchbird":
        database = str(connect.get("database", "")).strip()
        if not database:
            raise RuntimeError(f"scratchbird client requires connect.database for engine: {engine.get('id')}")
        db_path = Path(database)
        if not db_path.exists():
            create_stdout_path = output_dir / f"{engine.get('id')}.create.{run_id}.stdout.txt"
            create_stderr_path = output_dir / f"{engine.get('id')}.create.{run_id}.stderr.txt"
            create_cmd = [binary, "create", "database", database]
            create_rc, create_stdout_text, create_stderr_text, _ = _run_subprocess(
                create_cmd,
                env,
                create_stdout_path,
                create_stderr_path,
                timeout_seconds,
            )
            if create_rc != 0:
                combined = f"{create_stdout_text}\n{create_stderr_text}"
                return AdapterResult(
                    status="error",
                    returncode=create_rc,
                    sqlstate=_extract_sqlstate(combined),
                    message="scratchbird create database failed",
                    stdout=create_stdout_text,
                    stderr=create_stderr_text,
                    elapsed_ms=0,
                    assert_lines=_extract_assert_lines(combined),
                    stdout_path=str(create_stdout_path),
                    stderr_path=str(create_stderr_path),
                )
        sql_text = sql_file.read_text(encoding="utf-8", errors="ignore")
        cmd = [binary, "open", database]
        rc, stdout_text, stderr_text, elapsed = _run_subprocess(
            cmd,
            env,
            stdout_path,
            stderr_path,
            timeout_seconds,
            stdin_text=sql_text,
        )
        combined = f"{stdout_text}\n{stderr_text}"
    elif client == "psql":
        host = str(connect.get("host", "127.0.0.1"))
        port = str(connect.get("port", "5432"))
        database = str(connect.get("database", "postgres"))
        user = str(connect.get("user", "postgres"))
        password = str(connect.get("password", ""))
        env["PGPASSWORD"] = password
        cmd = [
            binary,
            "-X",
            "-v",
            "ON_ERROR_STOP=1",
            "-q",
            "-A",
            "-t",
            "-h",
            host,
            "-p",
            port,
            "-U",
            user,
            "-d",
            database,
            "-f",
            str(sql_file),
        ]
        rc, stdout_text, stderr_text, elapsed = _run_subprocess(cmd, env, stdout_path, stderr_path, timeout_seconds)
        combined = f"{stdout_text}\n{stderr_text}"
    elif client == "mysql":
        host = str(connect.get("host", "127.0.0.1"))
        port = str(connect.get("port", "3306"))
        database = str(connect.get("database", "")).strip()
        user = str(connect.get("user", "root"))
        password = str(connect.get("password", ""))
        env["MYSQL_PWD"] = password
        cmd = [
            binary,
            "--batch",
            "--raw",
            "--skip-column-names",
            "--silent",
            "--host",
            host,
            "--port",
            port,
            "--user",
            user,
        ]
        if database:
            cmd.extend(["--database", database])
        sql_text = sql_file.read_text(encoding="utf-8", errors="ignore")
        rc, stdout_text, stderr_text, elapsed = _run_subprocess(
            cmd,
            env,
            stdout_path,
            stderr_path,
            timeout_seconds,
            stdin_text=sql_text,
        )
        combined = f"{stdout_text}\n{stderr_text}"
    elif client == "duckdb":
        database = str(connect.get("database", ":memory:")).strip() or ":memory:"
        if database == ":memory:":
            persistent_db = (output_dir / f"{engine.get('id')}.duckdb").resolve()
            if stage == "setup" and persistent_db.exists():
                persistent_db.unlink()
            database = str(persistent_db)
        cmd = [binary, database]
        sql_text = sql_file.read_text(encoding="utf-8", errors="ignore")
        rc, stdout_text, stderr_text, elapsed = _run_subprocess(
            cmd,
            env,
            stdout_path,
            stderr_path,
            timeout_seconds,
            stdin_text=sql_text,
        )
        combined = f"{stdout_text}\n{stderr_text}"
    elif client in {"isql-fb", "isql"}:
        user = str(connect.get("user", "SYSDBA"))
        password = str(connect.get("password", "masterkey"))
        database = _firebird_database(connect)
        cmd = [binary]
        if database:
            cmd.append(database)
        cmd.extend(["-user", user, "-password", password, "-q"])
        sql_text = sql_file.read_text(encoding="utf-8", errors="ignore")
        if not sql_text.endswith("\n"):
            sql_text += "\n"
        sql_text += "QUIT;\n"
        rc, stdout_text, stderr_text, elapsed = _run_subprocess(
            cmd,
            env,
            stdout_path,
            stderr_path,
            timeout_seconds,
            stdin_text=sql_text,
        )
        combined = f"{stdout_text}\n{stderr_text}"
        if rc == 124:
            fatal_markers = (
                "Statement failed",
                "Dynamic SQL Error",
                "Error writing data to the connection",
                "connection lost",
                "SQLSTATE =",
            )
            if not any(marker in combined for marker in fatal_markers):
                rc = 0
    else:
        raise RuntimeError(f"Unsupported client adapter: {client}")

    status = "ok" if rc == 0 else "error"
    sqlstate = _extract_sqlstate(combined)
    assert_lines = _extract_assert_lines(combined)
    message = ""
    if status != "ok":
        message = combined.strip().splitlines()[-1] if combined.strip() else f"{client} exited {rc}"

    return AdapterResult(
        status=status,
        returncode=rc,
        sqlstate=sqlstate,
        message=message,
        stdout=combined,
        stderr="",
        elapsed_ms=elapsed,
        assert_lines=assert_lines,
        stdout_path=str(stdout_path),
        stderr_path=str(stderr_path),
    )


def normalize_assertions(lines: List[str], order_sensitive: bool) -> List[str]:
    cleaned = [x.strip() for x in lines if x.strip()]
    return cleaned if order_sensitive else sorted(cleaned)


def compare_assertions(lhs: List[str], rhs: List[str], order_sensitive: bool) -> Tuple[bool, str]:
    a = normalize_assertions(lhs, order_sensitive)
    b = normalize_assertions(rhs, order_sensitive)
    if a == b:
        return True, ""
    return False, f"assertion mismatch lhs={a} rhs={b}"
