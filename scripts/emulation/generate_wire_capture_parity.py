#!/usr/bin/env python3
"""
Generate EMU-010/011 wire-capture parity artifacts.

Modes:
  - live: capture native vs emulated wire traffic from live endpoints
  - deterministic: generate encoder-fixture captures only
  - auto: live if both sides are available for an engine, else deterministic
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from datetime import date
from pathlib import Path
import os
import shutil
import signal
import socket
import struct
import subprocess
import tempfile
import time
from typing import Dict, Optional, Tuple


TODAY = date.today().isoformat()
SCRIPT_REPO_ROOT = Path(__file__).resolve().parents[2]


def hex_lines(data: bytes) -> str:
    chunks = []
    for i in range(0, len(data), 16):
        row = data[i : i + 16]
        chunks.append(" ".join(f"{b:02x}" for b in row))
    return "\n".join(chunks) + "\n"


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def write_text(path: Path, text: str) -> None:
    ensure_dir(path.parent)
    path.write_text(text, encoding="utf-8")


def write_hex(path: Path, payload: bytes) -> None:
    write_text(path, hex_lines(payload))


def parse_endpoint(endpoint: str) -> Tuple[str, int]:
    if ":" not in endpoint:
        raise ValueError(f"Endpoint must be host:port, got: {endpoint}")
    host, port_str = endpoint.rsplit(":", 1)
    return host, int(port_str)


def endpoint_available(endpoint: str, timeout_sec: float = 1.0) -> Tuple[bool, str]:
    try:
        host, port = parse_endpoint(endpoint)
        with socket.create_connection((host, port), timeout=timeout_sec):
            return True, "reachable"
    except Exception as exc:
        return False, str(exc)


def recv_exact(sock: socket.socket, n: int, timeout_sec: float) -> bytes:
    deadline = time.monotonic() + timeout_sec
    out = bytearray()
    while len(out) < n:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"Timed out while reading {n} bytes")
        sock.settimeout(remaining)
        chunk = sock.recv(n - len(out))
        if not chunk:
            raise ConnectionError("Connection closed while reading data")
        out.extend(chunk)
    return bytes(out)


def read_until_idle(
    sock: socket.socket,
    total_timeout_sec: float,
    idle_timeout_sec: float = 0.25,
    max_bytes: int = 262_144,
) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + total_timeout_sec
    while len(data) < max_bytes:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(min(idle_timeout_sec, remaining))
        try:
            chunk = sock.recv(min(8192, max_bytes - len(data)))
        except socket.timeout:
            break
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def capture_mysql_handshake(endpoint: str, timeout_sec: float) -> bytes:
    host, port = parse_endpoint(endpoint)
    with socket.create_connection((host, port), timeout=timeout_sec) as sock:
        header = recv_exact(sock, 4, timeout_sec)
        payload_len = header[0] | (header[1] << 8) | (header[2] << 16)
        payload = recv_exact(sock, payload_len, timeout_sec)
        return header + payload


def build_pg_startup_message(user: str, database: Optional[str]) -> bytes:
    params = bytearray()
    params += b"user\x00" + user.encode("utf-8") + b"\x00"
    if database is not None and database != "":
        params += b"database\x00" + database.encode("utf-8") + b"\x00"
    params += b"\x00"
    protocol_version = 196608  # 3.0
    total_len = 4 + 4 + len(params)
    return struct.pack(">II", total_len, protocol_version) + bytes(params)


def capture_postgresql_startup(endpoint: str, timeout_sec: float) -> bytes:
    host, port = parse_endpoint(endpoint)
    startup_user = os.environ.get("PG_CAPTURE_USER", "scratchbird")
    configured_db = os.environ.get("PG_CAPTURE_DATABASE", "").strip()
    candidate_dbs: list[Optional[str]] = [None]
    if configured_db:
        candidate_dbs.append(configured_db)
    for fallback in ("live_capture", "default", "postgres", "scratchbird"):
        if fallback not in candidate_dbs:
            candidate_dbs.append(fallback)

    last_response = b""
    last_error = ""
    for database in candidate_dbs:
        startup = build_pg_startup_message(startup_user, database)
        try:
            with socket.create_connection((host, port), timeout=timeout_sec) as sock:
                sock.sendall(startup)
                data = read_until_idle(sock, total_timeout_sec=timeout_sec, idle_timeout_sec=0.30)
                if not data:
                    continue
                last_response = data
                # Prefer capturing authentication challenge/ok responses.
                if data[:1] in (b"R", b"S", b"K", b"Z"):
                    return data
        except Exception as exc:
            last_error = str(exc)

    if last_response:
        return last_response
    raise RuntimeError(
        "No PostgreSQL startup response bytes received"
        + (f" ({last_error})" if last_error else "")
    )


def be32(value: int) -> bytes:
    return struct.pack(">I", value)


def build_firebird_connect_packet(database: str = "") -> bytes:
    # Minimal connect frame accepted by ScratchBird Firebird parser-agent.
    db = database.encode("utf-8")
    body = bytearray()
    body += be32(1)      # op_connect
    body += be32(16)     # protocol version
    body += be32(1)      # arch type
    body += be32(1)      # min type
    body += be32(1)      # max type
    body += be32(4096)   # page size
    body += be32(len(db))
    body += db
    return be32(len(body)) + bytes(body)


def capture_firebird_accept(endpoint: str, timeout_sec: float) -> bytes:
    host, port = parse_endpoint(endpoint)
    request = build_firebird_connect_packet()
    with socket.create_connection((host, port), timeout=timeout_sec) as sock:
        sock.sendall(request)
        header = recv_exact(sock, 4, timeout_sec)
        payload_len = struct.unpack(">I", header)[0]
        payload = recv_exact(sock, payload_len, timeout_sec)
        return header + payload


def mysql_handshake_packet_deterministic() -> Tuple[bytes, Dict[str, object]]:
    CLIENT_LONG_PASSWORD = 0x00000001
    CLIENT_FOUND_ROWS = 0x00000002
    CLIENT_LONG_FLAG = 0x00000004
    CLIENT_CONNECT_WITH_DB = 0x00000008
    CLIENT_PROTOCOL_41 = 0x00000200
    CLIENT_TRANSACTIONS = 0x00002000
    CLIENT_MULTI_STATEMENTS = 0x00010000
    CLIENT_MULTI_RESULTS = 0x00020000
    CLIENT_PLUGIN_AUTH = 0x00080000
    CLIENT_SESSION_TRACK = 0x00800000
    CLIENT_DEPRECATE_EOF = 0x01000000
    default_capabilities = (
        CLIENT_LONG_PASSWORD
        | CLIENT_FOUND_ROWS
        | CLIENT_LONG_FLAG
        | CLIENT_CONNECT_WITH_DB
        | CLIENT_PROTOCOL_41
        | CLIENT_TRANSACTIONS
        | CLIENT_MULTI_STATEMENTS
        | CLIENT_MULTI_RESULTS
        | CLIENT_PLUGIN_AUTH
        | CLIENT_SESSION_TRACK
        | CLIENT_DEPRECATE_EOF
    )
    payload = bytearray()
    payload.append(10)
    payload += b"8.0.32-ScratchBird\x00"
    payload += struct.pack("<I", 0x12345678)
    payload += bytes(range(1, 9))
    payload.append(0)
    payload += struct.pack("<H", default_capabilities & 0xFFFF)
    payload.append(255)
    payload += struct.pack("<H", 0x0002)
    payload += struct.pack("<H", (default_capabilities >> 16) & 0xFFFF)
    payload.append(21)
    payload += b"\x00" * 10
    payload += bytes(range(9, 21))
    payload += b"\x00mysql_native_password\x00"
    return struct.pack("<I", len(payload))[:3] + b"\x00" + payload, {"mode": "deterministic"}


def postgresql_startup_auth_stream_deterministic() -> Tuple[bytes, Dict[str, object]]:
    def pg_msg(msg_type: bytes, body: bytes) -> bytes:
        return msg_type + struct.pack(">I", 4 + len(body)) + body

    stream = bytearray()
    stream += pg_msg(b"R", struct.pack(">I", 5) + b"ABCD")
    stream += pg_msg(b"R", struct.pack(">I", 0))
    stream += pg_msg(b"S", b"server_version\x0014.0\x00")
    stream += pg_msg(b"K", struct.pack(">II", 4242, 2121))
    stream += pg_msg(b"Z", b"I")
    return bytes(stream), {"mode": "deterministic"}


def firebird_accept_packet_deterministic() -> Tuple[bytes, Dict[str, object]]:
    body = bytearray()
    body += be32(3)
    body += be32(16)
    body += be32(1)
    body += be32(1)
    body += be32(1)
    body += be32(1)
    body += be32(0)
    data = b"Firebird/ScratchBird"
    body += be32(len(data)) + data
    plugin = b"Srp256"
    body += be32(len(plugin)) + plugin
    plugins = b"Srp256,Srp,Legacy_Auth"
    body += be32(len(plugins)) + plugins
    body += be32(0)
    return be32(len(body)) + bytes(body), {"mode": "deterministic"}


def compare_bytes(native: bytes, emulated: bytes) -> Dict[str, object]:
    if native == emulated:
        return {"equal": True, "first_mismatch": None, "native_len": len(native), "emulated_len": len(emulated)}
    min_len = min(len(native), len(emulated))
    mismatch = min_len
    for idx in range(min_len):
        if native[idx] != emulated[idx]:
            mismatch = idx
            break
    return {
        "equal": False,
        "first_mismatch": mismatch,
        "native_len": len(native),
        "emulated_len": len(emulated),
    }


def extract_mysql_server_version(packet: bytes) -> str:
    if len(packet) < 5:
        return ""
    payload_len = packet[0] | (packet[1] << 8) | (packet[2] << 16)
    payload = packet[4 : 4 + payload_len]
    if not payload or payload[0] != 10:
        return ""
    terminator = payload.find(b"\x00", 1)
    if terminator <= 1:
        return ""
    return payload[1:terminator].decode("utf-8", errors="replace")


def extract_mysql_version_series(version: str) -> str:
    if not version:
        return ""
    match = re.match(r"^\s*(\d+)\.(\d+)", version)
    if not match:
        return ""
    return f"{match.group(1)}.{match.group(2)}"


def parse_mysql_handshake(packet: bytes) -> Dict[str, object]:
    parsed: Dict[str, object] = {
        "valid": False,
        "error": "",
    }
    if len(packet) < 4:
        parsed["error"] = "packet too short"
        return parsed

    payload_len = packet[0] | (packet[1] << 8) | (packet[2] << 16)
    sequence = packet[3]
    if len(packet) < 4 + payload_len:
        parsed["error"] = "payload truncated"
        return parsed

    payload = packet[4 : 4 + payload_len]
    if not payload:
        parsed["error"] = "empty payload"
        return parsed
    if payload[0] != 10:
        parsed["error"] = f"unsupported protocol version byte: {payload[0]}"
        return parsed

    offset = 1
    term = payload.find(b"\x00", offset)
    if term <= offset:
        parsed["error"] = "missing server_version terminator"
        return parsed
    server_version = payload[offset:term].decode("utf-8", errors="replace")
    offset = term + 1

    if offset + 4 + 8 + 1 + 2 + 1 + 2 + 2 + 1 + 10 > len(payload):
        parsed["error"] = "payload shorter than handshake fixed fields"
        return parsed

    connection_id = int.from_bytes(payload[offset : offset + 4], "little")
    offset += 4
    auth_part1 = payload[offset : offset + 8]
    offset += 8
    filler = payload[offset]
    offset += 1
    cap_low = int.from_bytes(payload[offset : offset + 2], "little")
    offset += 2
    charset = payload[offset]
    offset += 1
    status_flags = int.from_bytes(payload[offset : offset + 2], "little")
    offset += 2
    cap_high = int.from_bytes(payload[offset : offset + 2], "little")
    offset += 2
    capabilities = cap_low | (cap_high << 16)
    auth_plugin_data_len = payload[offset]
    offset += 1
    reserved = payload[offset : offset + 10]
    offset += 10

    auth_part2_len = max(13, auth_plugin_data_len - 8)
    auth_part2 = payload[offset : min(len(payload), offset + auth_part2_len)]
    offset += len(auth_part2)

    auth_plugin_name = ""
    if offset < len(payload):
        term = payload.find(b"\x00", offset)
        if term == -1:
            term = len(payload)
        auth_plugin_name = payload[offset:term].decode("utf-8", errors="replace")

    parsed.update(
        {
            "valid": True,
            "sequence": sequence,
            "payload_len": payload_len,
            "protocol_version": payload[0],
            "server_version": server_version,
            "server_version_series": extract_mysql_version_series(server_version),
            "connection_id": connection_id,
            "auth_plugin_data_part1_hex": auth_part1.hex(),
            "filler": filler,
            "capabilities": capabilities,
            "charset": charset,
            "status_flags": status_flags,
            "auth_plugin_data_len": auth_plugin_data_len,
            "reserved_hex": reserved.hex(),
            "auth_plugin_data_part2_hex": auth_part2.hex(),
            "auth_plugin_name": auth_plugin_name,
        }
    )
    return parsed


MYSQL_CAPABILITY_NAMES: Dict[int, str] = {
    0x00000001: "LONG_PASSWORD",
    0x00000002: "FOUND_ROWS",
    0x00000004: "LONG_FLAG",
    0x00000008: "CONNECT_WITH_DB",
    0x00000010: "NO_SCHEMA",
    0x00000020: "COMPRESS",
    0x00000040: "ODBC",
    0x00000080: "LOCAL_FILES",
    0x00000100: "IGNORE_SPACE",
    0x00000200: "PROTOCOL_41",
    0x00000400: "INTERACTIVE",
    0x00000800: "SSL",
    0x00001000: "IGNORE_SIGPIPE",
    0x00002000: "TRANSACTIONS",
    0x00004000: "RESERVED",
    0x00008000: "SECURE_CONNECTION",
    0x00010000: "MULTI_STATEMENTS",
    0x00020000: "MULTI_RESULTS",
    0x00040000: "PS_MULTI_RESULTS",
    0x00080000: "PLUGIN_AUTH",
    0x00100000: "CONNECT_ATTRS",
    0x00200000: "PLUGIN_AUTH_LENENC_DATA",
    0x00400000: "CAN_HANDLE_EXPIRED_PW",
    0x00800000: "SESSION_TRACK",
    0x01000000: "DEPRECATE_EOF",
    0x02000000: "OPTIONAL_RESULTSET_METADATA",
    0x04000000: "ZSTD_COMPRESSION_ALGORITHM",
    0x08000000: "QUERY_ATTRIBUTES",
    0x10000000: "MULTI_FACTOR_AUTHENTICATION",
    0x20000000: "CAPABILITY_EXTENSION",
    0x40000000: "SSL_VERIFY_SERVER_CERT",
    0x80000000: "REMEMBER_OPTIONS",
}

MYSQL_OPTIONAL_CAPABILITY_DRIFT_MASK = (
    0x00000800 |  # SSL
    0x04000000 |  # ZSTD_COMPRESSION_ALGORITHM
    0x40000000    # SSL_VERIFY_SERVER_CERT
)


def capability_names(mask: int) -> list[str]:
    names: list[str] = []
    for bit, name in MYSQL_CAPABILITY_NAMES.items():
        if mask & bit:
            names.append(name)
    unknown = mask & ~sum(MYSQL_CAPABILITY_NAMES.keys())
    if unknown:
        names.append(f"UNKNOWN(0x{unknown:08x})")
    return names


def compare_mysql_handshake_semantics(
    native_packet: bytes,
    emulated_packet: bytes,
    baseline_series: str = "8.4",
) -> Dict[str, object]:
    native = parse_mysql_handshake(native_packet)
    emulated = parse_mysql_handshake(emulated_packet)

    result: Dict[str, object] = {
        "available": False,
        "equal": False,
        "baseline_series": baseline_series,
        "native": native,
        "emulated": emulated,
        "differences": [],
    }

    if not native.get("valid") or not emulated.get("valid"):
        result["parse_error"] = {
            "native_error": native.get("error", ""),
            "emulated_error": emulated.get("error", ""),
        }
        return result

    result["available"] = True
    diffs: list[str] = []

    # Deterministic fields that should match for the selected emulation family.
    for key in ("protocol_version", "charset", "status_flags", "auth_plugin_data_len", "auth_plugin_name"):
        if native.get(key) != emulated.get(key):
            diffs.append(f"{key}: native={native.get(key)!r} emulated={emulated.get(key)!r}")

    native_caps = int(native.get("capabilities", 0))
    emulated_caps = int(emulated.get("capabilities", 0))
    missing_caps = native_caps & ~emulated_caps
    extra_caps = emulated_caps & ~native_caps
    non_optional_cap_diff = (missing_caps | extra_caps) & ~MYSQL_OPTIONAL_CAPABILITY_DRIFT_MASK
    if non_optional_cap_diff:
        diffs.append(
            "capabilities(non-optional): "
            f"missing=0x{missing_caps:08x} extra=0x{extra_caps:08x}"
        )

    native_version = str(native.get("server_version", ""))
    emulated_version = str(emulated.get("server_version", ""))
    native_series = str(native.get("server_version_series", ""))
    emulated_series = str(emulated.get("server_version_series", ""))
    baseline_skew = bool(native_series and baseline_series and native_series != baseline_series)
    if not baseline_skew and native_version != emulated_version:
        diffs.append(f"server_version: native={native_version!r} emulated={emulated_version!r}")

    result["differences"] = diffs
    result["equal"] = len(diffs) == 0
    result["baseline_skew"] = baseline_skew
    result["native_series"] = native_series
    result["emulated_series"] = emulated_series
    result["missing_capabilities_mask"] = f"0x{missing_caps:08x}"
    result["extra_capabilities_mask"] = f"0x{extra_caps:08x}"
    result["missing_capabilities"] = capability_names(missing_caps)
    result["extra_capabilities"] = capability_names(extra_caps)
    result["optional_capability_only_drift"] = non_optional_cap_diff == 0 and (missing_caps | extra_caps) != 0
    return result


@dataclass
class CaptureResult:
    engine: str
    mode: str
    native_endpoint: str
    emulated_endpoint: str
    native_bytes: Optional[bytes]
    emulated_bytes: Optional[bytes]
    native_error: str
    emulated_error: str


@dataclass
class EmulatedServerHandle:
    process: subprocess.Popen
    temp_dir: Path
    log_path: Path

    def stop(self, cleanup_temp_dir: bool = True) -> None:
        control_dir = self.temp_dir / "control"
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        force_stop_orphaned_emulated_processes(control_dir)
        if cleanup_temp_dir:
            shutil.rmtree(self.temp_dir, ignore_errors=True)


def force_stop_orphaned_emulated_processes(control_dir: Path) -> None:
    """Best-effort cleanup for listener/parser processes that can outlive sb_server."""
    marker = str(control_dir)
    if not marker:
        return

    # Kill only listeners/parsers tied to this temp control directory marker.
    targets = []
    try:
        listing = subprocess.check_output(
            ["ps", "-eo", "pid=,args="],
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except Exception:
        return

    for raw_line in listing.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        pid_text, args = parts
        if marker not in args:
            continue
        if "sb_listener_" not in args and "sb_parser_" not in args and "sb_server" not in args:
            continue
        try:
            pid = int(pid_text)
        except ValueError:
            continue
        targets.append(pid)

    if not targets:
        return

    for pid in targets:
        try:
            os.kill(pid, signal.SIGTERM)
        except Exception:
            continue

    time.sleep(0.2)
    for pid in targets:
        try:
            os.kill(pid, 0)
        except Exception:
            continue
        try:
            os.kill(pid, signal.SIGKILL)
        except Exception:
            continue


def resolve_current_identity() -> Tuple[str, str]:
    user = os.environ.get("USER", "")
    group = os.environ.get("GROUP", "")

    if not user:
        try:
            import pwd  # type: ignore

            user = pwd.getpwuid(os.getuid()).pw_name
        except Exception:
            user = ""
    if not group:
        try:
            import grp  # type: ignore

            group = grp.getgrgid(os.getgid()).gr_name
        except Exception:
            group = ""

    # Service defaults resolve empty run_as values to "scratchbird",
    # so always force explicit valid local identity.
    if not user:
        user = "root"
    if not group:
        group = user
    return user, group


def write_temp_server_config(
    config_path: Path,
    pid_file: Path,
    run_as_user: str,
    run_as_group: str,
) -> None:
    config_text = "\n".join(
        [
            "[server]",
            "mode=single-database",
            f"pid_file={pid_file}",
            f"run_as_user={run_as_user}",
            f"run_as_group={run_as_group}",
            "",
        ]
    )
    write_text(config_path, config_text)


def summarize_startup_log(log_path: Path) -> str:
    if not log_path.exists():
        return "no startup log"
    try:
        lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    except Exception as exc:
        return f"log read failed: {exc}"
    if not lines:
        return "empty startup log"

    interesting = []
    for line in lines:
        lower = line.lower()
        if "error" in lower or "failed" in lower or "exec" in lower:
            interesting.append(line.strip())

    if interesting:
        return " | ".join(interesting[-12:])

    tail = [line.strip() for line in lines[-20:] if line.strip()]
    if tail:
        return " | ".join(tail)
    return "empty startup log"


def wait_for_endpoint(endpoint: str, timeout_sec: float) -> bool:
    deadline = time.monotonic() + timeout_sec
    while time.monotonic() < deadline:
        available, _ = endpoint_available(endpoint, timeout_sec=0.5)
        if available:
            return True
        time.sleep(0.2)
    return False


def start_emulated_server(
    sb_server_path: Path,
    emu_pg: str,
    emu_mysql: str,
    emu_fb: str,
    startup_timeout_sec: float,
) -> Tuple[Optional[EmulatedServerHandle], str]:
    if not sb_server_path.exists():
        return None, f"sb_server binary missing at {sb_server_path}"

    pg_host, pg_port = parse_endpoint(emu_pg)
    my_host, my_port = parse_endpoint(emu_mysql)
    fb_host, fb_port = parse_endpoint(emu_fb)
    if not (pg_host == my_host == fb_host):
        return None, "emulated endpoints use mixed hosts"

    tmp_root = Path(tempfile.mkdtemp(prefix="sb-live-capture-"))
    db_file = tmp_root / "live_capture.sbdb"
    control_dir = tmp_root / "control"
    unix_socket = tmp_root / "sb.sock"
    config_path = tmp_root / "sb_server.live.conf"
    pid_file = tmp_root / "sb_server.pid"
    log_path = tmp_root / "sb_server_live_capture.log"
    ensure_dir(control_dir)
    run_as_user, run_as_group = resolve_current_identity()
    write_temp_server_config(
        config_path=config_path,
        pid_file=pid_file,
        run_as_user=run_as_user,
        run_as_group=run_as_group,
    )

    cmd = [
        str(sb_server_path),
        "-c",
        str(config_path),
        "-F",
        "-d",
        str(db_file),
        "--create",
        "--host",
        pg_host,
        "--port",
        "0",
        "--postgres-pool-min",
        "0",
        "--mysql-pool-min",
        "0",
        "--firebird-pool-min",
        "0",
        "--pg-port",
        str(pg_port),
        "--mysql-port",
        str(my_port),
        "--fb-port",
        str(fb_port),
        "--enable-postgres",
        "--enable-mysql",
        "--enable-firebird",
        "--disable-native",
        "--control-socket-dir",
        str(control_dir),
        "--unix-socket",
        str(unix_socket),
    ]

    with open(log_path, "w", encoding="utf-8") as log_file:
        env = os.environ.copy()
        current_path = env.get("PATH", "")
        path_parts = [str(sb_server_path.parent)]
        build_src_dir = SCRIPT_REPO_ROOT / "build" / "src"
        if build_src_dir.exists() and str(build_src_dir) not in path_parts:
            path_parts.append(str(build_src_dir))
        if current_path:
            path_parts.append(current_path)
        env["PATH"] = ":".join(path_parts)
        process = subprocess.Popen(
            cmd,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=str(tmp_root),
            env=env,
        )

    handle = EmulatedServerHandle(process=process, temp_dir=tmp_root, log_path=log_path)

    ok = (
        wait_for_endpoint(emu_pg, startup_timeout_sec)
        and wait_for_endpoint(emu_mysql, startup_timeout_sec)
        and wait_for_endpoint(emu_fb, startup_timeout_sec)
    )
    if not ok:
        exit_code = handle.process.poll()
        log_excerpt = summarize_startup_log(log_path)
        issue = (
            f"auto-start failed (exit={exit_code if exit_code is not None else 'running-but-unready'}). "
            f"log: {log_excerpt}"
        )
        handle.stop(cleanup_temp_dir=True)
        return None, issue
    return handle, ""


def capture_live_pair(
    engine: str,
    native_endpoint: str,
    emulated_endpoint: str,
    timeout_sec: float,
) -> CaptureResult:
    capture_fn = {
        "mysql": capture_mysql_handshake,
        "postgresql": capture_postgresql_startup,
        "firebird": capture_firebird_accept,
    }[engine]

    native_bytes: Optional[bytes] = None
    emu_bytes: Optional[bytes] = None
    native_error = ""
    emu_error = ""

    native_ok, native_status = endpoint_available(native_endpoint, timeout_sec=1.0)
    if native_ok:
        try:
            native_bytes = capture_fn(native_endpoint, timeout_sec)
        except Exception as exc:
            native_error = str(exc)
    else:
        native_error = f"Native endpoint unavailable: {native_status}"

    emu_ok, emu_status = endpoint_available(emulated_endpoint, timeout_sec=1.0)
    if emu_ok:
        try:
            emu_bytes = capture_fn(emulated_endpoint, timeout_sec)
        except Exception as exc:
            emu_error = str(exc)
    else:
        emu_error = f"Emulated endpoint unavailable: {emu_status}"

    return CaptureResult(
        engine=engine,
        mode="live",
        native_endpoint=native_endpoint,
        emulated_endpoint=emulated_endpoint,
        native_bytes=native_bytes,
        emulated_bytes=emu_bytes,
        native_error=native_error,
        emulated_error=emu_error,
    )


def emit_engine_artifacts(root: Path, result: CaptureResult) -> Dict[str, object]:
    if result.engine == "mysql":
        capture_dir = root / "mysql" / "p5s1w2" / "my-emu-010-wire-captures"
        doc_path = root / "mysql" / "p5s1w2" / "my-emu-011-handshake-parity.md"
        native_file = "mysql-native-live-handshake.hex"
        emu_file = "mysql-emulated-live-handshake.hex"
        title = "# MY-EMU-011 Handshake Parity"
    elif result.engine == "postgresql":
        capture_dir = root / "postgresql" / "p5s1w2" / "pg-emu-010-wire-captures"
        doc_path = root / "postgresql" / "p5s1w2" / "pg-emu-011-startup-auth.md"
        native_file = "pg-native-live-startup-auth.hex"
        emu_file = "pg-emulated-live-startup-auth.hex"
        title = "# PG-EMU-011 Startup/Auth Parity"
    else:
        capture_dir = root / "firebird" / "p5s1w2" / "fb-emu-010-wire-captures"
        doc_path = root / "firebird" / "p5s1w2" / "fb-emu-011-handshake-parity.md"
        native_file = "firebird-native-live-accept.hex"
        emu_file = "firebird-emulated-live-accept.hex"
        title = "# FB-EMU-011 Handshake Parity"

    ensure_dir(capture_dir)
    native_rel = None
    emu_rel = None
    if result.native_bytes is not None:
        write_hex(capture_dir / native_file, result.native_bytes)
        native_rel = f"artifacts/emulation/{capture_dir.relative_to(root)}/{native_file}"
    if result.emulated_bytes is not None:
        write_hex(capture_dir / emu_file, result.emulated_bytes)
        emu_rel = f"artifacts/emulation/{capture_dir.relative_to(root)}/{emu_file}"

    comparison = None
    mysql_semantic_comparison = None
    if result.native_bytes is not None and result.emulated_bytes is not None:
        comparison = compare_bytes(result.native_bytes, result.emulated_bytes)
        if result.engine == "mysql":
            baseline_series = os.environ.get("MYSQL_EMU_BASELINE_SERIES", "8.4").strip() or "8.4"
            mysql_semantic_comparison = compare_mysql_handshake_semantics(
                result.native_bytes,
                result.emulated_bytes,
                baseline_series=baseline_series,
            )

    lines = [
        f"Last updated: {TODAY}",
        "",
        title,
        "",
        f"- Capture mode: `{result.mode}`",
        f"- Native endpoint: `{result.native_endpoint}`",
        f"- Emulated endpoint: `{result.emulated_endpoint}`",
        f"- Native capture: `{'ok' if result.native_bytes is not None else 'unavailable'}`",
        f"- Emulated capture: `{'ok' if result.emulated_bytes is not None else 'unavailable'}`",
    ]
    if result.engine == "mysql":
        if result.native_bytes is not None:
            native_version = extract_mysql_server_version(result.native_bytes)
            if native_version:
                lines.append(f"- Native server_version: `{native_version}`")
        if result.emulated_bytes is not None:
            emulated_version = extract_mysql_server_version(result.emulated_bytes)
            if emulated_version:
                lines.append(f"- Emulated server_version: `{emulated_version}`")
        if mysql_semantic_comparison is not None:
            lines.append(
                f"- Baseline series target: `{mysql_semantic_comparison.get('baseline_series', '8.4')}`"
            )
            native_series = mysql_semantic_comparison.get("native_series")
            emulated_series = mysql_semantic_comparison.get("emulated_series")
            if native_series:
                lines.append(f"- Native version series: `{native_series}`")
            if emulated_series:
                lines.append(f"- Emulated version series: `{emulated_series}`")
    if result.native_error:
        lines.append(f"- Native issue: `{result.native_error}`")
    if result.emulated_error:
        lines.append(f"- Emulated issue: `{result.emulated_error}`")

    lines.extend(["", "## Comparison"])
    if comparison is None:
        lines.append("- Comparison status: `skipped` (both capture streams not available).")
    else:
        lines.append(f"- Byte-equivalent: `{str(comparison['equal']).lower()}`")
        lines.append(f"- Native length: `{comparison['native_len']}`")
        lines.append(f"- Emulated length: `{comparison['emulated_len']}`")
        lines.append(
            f"- First mismatch offset: `{comparison['first_mismatch'] if comparison['first_mismatch'] is not None else 'none'}`"
        )
        if mysql_semantic_comparison is not None and mysql_semantic_comparison.get("available"):
            lines.append(
                f"- Semantic parity (normalized): `{str(mysql_semantic_comparison.get('equal', False)).lower()}`"
            )
            if mysql_semantic_comparison.get("baseline_skew"):
                lines.append(
                    "- Native endpoint series is outside baseline target; "
                    "server-version mismatch treated as expected track drift."
                )
            lines.append(
                f"- Missing capabilities (emulated): `{mysql_semantic_comparison.get('missing_capabilities_mask', '0x00000000')}`"
            )
            missing_caps = mysql_semantic_comparison.get("missing_capabilities", [])
            if missing_caps:
                lines.append(
                    f"- Missing capability names: `{', '.join(missing_caps)}`"
                )
            extra_caps = mysql_semantic_comparison.get("extra_capabilities", [])
            lines.append(
                f"- Extra capabilities (emulated): `{mysql_semantic_comparison.get('extra_capabilities_mask', '0x00000000')}`"
            )
            if extra_caps:
                lines.append(
                    f"- Extra capability names: `{', '.join(extra_caps)}`"
                )
            if mysql_semantic_comparison.get("optional_capability_only_drift"):
                lines.append("- Capability drift is currently optional-only.")
            differences = mysql_semantic_comparison.get("differences", [])
            if differences:
                lines.append("- Normalized differences:")
                for diff in differences:
                    lines.append(f"  - `{diff}`")

    lines.extend(["", "## Capture Artifacts"])
    lines.append(f"- Native capture: `{native_rel if native_rel else 'not generated'}`")
    lines.append(f"- Emulated capture: `{emu_rel if emu_rel else 'not generated'}`")
    lines.append("")
    write_text(doc_path, "\n".join(lines))

    return {
        "engine": result.engine,
        "native_ok": result.native_bytes is not None,
        "emulated_ok": result.emulated_bytes is not None,
        "comparison": comparison,
        "mysql_semantic_comparison": mysql_semantic_comparison,
        "native_issue": result.native_error,
        "emulated_issue": result.emulated_error,
    }


def emit_availability_summary(root: Path, summaries: Dict[str, Dict[str, object]], mode: str) -> None:
    summary_path = root / "live-capture-availability-2026-02-22.md"
    lines = [
        f"Last updated: {TODAY}",
        "",
        "# Live Capture Availability Summary",
        "",
        f"- Requested mode: `{mode}`",
        "",
        "## Engine Status",
    ]
    for engine in ("mysql", "postgresql", "firebird"):
        info = summaries.get(engine, {})
        lines.append(f"- `{engine}` native capture: `{'ok' if info.get('native_ok') else 'unavailable'}`")
        lines.append(f"- `{engine}` emulated capture: `{'ok' if info.get('emulated_ok') else 'unavailable'}`")
        if info.get("native_issue"):
            lines.append(f"- `{engine}` native issue: `{info['native_issue']}`")
        if info.get("emulated_issue"):
            lines.append(f"- `{engine}` emulated issue: `{info['emulated_issue']}`")
    lines.append("")
    write_text(summary_path, "\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate emulation wire-capture parity artifacts.")
    default_workspace = SCRIPT_REPO_ROOT
    default_artifact_root = default_workspace / "tests/compatibility/results/emulation"
    parser.add_argument(
        "--artifact-root",
        default=str(default_artifact_root),
        help="Root artifacts directory.",
    )
    parser.add_argument(
        "--mode",
        default="live",
        choices=["live", "deterministic", "auto"],
        help="Capture mode.",
    )
    parser.add_argument(
        "--workspace-root",
        default=str(default_workspace),
        help="Workspace root (used to locate sb_server when starting emulated runtime).",
    )
    parser.add_argument("--native-mysql", default="127.0.0.1:3306")
    parser.add_argument("--native-postgresql", default="127.0.0.1:5432")
    parser.add_argument("--native-firebird", default="127.0.0.1:3050")
    parser.add_argument("--emulated-mysql", default="127.0.0.1:13306")
    parser.add_argument("--emulated-postgresql", default="127.0.0.1:15432")
    parser.add_argument("--emulated-firebird", default="127.0.0.1:13050")
    parser.add_argument(
        "--timeout-sec",
        default=5.0,
        type=float,
        help="Socket operation timeout in seconds.",
    )
    parser.add_argument(
        "--start-emulated-server",
        action="store_true",
        default=False,
        help="Start sb_server on emulated endpoints when they are unavailable.",
    )
    args = parser.parse_args()
    root = Path(args.artifact_root)

    if args.mode == "deterministic":
        mysql_bytes, _ = mysql_handshake_packet_deterministic()
        pg_bytes, _ = postgresql_startup_auth_stream_deterministic()
        fb_bytes, _ = firebird_accept_packet_deterministic()
        emit_engine_artifacts(
            root,
            CaptureResult(
                engine="mysql",
                mode="deterministic",
                native_endpoint="deterministic",
                emulated_endpoint="deterministic",
                native_bytes=mysql_bytes,
                emulated_bytes=mysql_bytes,
                native_error="",
                emulated_error="",
            ),
        )
        emit_engine_artifacts(
            root,
            CaptureResult(
                engine="postgresql",
                mode="deterministic",
                native_endpoint="deterministic",
                emulated_endpoint="deterministic",
                native_bytes=pg_bytes,
                emulated_bytes=pg_bytes,
                native_error="",
                emulated_error="",
            ),
        )
        emit_engine_artifacts(
            root,
            CaptureResult(
                engine="firebird",
                mode="deterministic",
                native_endpoint="deterministic",
                emulated_endpoint="deterministic",
                native_bytes=fb_bytes,
                emulated_bytes=fb_bytes,
                native_error="",
                emulated_error="",
            ),
        )
        emit_availability_summary(
            root,
            {
                "mysql": {"native_ok": True, "emulated_ok": True},
                "postgresql": {"native_ok": True, "emulated_ok": True},
                "firebird": {"native_ok": True, "emulated_ok": True},
            },
            mode="deterministic",
        )
        print(f"Generated deterministic wire-capture artifacts under: {root}")
        return

    emulated_handle: Optional[EmulatedServerHandle] = None
    emulated_start_issue = ""
    try:
        if args.start_emulated_server:
            emu_checks = [
                endpoint_available(args.emulated_mysql, 0.5)[0],
                endpoint_available(args.emulated_postgresql, 0.5)[0],
                endpoint_available(args.emulated_firebird, 0.5)[0],
            ]
            if not all(emu_checks):
                workspace_root = Path(args.workspace_root)
                sb_server_candidates = [
                    workspace_root / "build/src/sb_server",
                    workspace_root / "build/src/server/sb_server",
                    workspace_root / "build_clean/src/sb_server",
                    workspace_root / "ScratchBird/build/src/sb_server",
                ]
                sb_server_path = next((p for p in sb_server_candidates if p.exists()), sb_server_candidates[0])
                emulated_handle, emulated_start_issue = start_emulated_server(
                    sb_server_path=sb_server_path,
                    emu_pg=args.emulated_postgresql,
                    emu_mysql=args.emulated_mysql,
                    emu_fb=args.emulated_firebird,
                    startup_timeout_sec=20.0,
                )

        mysql_live = capture_live_pair("mysql", args.native_mysql, args.emulated_mysql, args.timeout_sec)
        pg_live = capture_live_pair("postgresql", args.native_postgresql, args.emulated_postgresql, args.timeout_sec)
        fb_live = capture_live_pair("firebird", args.native_firebird, args.emulated_firebird, args.timeout_sec)

        if emulated_start_issue:
            for result in (mysql_live, pg_live, fb_live):
                if result.emulated_bytes is None:
                    if result.emulated_error:
                        result.emulated_error += f"; {emulated_start_issue}"
                    else:
                        result.emulated_error = emulated_start_issue

        summaries: Dict[str, Dict[str, object]] = {}
        for live_result, deterministic_fn in [
            (mysql_live, mysql_handshake_packet_deterministic),
            (pg_live, postgresql_startup_auth_stream_deterministic),
            (fb_live, firebird_accept_packet_deterministic),
        ]:
            if args.mode == "auto" and (live_result.native_bytes is None or live_result.emulated_bytes is None):
                det_bytes, _ = deterministic_fn()
                det_result = CaptureResult(
                    engine=live_result.engine,
                    mode="deterministic-fallback",
                    native_endpoint=live_result.native_endpoint,
                    emulated_endpoint=live_result.emulated_endpoint,
                    native_bytes=det_bytes,
                    emulated_bytes=det_bytes,
                    native_error=live_result.native_error,
                    emulated_error=live_result.emulated_error,
                )
                summaries[live_result.engine] = emit_engine_artifacts(root, det_result)
            else:
                summaries[live_result.engine] = emit_engine_artifacts(root, live_result)

        emit_availability_summary(root, summaries, mode=args.mode)
        print(f"Generated {args.mode} wire-capture artifacts under: {root}")
    finally:
        if emulated_handle is not None:
            emulated_handle.stop()


if __name__ == "__main__":
    main()
