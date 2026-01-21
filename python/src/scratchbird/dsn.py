"""DSN parsing helpers for ScratchBird Python driver."""

from __future__ import annotations

import urllib.parse


def parse_dsn(dsn: str | None) -> dict:
    if not dsn:
        return {}

    if "://" in dsn:
        return _parse_uri(dsn)
    return _parse_kv(dsn)


def _parse_uri(dsn: str) -> dict:
    parsed = urllib.parse.urlparse(dsn)
    if parsed.scheme not in ("scratchbird", "sb"):
        raise ValueError(f"Unsupported DSN scheme: {parsed.scheme}")

    params = {}
    if parsed.hostname:
        params["host"] = parsed.hostname
    if parsed.port:
        params["port"] = parsed.port
    if parsed.username:
        params["user"] = urllib.parse.unquote(parsed.username)
    if parsed.password:
        params["password"] = urllib.parse.unquote(parsed.password)
    if parsed.path and parsed.path != "/":
        params["database"] = parsed.path.lstrip("/")

    query = urllib.parse.parse_qs(parsed.query, keep_blank_values=True)
    for key, values in query.items():
        if values:
            params[key] = values[-1]
    return params


def _parse_kv(dsn: str) -> dict:
    params = {}
    tokens = dsn.split()
    for token in tokens:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        params[key.strip()] = value.strip()
    return params
