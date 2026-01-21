"""Connection implementation for ScratchBird Python driver."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, Optional

import os
import socket
import ssl

from . import errors
from .dsn import parse_dsn
from .cursor import Cursor
from .protocol import (
    AuthMethod,
    AuthStatus,
    MessageType,
    build_auth_request,
    build_commit,
    build_connect_request,
    build_query,
    decode_header,
    parse_auth_response,
    parse_command_complete,
    parse_connect_response,
    parse_query_error,
    parse_query_result,
    parse_row_data,
    parse_row_description,
)
from .scram import ScramExchange
from .types import decode_value


@dataclass
class ConnectionConfig:
    host: str = "localhost"
    port: int = 3092
    database: Optional[str] = None
    user: Optional[str] = None
    password: Optional[str] = None
    sslmode: str = "prefer"
    connect_timeout: int = 30
    socket_timeout: int = 0
    application_name: Optional[str] = None
    search_path: Optional[str] = None
    binary_transfer: bool = True
    compression: Optional[str] = None
    extra: Dict[str, Any] = field(default_factory=dict)


def connect(dsn=None, user=None, password=None, host=None, database=None, **kwargs):
    params = {}
    params.update(parse_dsn(dsn))

    for key, value in kwargs.items():
        params[key] = value

    if user is not None:
        params["user"] = user
    if password is not None:
        params["password"] = password
    if host is not None:
        params["host"] = host
    if database is not None:
        params["database"] = database

    cfg = ConnectionConfig()
    cfg.host = params.get("host", cfg.host)
    cfg.port = int(params.get("port", cfg.port))
    cfg.database = params.get("database", cfg.database)
    cfg.user = params.get("user", cfg.user)
    cfg.password = params.get("password", cfg.password)
    cfg.sslmode = params.get("sslmode", params.get("ssl", cfg.sslmode))
    cfg.connect_timeout = int(params.get("connect_timeout", cfg.connect_timeout))
    cfg.socket_timeout = int(params.get("socket_timeout", cfg.socket_timeout))
    cfg.application_name = params.get("application_name", cfg.application_name)
    cfg.search_path = params.get("search_path", cfg.search_path)
    cfg.binary_transfer = bool(params.get("binary_transfer", cfg.binary_transfer))
    cfg.compression = params.get("compression", cfg.compression)
    cfg.extra = {k: v for k, v in params.items() if k not in {
        "host", "port", "database", "user", "password", "sslmode", "ssl",
        "connect_timeout", "socket_timeout", "application_name", "search_path",
        "binary_transfer", "compression"}}

    return Connection(cfg)


class Connection:
    def __init__(self, config: ConnectionConfig):
        self._config = config
        self._closed = False
        self._cursors = []
        self._autocommit = True
        self._warnings = None
        self._session_id = None
        self._socket = None
        self._in_transaction = False
        self._connect()

    def _connect(self) -> None:
        if not self._config.user or not self._config.database:
            raise errors.InterfaceError("user and database are required")
        raw_sock = socket.create_connection(
            (self._config.host, self._config.port),
            timeout=self._config.connect_timeout,
        )
        raw_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        raw_sock.settimeout(self._config.socket_timeout or None)
        sock = raw_sock

        if self._config.sslmode.lower() != "disable":
            ctx = ssl.create_default_context()
            if self._config.sslmode.lower() in ("require", "verify-ca", "verify-full", "prefer"):
                ctx.check_hostname = self._config.sslmode.lower() == "verify-full"
                if self._config.sslmode.lower() == "require":
                    ctx.check_hostname = False
            if self._config.sslmode.lower() in ("allow", "prefer"):
                ctx.check_hostname = False
            if self._config.sslmode.lower() == "disable":
                pass
            if self._config.sslmode.lower() in ("verify-ca", "verify-full"):
                ctx.verify_mode = ssl.CERT_REQUIRED
            if self._config.sslmode.lower() in ("require", "prefer", "allow"):
                ctx.verify_mode = ssl.CERT_NONE
            if self._config.extra.get("sslrootcert"):
                ctx.load_verify_locations(self._config.extra.get("sslrootcert"))
            if self._config.extra.get("sslcert") and self._config.extra.get("sslkey"):
                ctx.load_cert_chain(self._config.extra.get("sslcert"),
                                    self._config.extra.get("sslkey"))
            try:
                sock = ctx.wrap_socket(raw_sock, server_hostname=self._config.host)
            except ssl.SSLError:
                if self._config.sslmode.lower() in ("allow", "prefer"):
                    sock = raw_sock
                else:
                    raw_sock.close()
                    raise
        self._socket = sock

        connect_msg = build_connect_request(
            self._config.database,
            self._config.application_name or "scratchbird_python",
            os.getpid(),
        )
        self._send(connect_msg)
        msg_type, payload = self._recv()
        if msg_type != MessageType.CONNECT_RESPONSE:
            raise errors.InterfaceError("unexpected response to CONNECT_REQUEST")
        success, session_id, _, _, _, err = parse_connect_response(payload)
        if not success:
            raise errors.InterfaceError(err or "connect failed")
        self._session_id = session_id
        self._authenticate()
        self._connected = True

    def close(self) -> None:
        if not self._closed:
            self._closed = True
            self._connected = False
            if self._socket:
                try:
                    self._socket.close()
                except OSError:
                    pass

    def commit(self) -> None:
        self._ensure_open()
        if not self._in_transaction:
            return
        self._send(build_commit(self._session_id))
        self._drain_until_complete()
        self._in_transaction = False

    def rollback(self) -> None:
        self._ensure_open()
        if not self._in_transaction:
            return
        from .protocol import build_rollback
        self._send(build_rollback(self._session_id))
        self._drain_until_complete()
        self._in_transaction = False

    def cursor(self) -> Cursor:
        self._ensure_open()
        cur = Cursor(self)
        self._cursors.append(cur)
        return cur

    def execute(self, sql: str, params=None) -> Cursor:
        cur = self.cursor()
        cur.execute(sql, params)
        return cur

    def executemany(self, sql: str, seq_of_params) -> Cursor:
        cur = self.cursor()
        cur.executemany(sql, seq_of_params)
        return cur

    def setinputsizes(self, sizes) -> None:
        self._ensure_open()

    def setoutputsize(self, size, column=None) -> None:
        self._ensure_open()

    def _ensure_open(self) -> None:
        if self._closed:
            raise errors.InterfaceError("connection is closed")

    @property
    def closed(self) -> bool:
        return self._closed

    @property
    def autocommit(self) -> bool:
        return self._autocommit

    @autocommit.setter
    def autocommit(self, value: bool) -> None:
        self._ensure_open()
        self._autocommit = bool(value)

    def _authenticate(self) -> None:
        auth_method = AuthMethod.SCRAM_SHA_256
        exchange = ScramExchange(self._config.user)
        client_first = exchange.client_first_message().encode("utf-8")

        msg = build_auth_request(self._session_id, self._config.user, auth_method, client_first)
        self._send(msg)
        msg_type, payload = self._recv()
        if msg_type != MessageType.AUTH_RESPONSE:
            raise errors.InterfaceError("unexpected response to AUTH_REQUEST")
        status, _, err, extra = parse_auth_response(payload)
        if status != AuthStatus.CONTINUE:
            raise errors.OperationalError(err or "auth failed")

        server_first = extra.decode("utf-8", errors="replace")
        client_final = exchange.handle_server_first(self._config.password or "", server_first)
        msg = build_auth_request(
            self._session_id,
            self._config.user,
            auth_method,
            client_final.encode("utf-8"),
        )
        self._send(msg)
        msg_type, payload = self._recv()
        if msg_type != MessageType.AUTH_RESPONSE:
            raise errors.InterfaceError("unexpected response to SCRAM final")
        status, _, err, extra = parse_auth_response(payload)
        if status != AuthStatus.OK:
            raise errors.OperationalError(err or "auth failed")
        if extra:
            exchange.verify_server_final(extra.decode("utf-8", errors="replace"))

    def _send(self, data: bytes) -> None:
        if not self._socket:
            raise errors.InterfaceError("no active socket")
        self._socket.sendall(data)

    def _recv(self):
        header = self._read_exact(12)
        _, msg_type, _, length = decode_header(header)
        payload = self._read_exact(length) if length else b""
        return msg_type, payload

    def _read_exact(self, n: int) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            chunk = self._socket.recv(n - len(buf))
            if not chunk:
                raise errors.OperationalError("connection closed")
            buf.extend(chunk)
        return bytes(buf)

    def _drain_until_complete(self):
        while True:
            msg_type, payload = self._recv()
            if msg_type == MessageType.QUERY_ERROR:
                self._raise_query_error(payload)
            if msg_type == MessageType.COMMAND_COMPLETE:
                return
            if msg_type == MessageType.END_OF_RESULTS:
                return

    def _execute_query(self, sql: str):
        if not self._autocommit:
            self._ensure_transaction()
        self._send(build_query(self._session_id, sql, 0))
        columns = []
        rows = []
        rowcount = -1
        rowcount_hint = -1
        while True:
            msg_type, payload = self._recv()
            if msg_type == MessageType.QUERY_ERROR:
                self._raise_query_error(payload)
            if msg_type == MessageType.QUERY_RESULT:
                _, _, rowcount_hint = parse_query_result(payload)
            if msg_type == MessageType.ROW_DESCRIPTION:
                columns = parse_row_description(payload)
            elif msg_type == MessageType.ROW_DATA:
                values = parse_row_data(payload)
                decoded = []
                for idx, value in enumerate(values):
                    wire_type = columns[idx].wire_type if idx < len(columns) else 0
                    decoded.append(decode_value(wire_type, value.data))
                rows.append(tuple(decoded))
            elif msg_type == MessageType.COMMAND_COMPLETE:
                _, rowcount = parse_command_complete(payload)
            elif msg_type == MessageType.END_OF_RESULTS:
                break
        if rowcount < 0 and rowcount_hint >= 0:
            rowcount = rowcount_hint
        return columns, rows, rowcount

    def _ensure_transaction(self) -> None:
        if self._in_transaction:
            return
        from .protocol import build_begin
        self._send(build_begin(self._session_id))
        self._drain_until_complete()
        self._in_transaction = True

    def _raise_query_error(self, payload: bytes) -> None:
        try:
            error_code, sqlstate, message, detail, hint = parse_query_error(payload)
        except ValueError:
            raise errors.DatabaseError("query failed") from None
        parts = []
        if message:
            parts.append(message)
        if detail:
            parts.append(f"DETAIL: {detail}")
        if hint:
            parts.append(f"HINT: {hint}")
        text = "\n".join(parts) if parts else "query failed"
        if sqlstate:
            text = f"[{sqlstate}] {text}"
        raise errors.DatabaseError(text)
