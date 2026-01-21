"""SQL parameter substitution helpers."""

from __future__ import annotations

import datetime as _dt
import decimal as _decimal
import json as _json
import uuid as _uuid
import ipaddress as _ip


def _escape_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace("'", "''")


def _format_param(value) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, bool):
        return "TRUE" if value else "FALSE"
    if isinstance(value, (int, float, _decimal.Decimal)):
        return str(value)
    if isinstance(value, _uuid.UUID):
        return f"UUID '{value}'"
    if isinstance(value, (_ip.IPv4Address, _ip.IPv6Address)):
        return f"INET '{value}'"
    if isinstance(value, (_ip.IPv4Network, _ip.IPv6Network)):
        return f"CIDR '{value}'"
    if isinstance(value, (bytes, bytearray, memoryview)):
        hex_str = bytes(value).hex().upper()
        return f"X'{hex_str}'"
    if isinstance(value, _dt.date) and not isinstance(value, _dt.datetime):
        return f"DATE '{value.isoformat()}'"
    if isinstance(value, _dt.time):
        return f"TIME '{value.isoformat()}'"
    if isinstance(value, _dt.datetime):
        return f"TIMESTAMP '{value.isoformat(sep=' ')}'"
    if isinstance(value, (list, tuple)):
        return _format_array(value)
    if isinstance(value, dict):
        encoded = _json.dumps(value, ensure_ascii=True)
        return f"JSON '{_escape_string(encoded)}'"
    return f"'{_escape_string(str(value))}'"


def _format_array(values) -> str:
    items = []
    for item in values:
        if isinstance(item, (list, tuple)):
            items.append(_format_array(item))
        else:
            items.append(_format_param(item))
    return "ARRAY[" + ", ".join(items) + "]"


def substitute_parameters(sql: str, params) -> str:
    if params is None:
        return sql

    if isinstance(params, dict):
        return _substitute_named(sql, params)

    values = list(params)
    return _substitute_positional(sql, values)


def _substitute_named(sql: str, params: dict) -> str:
    result = []
    i = 0
    while i < len(sql):
        ch = sql[i]
        if ch == "'" and i + 1 < len(sql):
            result.append(ch)
            i += 1
            while i < len(sql):
                result.append(sql[i])
                if sql[i] == "'" and (i + 1 >= len(sql) or sql[i + 1] != "'"):
                    i += 1
                    break
                if sql[i] == "'" and i + 1 < len(sql) and sql[i + 1] == "'":
                    i += 1
                i += 1
            continue
        if ch == ":" and i + 1 < len(sql) and sql[i + 1].isidentifier():
            j = i + 1
            while j < len(sql) and (sql[j].isalnum() or sql[j] == "_"):
                j += 1
            key = sql[i + 1 : j]
            if key in params:
                result.append(_format_param(params[key]))
            else:
                result.append(sql[i:j])
            i = j
            continue
        result.append(ch)
        i += 1
    return "".join(result)


def _substitute_positional(sql: str, values: list) -> str:
    result = []
    i = 0
    next_param = 0
    while i < len(sql):
        ch = sql[i]
        if ch == "$" and i + 1 < len(sql) and sql[i + 1].isdigit():
            j = i + 1
            num = 0
            while j < len(sql) and sql[j].isdigit():
                num = num * 10 + int(sql[j])
                j += 1
            if 0 < num <= len(values):
                result.append(_format_param(values[num - 1]))
            else:
                result.append(sql[i:j])
            i = j
            continue
        if ch == "?":
            if next_param < len(values):
                result.append(_format_param(values[next_param]))
                next_param += 1
            else:
                result.append(ch)
            i += 1
            continue
        if ch == "'" and i + 1 < len(sql):
            result.append(ch)
            i += 1
            while i < len(sql):
                result.append(sql[i])
                if sql[i] == "'" and (i + 1 >= len(sql) or sql[i + 1] != "'"):
                    i += 1
                    break
                if sql[i] == "'" and i + 1 < len(sql) and sql[i + 1] == "'":
                    i += 1
                i += 1
            continue
        if ch == "-" and i + 1 < len(sql) and sql[i + 1] == "-":
            while i < len(sql) and sql[i] != "\n":
                result.append(sql[i])
                i += 1
            continue
        if ch == "/" and i + 1 < len(sql) and sql[i + 1] == "*":
            result.append(ch)
            result.append(sql[i + 1])
            i += 2
            while i + 1 < len(sql) and not (sql[i] == "*" and sql[i + 1] == "/"):
                result.append(sql[i])
                i += 1
            if i + 1 < len(sql):
                result.append(sql[i])
                result.append(sql[i + 1])
                i += 2
            continue
        result.append(ch)
        i += 1
    return "".join(result)
