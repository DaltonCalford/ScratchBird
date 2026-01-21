"""ScratchBird wire protocol encoding/decoding (v1.0)."""

from __future__ import annotations

import os
import struct
from dataclasses import dataclass
from typing import List, Optional

PROTOCOL_MAGIC = 0x42444253  # "SBDB"
PROTOCOL_VERSION_MAJOR = 1
PROTOCOL_VERSION_MINOR = 0
PROTOCOL_VERSION = (PROTOCOL_VERSION_MAJOR << 8) | PROTOCOL_VERSION_MINOR

MAX_MESSAGE_SIZE = 16 * 1024 * 1024


class MessageType:
    CONNECT_REQUEST = 0x01
    CONNECT_RESPONSE = 0x02
    DISCONNECT = 0x03

    AUTH_REQUEST = 0x10
    AUTH_RESPONSE = 0x11

    QUERY = 0x20
    QUERY_RESULT = 0x21
    QUERY_ERROR = 0x22
    QUERY_CANCEL = 0x23

    PREPARE = 0x30
    PREPARE_RESPONSE = 0x31
    EXECUTE = 0x32
    CLOSE_STATEMENT = 0x33
    DESCRIBE = 0x34
    DESCRIBE_RESPONSE = 0x35

    BEGIN_TRANSACTION = 0x40
    COMMIT = 0x41
    ROLLBACK = 0x42
    SAVEPOINT = 0x43
    RELEASE_SAVEPOINT = 0x44
    ROLLBACK_TO = 0x45
    TRANSACTION_STATUS = 0x46

    ROW_DESCRIPTION = 0x50
    ROW_DATA = 0x51
    END_OF_RESULTS = 0x52
    COMMAND_COMPLETE = 0x53

    COPY_DATA = 0x70
    COPY_DONE = 0x71
    COPY_FAIL = 0x72
    COPY_IN_RESPONSE = 0x73
    COPY_OUT_RESPONSE = 0x74
    COPY_BOTH_RESPONSE = 0x75
    STREAM_CONTROL = 0x76
    STREAM_READY = 0x77
    STREAM_DATA = 0x78
    STREAM_END = 0x79


class AuthMethod:
    PASSWORD = 0
    MD5 = 1
    SCRAM_SHA_256 = 2
    SCRAM_SHA_512 = 3


class AuthStatus:
    OK = 0
    ERROR = 1
    CONTINUE = 2


class WireType:
    NULL_TYPE = 0x00
    BOOLEAN = 0x01
    INT16 = 0x02
    INT32 = 0x03
    INT64 = 0x04
    FLOAT32 = 0x05
    FLOAT64 = 0x06
    DECIMAL = 0x07
    VARCHAR = 0x08
    CHAR = 0x09
    BYTEA = 0x0A
    DATE = 0x0B
    TIME = 0x0C
    TIMESTAMP = 0x0D
    TIMESTAMPTZ = 0x0E
    INTERVAL = 0x0F
    UUID = 0x10
    JSON = 0x11
    JSONB = 0x12
    ARRAY = 0x13
    COMPOSITE = 0x14
    GEOMETRY = 0x15
    VECTOR = 0x16
    MONEY = 0x17
    XML = 0x18
    INET = 0x19
    CIDR = 0x1A
    MACADDR = 0x1B
    TSVECTOR = 0x1C
    TSQUERY = 0x1D
    RANGE = 0x1E
    UNKNOWN = 0xFF


_HEADER_STRUCT = struct.Struct("<I H B B I")


@dataclass
class ColumnInfo:
    name: str
    wire_type: int
    type_modifier: int
    format_code: int


@dataclass
class ColumnValue:
    data: Optional[bytes]

    @property
    def is_null(self) -> bool:
        return self.data is None


def encode_message(msg_type: int, payload: bytes, flags: int = 0) -> bytes:
    header = _HEADER_STRUCT.pack(
        PROTOCOL_MAGIC,
        PROTOCOL_VERSION,
        msg_type,
        flags,
        len(payload),
    )
    return header + payload


def decode_header(data: bytes):
    if len(data) != _HEADER_STRUCT.size:
        raise ValueError("invalid header length")
    magic, version, msg_type, flags, length = _HEADER_STRUCT.unpack(data)
    if magic != PROTOCOL_MAGIC:
        raise ValueError("invalid protocol magic")
    if length > MAX_MESSAGE_SIZE:
        raise ValueError("payload too large")
    return version, msg_type, flags, length


def write_null_terminated(value: str, max_len: int) -> bytes:
    encoded = value.encode("utf-8")
    if len(encoded) >= max_len:
        encoded = encoded[: max_len - 1]
    return encoded + b"\x00" * (max_len - len(encoded))


def read_null_terminated(data: bytes) -> str:
    if b"\x00" in data:
        data = data.split(b"\x00", 1)[0]
    return data.decode("utf-8", errors="replace")


def build_connect_request(database: str, client_name: str, client_pid: int) -> bytes:
    payload = bytearray()
    payload += struct.pack("<H", PROTOCOL_VERSION)
    payload += struct.pack("<H", 0)
    payload += struct.pack("<I", client_pid)
    payload += write_null_terminated(database, 256)
    payload += write_null_terminated(client_name, 64)
    payload += write_null_terminated("1.0.0", 32)
    return encode_message(MessageType.CONNECT_REQUEST, bytes(payload))


