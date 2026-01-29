#!/usr/bin/env python3
"""
Download Unicode mapping tables and emit ScratchBird charset mapping JSON files.
"""
from __future__ import annotations

import argparse
import gzip
import json
import os
import re
import subprocess
import urllib.request
from typing import Dict, List, Tuple

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CHARSETS_JSON = os.path.join(ROOT, "charsets", "charsets.json")
MAPPING_DIR = os.path.join(ROOT, "charsets", "mappings")

USER_AGENT = "ScratchBird-Mappings/1.0"

CHARMAP_PREFIX = "charmap:"
ICONV_PREFIX = "iconv:"

MAPPING_SOURCES: List[Tuple[str, str]] = [
    # ISO-8859
    ("ISO-8859-1", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-1.TXT"),
    ("ISO-8859-2", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-2.TXT"),
    ("ISO-8859-3", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-3.TXT"),
    ("ISO-8859-4", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-4.TXT"),
    ("ISO-8859-5", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-5.TXT"),
    ("ISO-8859-6", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-6.TXT"),
    ("ISO-8859-7", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-7.TXT"),
    ("ISO-8859-8", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-8.TXT"),
    ("ISO-8859-9", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-9.TXT"),
    ("ISO-8859-10", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-10.TXT"),
    ("ISO-8859-11", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-11.TXT"),
    ("ISO-8859-13", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-13.TXT"),
    ("ISO-8859-14", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-14.TXT"),
    ("ISO-8859-15", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-15.TXT"),
    ("ISO-8859-16", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-16.TXT"),
    # Windows code pages
    ("Windows-1250", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1250.TXT"),
    ("Windows-1251", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1251.TXT"),
    ("Windows-1252", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT"),
    ("Windows-1253", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1253.TXT"),
    ("Windows-1254", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1254.TXT"),
    ("Windows-1255", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1255.TXT"),
    ("Windows-1256", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1256.TXT"),
    ("Windows-1257", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1257.TXT"),
    ("Windows-1258", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1258.TXT"),
    ("cp1250", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1250.TXT"),
    ("cp1251", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1251.TXT"),
    ("cp1256", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1256.TXT"),
    ("cp1257", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1257.TXT"),
    # DOS code pages (Firebird)
    ("DOS437", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP437.TXT"),
    ("DOS737", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP737.TXT"),
    ("DOS775", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP775.TXT"),
    ("DOS850", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP850.TXT"),
    ("DOS852", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP852.TXT"),
    ("DOS857", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP857.TXT"),
    ("DOS858", f"{CHARMAP_PREFIX}/usr/share/i18n/charmaps/IBM858.gz"),
    ("DOS860", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP860.TXT"),
    ("DOS861", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP861.TXT"),
    ("DOS862", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP862.TXT"),
    ("DOS863", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP863.TXT"),
    ("DOS864", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP864.TXT"),
    ("DOS865", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP865.TXT"),
    ("DOS866", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP866.TXT"),
    ("DOS869", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP869.TXT"),
    ("cp850", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP850.TXT"),
    ("cp852", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP852.TXT"),
    ("cp866", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP866.TXT"),
    # KOI8
    ("KOI8-R", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MISC/KOI8-R.TXT"),
    ("KOI8-U", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MISC/KOI8-U.TXT"),
    # Mac encodings
    ("MACROMAN", "https://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT"),
    ("MACCE", "https://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CENTEURO.TXT"),
    # Shift-JIS/GB/EUC
    ("CP943C", f"{ICONV_PREFIX}IBM943"),
    ("Shift_JIS", "https://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/JIS/SHIFTJIS.TXT"),
    ("GB_2312", f"{CHARMAP_PREFIX}/usr/share/i18n/charmaps/GB2312.gz"),
    ("GBK", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP936.TXT"),
    ("Big5", "https://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER/BIG5.TXT"),
    ("EUC-JP", f"{CHARMAP_PREFIX}/usr/share/i18n/charmaps/EUC-JP.gz"),
    ("EUC-KR", "https://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/KSC/KSC5601.TXT"),
    ("GB18030", f"{CHARMAP_PREFIX}/usr/share/i18n/charmaps/GB18030.gz"),
    ("cp932", "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP932.TXT"),
    ("TIS-620", "https://www.unicode.org/Public/MAPPINGS/ISO8859/8859-11.TXT"),
]

DESCRIPTION_OVERRIDES = {
    "DOS858": "Mapping table parsed from system IBM858 charmap.",
    "CP943C": "Mapping table generated via iconv IBM943 (CP943C-compatible).",
    "GB_2312": "Mapping table parsed from system GB2312 charmap.",
    "EUC-JP": "Mapping table parsed from system EUC-JP charmap.",
    "GB18030": "Mapping table parsed from system GB18030 charmap."
}


def load_charsets() -> Dict[str, dict]:
    with open(CHARSETS_JSON, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    return {c["name"]: c for c in data.get("character_sets", [])}


def normalize_filename(charset: str) -> str:
    value = charset.lower().replace("_", "-")
    value = re.sub(r"[^a-z0-9\\-]+", "-", value).strip("-")
    return f"{value}.map.json"


def load_text(source: str) -> Tuple[str, str]:
    if source.startswith(CHARMAP_PREFIX):
        path = source[len(CHARMAP_PREFIX):]
        if path.endswith(".gz"):
            with gzip.open(path, "rt", encoding="utf-8", errors="replace") as fh:
                return fh.read(), "charmap"
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            return fh.read(), "charmap"
    if source.startswith(ICONV_PREFIX):
        return source[len(ICONV_PREFIX):], "iconv"
    req = urllib.request.Request(source, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req) as resp:
        return resp.read().decode("utf-8", errors="replace"), "unicode"


def download(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req) as resp:
        return resp.read().decode("utf-8", errors="replace")


def parse_mapping(text: str) -> List[dict]:
    mapping: Dict[str, str] = {}
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        tokens = [t for t in line.split() if t.lower().startswith("0x")]
        if len(tokens) < 2:
            continue
        unicode_token = tokens[-1][2:]
        if not unicode_token:
            continue
        src_tokens = tokens[:-1]
        bytes_list: List[str] = []
        for token in src_tokens:
            hexstr = token[2:]
            if len(hexstr) % 2 != 0:
                bytes_list = []
                break
            for i in range(0, len(hexstr), 2):
                bytes_list.append(hexstr[i:i + 2].upper())
        if not bytes_list:
            continue
        byte_sequence = " ".join(f"0x{b}" for b in bytes_list)
        codepoint = unicode_token.upper().zfill(4)
        mapping[byte_sequence] = f"U+{codepoint}"
    return [
        {"byte_sequence": key, "unicode_codepoint": value}
        for key, value in sorted(mapping.items())
    ]


def parse_charmap(text: str) -> List[dict]:
    mapping: Dict[str, str] = {}
    escape_char = "/"
    in_charmap = False
    for raw in text.splitlines():
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
            bytes_list = re.findall(r"0x([0-9A-Fa-f]{2})", bytes_part)
        if not bytes_list:
            continue
        byte_sequence = " ".join(f"0x{b.upper()}" for b in bytes_list)
        mapping[byte_sequence] = f"U+{codepoint}"
    return [
        {"byte_sequence": key, "unicode_codepoint": value}
        for key, value in sorted(mapping.items())
    ]


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


def parse_iconv(encoding: str) -> List[dict]:
    mapping: Dict[str, str] = {}
    # Single-byte sequences
    for b in range(0x00, 0x100):
        codepoint = decode_iconv(encoding, bytes([b]))
        if codepoint:
            mapping[f"0x{b:02X}"] = codepoint
    # Two-byte sequences (Shift-JIS style lead/trail ranges)
    lead_ranges = [(0x81, 0x9F), (0xE0, 0xFC)]
    trail_ranges = [(0x40, 0x7E), (0x80, 0xFC)]
    for start, end in lead_ranges:
        for lead in range(start, end + 1):
            for t_start, t_end in trail_ranges:
                for trail in range(t_start, t_end + 1):
                    codepoint = decode_iconv(encoding, bytes([lead, trail]))
                    if codepoint:
                        mapping[f"0x{lead:02X} 0x{trail:02X}"] = codepoint
    return [
        {"byte_sequence": key, "unicode_codepoint": value}
        for key, value in sorted(mapping.items())
    ]


def write_mapping(charset: str, info: dict, url: str) -> bool:
    try:
        text, source_type = load_text(url)
    except Exception as exc:
        print(f"[skip] {charset}: {exc}")
        return False
    if source_type == "charmap":
        entries = parse_charmap(text)
    elif source_type == "iconv":
        entries = parse_iconv(text)
    else:
        entries = parse_mapping(text)
    if not entries:
        print(f"[skip] {charset}: no entries parsed")
        return False
    description = DESCRIPTION_OVERRIDES.get(charset, f"Mapping table from {url}")
    payload = {
        "charset": charset,
        "description": description,
        "aliases": info.get("aliases", []),
        "min_bytes": info.get("min_bytes", 1),
        "max_bytes": info.get("max_bytes", 1),
        "is_variable_width": info.get("is_variable_width", False),
        "unmapped_policy": "reject",
        "replacement_codepoint": "U+FFFD",
        "mappings": entries
    }
    path = os.path.join(MAPPING_DIR, normalize_filename(charset))
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2, sort_keys=False)
        fh.write("\n")
    print(f"[ok] {charset} -> {path}")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate charset mapping tables.")
    parser.add_argument("--only", help="Comma-separated charset names to regenerate.")
    args = parser.parse_args()
    only = None
    if args.only:
        only = {item.strip() for item in args.only.split(",") if item.strip()}
    os.makedirs(MAPPING_DIR, exist_ok=True)
    charsets = load_charsets()
    for charset, url in MAPPING_SOURCES:
        if only is not None and charset not in only:
            continue
        info = charsets.get(charset)
        if not info:
            print(f"[skip] {charset}: not in charsets.json")
            continue
        write_mapping(charset, info, url)


if __name__ == "__main__":
    main()
