#!/usr/bin/env python3
"""
Validate charset mapping tables against iconv and system charmaps.
"""
from __future__ import annotations

import gzip
import json
import os
import re
import subprocess
from typing import Dict, List, Tuple

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MAPPING_DIR = os.path.join(ROOT, "charsets", "mappings")

CHARMAP_SOURCES = {
    "DOS858": "/usr/share/i18n/charmaps/IBM858.gz",
    "EUC-JP": "/usr/share/i18n/charmaps/EUC-JP.gz",
    "GB_2312": "/usr/share/i18n/charmaps/GB2312.gz",
    "NEXT": "/usr/share/i18n/charmaps/NEXTSTEP.gz"
}

FILE_OVERRIDES = {
    "GB_2312": "gb-2312.map.json"
}

ICONV_SOURCES = {
    "CP943C": "IBM943"
}


def parse_charmap(path: str) -> Dict[str, str]:
    mapping: Dict[str, str] = {}
    escape_char = "/"
    in_charmap = False
    open_fn = gzip.open if path.endswith(".gz") else open
    with open_fn(path, "rt", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.split("%", 1)[0].strip()
            if not line:
                continue
            if line.startswith("<escape_char>"):
                parts = line.split()
                if parts:
                    escape_char = parts[-1]
                continue
            if line == "CHARMAP":
                in_charmap = True
                continue
            if line == "END CHARMAP":
                break
            if not in_charmap:
                continue
            match = re.match(r"<U([0-9A-Fa-f]{4,8})>\s+(.+)$", line)
            if not match:
                continue
            codepoint = match.group(1).upper().zfill(4)
            bytes_part = match.group(2)
            pattern = re.escape(escape_char) + r"x([0-9A-Fa-f]{2})"
            bytes_list = re.findall(pattern, bytes_part)
            if not bytes_list:
                continue
            byte_sequence = " ".join(f"0x{b.upper()}" for b in bytes_list)
            mapping[byte_sequence] = f"U+{codepoint}"
    return mapping


def decode_iconv(encoding: str, data: bytes) -> str | None:
    result = subprocess.run(
        ["iconv", "-f", encoding, "-t", "UTF-8"],
        input=data,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    if result.returncode != 0 or not result.stdout:
        return None
    try:
        text = result.stdout.decode("utf-8")
    except UnicodeDecodeError:
        return None
    if len(text) != 1:
        return None
    return f"U+{ord(text):04X}"


def parse_iconv(encoding: str) -> Dict[str, str]:
    mapping: Dict[str, str] = {}
    for b in range(0x00, 0x100):
        codepoint = decode_iconv(encoding, bytes([b]))
        if codepoint:
            mapping[f"0x{b:02X}"] = codepoint
    lead_ranges = [(0x81, 0x9F), (0xE0, 0xFC)]
    trail_ranges = [(0x40, 0x7E), (0x80, 0xFC)]
    for start, end in lead_ranges:
        for lead in range(start, end + 1):
            for t_start, t_end in trail_ranges:
                for trail in range(t_start, t_end + 1):
                    codepoint = decode_iconv(encoding, bytes([lead, trail]))
                    if codepoint:
                        mapping[f"0x{lead:02X} 0x{trail:02X}"] = codepoint
    return mapping


def load_mapping(path: str) -> Dict[str, str]:
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    return {m["byte_sequence"]: m["unicode_codepoint"] for m in data.get("mappings", [])}


def compare_maps(name: str, expected: Dict[str, str], actual: Dict[str, str]) -> Tuple[List[str], int, int, int]:
    errors = []
    missing = 0
    mismatch = 0
    for key, value in expected.items():
        if key not in actual:
            errors.append(f"{name}: missing {key}")
            missing += 1
            continue
        if actual[key] != value:
            errors.append(f"{name}: mismatch {key} {actual[key]} != {value}")
            mismatch += 1
    extra = max(0, len(actual) - len(expected))
    return errors, missing, mismatch, extra


def validate_charmap(name: str, path: str) -> Tuple[List[str], Dict[str, int]]:
    expected = parse_charmap(path)
    filename = FILE_OVERRIDES.get(name, f"{name.lower()}.map.json")
    actual = load_mapping(os.path.join(MAPPING_DIR, filename))
    errors, missing, mismatch, extra = compare_maps(name, expected, actual)
    report = {
        "expected": len(expected),
        "actual": len(actual),
        "missing": missing,
        "mismatch": mismatch,
        "extra": extra
    }
    return errors, report


def validate_iconv(name: str, encoding: str) -> Tuple[List[str], Dict[str, int]]:
    expected = parse_iconv(encoding)
    filename = FILE_OVERRIDES.get(name, f"{name.lower()}.map.json")
    actual = load_mapping(os.path.join(MAPPING_DIR, filename))
    errors, missing, mismatch, extra = compare_maps(name, expected, actual)
    report = {
        "expected": len(expected),
        "actual": len(actual),
        "missing": missing,
        "mismatch": mismatch,
        "extra": extra
    }
    return errors, report


def main() -> None:
    errors: List[str] = []
    reports: Dict[str, Dict[str, int]] = {}
    for name, path in CHARMAP_SOURCES.items():
        if not os.path.exists(path):
            errors.append(f"{name}: missing charmap source {path}")
            continue
        errs, report = validate_charmap(name, path)
        errors.extend(errs)
        reports[name] = report
    for name, encoding in ICONV_SOURCES.items():
        errs, report = validate_iconv(name, encoding)
        errors.extend(errs)
        reports[name] = report
    if reports:
        print("Validation summary:")
        for name in sorted(reports.keys()):
            r = reports[name]
            print(f"- {name}: expected={r['expected']} actual={r['actual']} missing={r['missing']} mismatch={r['mismatch']} extra={r['extra']}")
    if errors:
        print("Validation errors:")
        for err in errors[:200]:
            print("-", err)
        if len(errors) > 200:
            print(f"... {len(errors) - 200} more")
        raise SystemExit(1)
    print("All mapping tables validated") 


if __name__ == "__main__":
    main()
