"""Type helpers and defaults."""

from __future__ import annotations

import datetime as _dt
import decimal as _decimal
import ipaddress as _ip
import struct
import uuid
from typing import Any

from .protocol import WireType

DATE = _dt.date
TIME = _dt.time
TIMESTAMP = _dt.datetime
DECIMAL = _decimal.Decimal


def adapt_param(value: Any) -> Any:
    return value


def decode_value(wire_type: int, data: bytes):
    if data is None:
        return None
    if wire_type == WireType.BOOLEAN:
        return data[:1] == b"\x01"
    if wire_type == WireType.INT16:
        return struct.unpack_from("<h", data)[0]
    if wire_type == WireType.INT32:
        return struct.unpack_from("<i", data)[0]
    if wire_type == WireType.INT64:
        return struct.unpack_from("<q", data)[0]
    if wire_type == WireType.FLOAT32:
        return struct.unpack_from("<f", data)[0]
    if wire_type == WireType.FLOAT64:
        return struct.unpack_from("<d", data)[0]
    if wire_type in (WireType.VARCHAR, WireType.CHAR, WireType.JSON, WireType.XML,
                     WireType.TSVECTOR, WireType.TSQUERY):
        return data.decode("utf-8", errors="replace")
    if wire_type == WireType.JSONB:
        return data.decode("utf-8", errors="replace")
    if wire_type == WireType.DECIMAL:
        return _decimal.Decimal(data.decode("utf-8", errors="replace"))
    if wire_type == WireType.BYTEA:
        return data
    if wire_type == WireType.DATE:
        days = struct.unpack_from("<i", data)[0]
        return _dt.date(2000, 1, 1) + _dt.timedelta(days=days)
    if wire_type == WireType.TIME:
        micros = struct.unpack_from("<q", data)[0]
        secs, micro = divmod(micros, 1_000_000)
        hours, rem = divmod(secs, 3600)
        mins, secs = divmod(rem, 60)
        return _dt.time(int(hours % 24), int(mins), int(secs), int(micro))
    if wire_type == WireType.TIMESTAMP:
        micros = struct.unpack_from("<q", data)[0]
        return _dt.datetime.utcfromtimestamp(micros / 1_000_000)
    if wire_type == WireType.TIMESTAMPTZ:
        micros = struct.unpack_from("<q", data)[0]
        offset = struct.unpack_from("<h", data, 8)[0] if len(data) >= 10 else 0
        tz = _dt.timezone(_dt.timedelta(minutes=offset))
        return _dt.datetime.fromtimestamp(micros / 1_000_000, tz=tz)
    if wire_type == WireType.INTERVAL:
        months = struct.unpack_from("<i", data)[0]
        days = struct.unpack_from("<i", data, 4)[0]
        micros = struct.unpack_from("<q", data, 8)[0]
        return {"months": months, "days": days, "micros": micros}
    if wire_type == WireType.UUID:
        return uuid.UUID(bytes=data[:16])
    if wire_type == WireType.MONEY:
        cents = struct.unpack_from("<q", data)[0]
        return _decimal.Decimal(cents) / _decimal.Decimal(100)
    if wire_type == WireType.INET:
        return _ip.ip_address(data.decode("utf-8", errors="replace"))
    if wire_type == WireType.CIDR:
        return _ip.ip_network(data.decode("utf-8", errors="replace"), strict=False)
    if wire_type == WireType.ARRAY:
        text = data.decode("utf-8", errors="replace")
        return _parse_array_literal(text)
    if wire_type == WireType.VECTOR:
        text = data.decode("utf-8", errors="replace")
        return _parse_vector_literal(text)
    return data


def _parse_array_literal(text: str):
    text = text.strip()
    if text == "" or text == "{}":
        return []
    if text.startswith("{") and text.endswith("}"):
        text = text[1:-1]
    return _split_array_items(text)


def _split_array_items(text: str):
    items = []
    buf = []
    depth = 0
    i = 0
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
            buf.append(ch)
        elif ch == "}":
            depth = max(0, depth - 1)
            buf.append(ch)
        elif ch == "," and depth == 0:
            items.append(_parse_array_item("".join(buf)))
            buf = []
        else:
            buf.append(ch)
        i += 1
    if buf or text:
        items.append(_parse_array_item("".join(buf)))
    return items


def _parse_array_item(raw: str):
    token = raw.strip()
    if token == "":
        return ""
    if token.upper() == "NULL":
        return None
    if token.startswith("{") and token.endswith("}"):
        return _parse_array_literal(token)
    if token.startswith("[") and token.endswith("]"):
        return _parse_vector_literal(token)
    if token.lower() in ("true", "false"):
        return token.lower() == "true"
    try:
        if "." in token:
            return float(token)
        return int(token)
    except ValueError:
        return token


def _parse_vector_literal(text: str):
    text = text.strip()
    if text.startswith("[") and text.endswith("]"):
        text = text[1:-1]
    if text.strip() == "":
        return []
    parts = [part.strip() for part in text.split(",")]
    values = []
    for part in parts:
        if part == "":
            continue
        try:
            values.append(float(part))
        except ValueError:
            values.append(part)
    return values
