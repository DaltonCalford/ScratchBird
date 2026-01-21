from __future__ import annotations

import ipaddress
import uuid

from scratchbird.sql import substitute_parameters


def test_substitute_positional_array():
    sql = "SELECT ?"
    rendered = substitute_parameters(sql, ([1, "alpha", None],))
    assert rendered == "SELECT ARRAY[1, 'alpha', NULL]"


def test_substitute_named_uuid_and_inet():
    value = uuid.UUID("12345678-1234-5678-1234-567812345678")
    addr = ipaddress.ip_address("192.168.1.10")
    sql = "SELECT :id, :addr"
    rendered = substitute_parameters(sql, {"id": value, "addr": addr})
    assert rendered == "SELECT UUID '12345678-1234-5678-1234-567812345678', INET '192.168.1.10'"


def test_substitute_json_dict():
    sql = "SELECT :payload"
    rendered = substitute_parameters(sql, {"payload": {"a": 1, "b": "x"}})
    assert rendered == "SELECT JSON '{\"a\": 1, \"b\": \"x\"}'"