def parse_connect_response(payload: bytes):
    if len(payload) < 1 + 2 + 2 + 16 + 64 + 32:
        raise ValueError("connect response truncated")
    offset = 0
    status = payload[offset]
    offset += 1
    version = struct.unpack_from("<H", payload, offset)[0]
    offset += 2
    offset += 2  # flags
    session_id = payload[offset : offset + 16]
    offset += 16
    server_name = read_null_terminated(payload[offset : offset + 64])
    offset += 64
    server_version = read_null_terminated(payload[offset : offset + 32])
    offset += 32
    error_message = ""
    if status != 0 and offset + 2 <= len(payload):
        msg_len = struct.unpack_from("<H", payload, offset)[0]
        offset += 2
        error_message = payload[offset : offset + msg_len].decode("utf-8", errors="replace")
    return status == 0, session_id, version, server_name, server_version, error_message


def build_auth_request(session_id: bytes, username: str, auth_method: int, payload: bytes) -> bytes:
    if len(session_id) != 16:
        raise ValueError("session_id must be 16 bytes")
    out = bytearray()
    out += session_id
    out += write_null_terminated(username, 64)
    out += struct.pack("<B", auth_method)
    out += struct.pack("<H", len(payload))
    out += payload
    return encode_message(MessageType.AUTH_REQUEST, bytes(out))


def parse_auth_response(payload: bytes):
    if len(payload) < 1 + 4 + 256:
        raise ValueError("auth response truncated")
    status = payload[0]
    user_id = struct.unpack_from("<I", payload, 1)[0]
    error_message = read_null_terminated(payload[5 : 5 + 256])
    extra = payload[5 + 256 :]
    return status, user_id, error_message, extra


def build_query(session_id: bytes, sql: str, flags: int = 0) -> bytes:
    if len(session_id) != 16:
        raise ValueError("session_id must be 16 bytes")
    sql_bytes = sql.encode("utf-8")
    payload = bytearray()
    payload += session_id
    payload += struct.pack("<I", len(sql_bytes))
    payload += struct.pack("<B", flags)
    payload += sql_bytes
    return encode_message(MessageType.QUERY, bytes(payload))


def parse_row_description(payload: bytes) -> List[ColumnInfo]:
    if len(payload) < 2:
        raise ValueError("row description truncated")
    offset = 0
    column_count = struct.unpack_from("<H", payload, offset)[0]
    offset += 2
    columns: List[ColumnInfo] = []
    for _ in range(column_count):
        if offset + 2 > len(payload):
            raise ValueError("row description truncated")
        name_len = struct.unpack_from("<H", payload, offset)[0]
        offset += 2
        name = payload[offset : offset + name_len].decode("utf-8", errors="replace")
        offset += name_len
        wire_type = payload[offset]
        offset += 1
        type_modifier = struct.unpack_from("<I", payload, offset)[0]
        offset += 4
        format_code = struct.unpack_from("<H", payload, offset)[0]
        offset += 2
        columns.append(ColumnInfo(name, wire_type, type_modifier, format_code))
    return columns


def parse_row_data(payload: bytes) -> List[ColumnValue]:
    if len(payload) < 2:
        raise ValueError("row data truncated")
    offset = 0
    count = struct.unpack_from("<H", payload, offset)[0]
    offset += 2
    values: List[ColumnValue] = []
    for _ in range(count):
        if offset + 4 > len(payload):
            raise ValueError("row data truncated")
        length = struct.unpack_from("<i", payload, offset)[0]
        offset += 4
        if length < 0:
            values.append(ColumnValue(None))
            continue
        if offset + length > len(payload):
            raise ValueError("row data truncated")
        data = payload[offset : offset + length]
        offset += length
        values.append(ColumnValue(data))
    return values


def parse_command_complete(payload: bytes):
    if len(payload) < 64 + 8:
        raise ValueError("command complete truncated")
    tag = read_null_terminated(payload[:64])
    rows_affected = struct.unpack_from("<q", payload, 64)[0]
    return tag, rows_affected


def parse_query_result(payload: bytes):
    if len(payload) < 1 + 4 + 8:
        raise ValueError("query result truncated")
    status = payload[0]
    column_count = struct.unpack_from("<I", payload, 1)[0]
    row_count = struct.unpack_from("<q", payload, 5)[0]
    return status, column_count, row_count


def parse_query_error(payload: bytes):
    if len(payload) < 4 + 6 + 2 + 2 + 2:
        raise ValueError("query error truncated")
    offset = 0
    error_code = struct.unpack_from("<I", payload, offset)[0]
    offset += 4
    sqlstate = read_null_terminated(payload[offset : offset + 6])
    offset += 6
    message_len = struct.unpack_from("<H", payload, offset)[0]
    offset += 2
    detail_len = struct.unpack_from("<H", payload, offset)[0]
    offset += 2
    hint_len = struct.unpack_from("<H", payload, offset)[0]
    offset += 2
    if offset + message_len + detail_len + hint_len > len(payload):
        raise ValueError("query error truncated")
    message = payload[offset : offset + message_len].decode("utf-8", errors="replace")
    offset += message_len
    detail = payload[offset : offset + detail_len].decode("utf-8", errors="replace")
    offset += detail_len
    hint = payload[offset : offset + hint_len].decode("utf-8", errors="replace")
    return error_code, sqlstate, message, detail, hint


def build_commit(session_id: bytes) -> bytes:
    return encode_message(MessageType.COMMIT, session_id)


def build_rollback(session_id: bytes) -> bytes:
    return encode_message(MessageType.ROLLBACK, session_id)


def build_begin(session_id: bytes, isolation_level: int = 0, read_only: bool = False) -> bytes:
    payload = bytearray()
    payload += session_id
    payload += struct.pack("<B", isolation_level)
    payload += struct.pack("<B", 1 if read_only else 0)
    return encode_message(MessageType.BEGIN_TRANSACTION, bytes(payload))


def build_disconnect(session_id: bytes) -> bytes:
    return encode_message(MessageType.DISCONNECT, session_id)
