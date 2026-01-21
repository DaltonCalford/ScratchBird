from __future__ import annotations

from scratchbird.protocol import WireType
from scratchbird.types import decode_value


def test_decode_array_literal():
    raw = b"{1, 2, 3}"
    assert decode_value(WireType.ARRAY, raw) == [1, 2, 3]


def test_decode_nested_array_literal():
    raw = b"{1, {2, 3}, NULL}"
    assert decode_value(WireType.ARRAY, raw) == [1, [2, 3], None]


def test_decode_vector_literal():
    raw = b"[1, 2.5]"
    assert decode_value(WireType.VECTOR, raw) == [1.0, 2.5]
