#!/usr/bin/env python3
"""
Update ScratchBird charset/collation resource JSON files using:
  - Firebird Appendix H (local doc)
  - MySQL 8.0 source Index.xml
  - PostgreSQL encodings list + OS locales for collations
"""
from __future__ import annotations

import io
import json
import os
import re
import subprocess
import tarfile
import urllib.request
import xml.etree.ElementTree as ET
from typing import Dict, List, Set, Tuple

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CHARSETS_JSON = os.path.join(ROOT, "charsets", "charsets.json")
COLLATIONS_JSON = os.path.join(ROOT, "collations", "collations.json")

FIREBIRD_DOC = os.path.abspath(os.path.join(
    ROOT, "..", "..", "docs", "specifications", "reference", "firebird",
    "firebird_docs_split", "App_H_Charsets_and_Collations.md"
))

MYSQL_TARBALL_URL = "https://cdn.mysql.com/Downloads/MySQL-8.0/mysql-8.0.37.tar.gz"
MYSQL_INDEX_XML_PATH = "share/charsets/Index.xml"

PG_TARBALL_URL = "https://ftp.postgresql.org/pub/source/v16.2/postgresql-16.2.tar.gz"
PG_ENC_NAMES_PATHS = [
    "src/backend/utils/mb/encnames.c",
    "src/common/encnames.c",
]


def normalize(value: str) -> str:
    return "".join(c.lower() for c in value if c.isalnum())


def load_json(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def save_json(path: str, data: dict) -> None:
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2, sort_keys=False)
        fh.write("\n")


def merge_list(target: List[str], items: List[str]) -> List[str]:
    seen = {normalize(v) for v in target}
    for item in items:
        if normalize(item) not in seen:
            target.append(item)
            seen.add(normalize(item))
    return target


def upsert_charset(charsets: Dict[str, dict],
                   name: str,
                   max_bytes: int,
                   min_bytes: int,
                   is_variable: bool,
                   encoding_type: str,
                   description: str,
                   aliases: List[str],
                   supported_by: List[str],
                   iana_name: str | None = None) -> None:
    key = normalize(name)
    entry = charsets.get(key)
    if entry is None:
        entry = {
            "name": name,
            "description": description,
            "aliases": [],
            "max_bytes": max_bytes,
            "min_bytes": min_bytes,
            "is_variable_width": is_variable,
            "encoding_type": encoding_type,
            "iana_name": iana_name or name,
            "supported_by": []
        }
        charsets[key] = entry
    else:
        entry["max_bytes"] = max(entry.get("max_bytes", max_bytes), max_bytes)
        entry["min_bytes"] = min(entry.get("min_bytes", min_bytes), min_bytes)
        entry["is_variable_width"] = bool(entry.get("is_variable_width", False) or is_variable)
        if not entry.get("encoding_type") and encoding_type:
            entry["encoding_type"] = encoding_type
        if not entry.get("description") and description:
            entry["description"] = description
        if not entry.get("iana_name") and iana_name:
            entry["iana_name"] = iana_name

    entry["aliases"] = merge_list(entry.get("aliases", []), aliases)
    entry["supported_by"] = merge_list(entry.get("supported_by", []), supported_by)


def upsert_collation(collations: Dict[str, dict],
                     name: str,
                     charset: str,
                     case_insensitive: bool,
                     accent_insensitive: bool,
                     language: str,
                     description: str,
                     supported_by: List[str]) -> None:
    key = normalize(name)
    entry = collations.get(key)
    if entry is None:
        entry = {
            "name": name,
            "charset": charset,
            "case_insensitive": case_insensitive,
            "accent_insensitive": accent_insensitive,
            "language": language,
            "description": description,
            "supported_by": []
        }
        collations[key] = entry
    else:
        if entry.get("charset") != charset and entry.get("charset"):
            # Keep existing charset; add description note if mismatch
            if description and description not in entry.get("description", ""):
                entry["description"] = (entry.get("description", "") + "; " + description).strip("; ")
        if not entry.get("description") and description:
            entry["description"] = description
        if not entry.get("language") and language:
            entry["language"] = language
        entry["case_insensitive"] = bool(entry.get("case_insensitive", False) or case_insensitive)
        entry["accent_insensitive"] = bool(entry.get("accent_insensitive", False) or accent_insensitive)

    entry["supported_by"] = merge_list(entry.get("supported_by", []), supported_by)


def charset_encoding_type(name: str, max_bytes: int) -> str:
    upper = name.upper()
    if "UTF" in upper or "UNICODE" in upper or "UCS" in upper:
        return "unicode"
    if max_bytes > 1:
        return "multi_byte"
    return "single_byte"


def parse_firebird_doc(path: str) -> Tuple[Dict[str, int], List[Tuple[str, str, str]]]:
    charsets: Dict[str, int] = {}
    collations: List[Tuple[str, str, str]] = []
    last_charset = None
    last_id = None
    last_bytes = None
    started = False

    if not os.path.exists(path):
        return charsets, collations

    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if "Table 275" in line:
                started = True
                continue
            if not started:
                continue
            if line.startswith("Appendix") or line.startswith("<!--") or line.startswith("Character Set"):
                continue

            tokens = line.split()
            if len(tokens) < 4:
                continue

            charset = tokens[0]
            cs_id = tokens[1]
            bytes_per = tokens[2]
            collation = tokens[3]
            language = " ".join(tokens[4:]) if len(tokens) > 4 else ""

            if charset == "〃":
                charset = last_charset
            if cs_id == "〃":
                cs_id = last_id
            if bytes_per == "〃":
                bytes_per = last_bytes

            if not charset or not bytes_per:
                continue

            try:
                bytes_val = int(bytes_per)
            except ValueError:
                continue

            last_charset = charset
            last_id = cs_id
            last_bytes = bytes_per

            charsets[charset] = max(charsets.get(charset, 0), bytes_val)
            collations.append((collation, charset, language))

    return charsets, collations


def find_member_by_suffixes(tf: tarfile.TarFile, suffixes: List[str]) -> tarfile.TarInfo:
    for member in tf.getmembers():
        for suffix in suffixes:
            if member.name.endswith(suffix):
                return member
    raise KeyError(f"member suffixes '{suffixes}' not found in archive")


def download_mysql_index_xml(url: str) -> bytes:
    with urllib.request.urlopen(url) as resp:
        data = resp.read()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tf:
        member = find_member_by_suffixes(tf, [MYSQL_INDEX_XML_PATH])
        extracted = tf.extractfile(member)
        return extracted.read() if extracted else b""


def parse_mysql_index_xml(xml_bytes: bytes) -> Tuple[List[dict], List[dict]]:
    charsets = []
    collations = []
    root = ET.fromstring(xml_bytes)
    for cs in root.findall("charset"):
        name = cs.get("name")
        maxlen = int(cs.get("maxlen", "1"))
        comment = cs.get("comment", "")
        charsets.append({
            "name": name,
            "maxlen": maxlen,
            "comment": comment
        })
        for coll in cs.findall("collation"):
            collations.append({
                "name": coll.get("name"),
                "charset": name,
                "comment": coll.get("comment", "")
            })
    return charsets, collations


def download_pg_encnames(url: str) -> str:
    with urllib.request.urlopen(url) as resp:
        data = resp.read()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tf:
        member = find_member_by_suffixes(tf, PG_ENC_NAMES_PATHS)
        extracted = tf.extractfile(member)
        return extracted.read().decode("utf-8", errors="replace") if extracted else ""


def parse_pg_encnames(src: str) -> List[str]:
    names = []
    pattern = re.compile(r"\\{\\s*\"([A-Z0-9_]+)\"\\s*,\\s*[0-9]+\\s*\\}")
    for match in pattern.finditer(src):
        names.append(match.group(1))
    return sorted(set(names))


def mysql_collation_flags(name: str) -> Tuple[bool, bool]:
    lower = name.lower()
    if "_bin" in lower:
        return False, False
    case_insensitive = "_ci" in lower
    accent_insensitive = "_ai" in lower
    if "_cs" in lower:
        case_insensitive = False
    if "_as" in lower:
        accent_insensitive = False
    return case_insensitive, accent_insensitive


def firebird_collation_flags(name: str) -> Tuple[bool, bool]:
    upper = name.upper()
    case_insensitive = "_CI" in upper
    accent_insensitive = "_AI" in upper
    if "_CS" in upper:
        case_insensitive = False
    if "_AS" in upper:
        accent_insensitive = False
    return case_insensitive, accent_insensitive


def pg_collation_flags(_: str) -> Tuple[bool, bool]:
    return False, False


def canonical_charset(name: str) -> str:
    upper = name.upper()
    if upper.startswith("ISO8859_"):
        return upper.replace("ISO8859_", "ISO-8859-")
    if upper.startswith("ISO8859"):
        return upper.replace("ISO8859", "ISO-8859-")
    if upper.startswith("WIN") and upper[3:].isdigit():
        return f"Windows-{upper[3:]}"
    if upper.startswith("DOS") and upper[3:].isdigit():
        return f"CP{upper[3:]}"
    if upper == "UTF8":
        return "UTF-8"
    if upper == "UTF16":
        return "UTF-16"
    if upper == "UTF32":
        return "UTF-32"
    if upper == "BIG_5":
        return "Big5"
    if upper == "GB_2312":
        return "GB2312"
    if upper == "KOI8R":
        return "KOI8-R"
    if upper == "KOI8U":
        return "KOI8-U"
    return name


def charset_from_locale(locale_name: str) -> str:
    if "." in locale_name:
        suffix = locale_name.split(".", 1)[1]
        if suffix.lower() in ("utf8", "utf-8"):
            return "UTF-8"
        return canonical_charset(suffix.upper())
    return "ASCII"


def main() -> None:
    charsets_data = load_json(CHARSETS_JSON)
    collations_data = load_json(COLLATIONS_JSON)

    charsets: Dict[str, dict] = {
        normalize(cs["name"]): cs for cs in charsets_data.get("character_sets", [])
    }
    collations: Dict[str, dict] = {
        normalize(c["name"]): c for c in collations_data.get("collations", [])
    }

    # Firebird
    fb_charsets, fb_collations = parse_firebird_doc(FIREBIRD_DOC)
    for name, max_bytes in fb_charsets.items():
        canonical = canonical_charset(name)
        upsert_charset(
            charsets,
            canonical,
            max_bytes=max_bytes,
            min_bytes=1,
            is_variable=max_bytes > 1,
            encoding_type=charset_encoding_type(canonical, max_bytes),
            description=f"Firebird charset {name}",
            aliases=[name],
            supported_by=["Firebird"]
        )
    for coll_name, charset_name, language in fb_collations:
        canonical = canonical_charset(charset_name)
        case_ins, accent_ins = firebird_collation_flags(coll_name)
        upsert_collation(
            collations,
            coll_name,
            charset=canonical,
            case_insensitive=case_ins,
            accent_insensitive=accent_ins,
            language=language,
            description=f"Firebird collation {coll_name} ({language})".strip(),
            supported_by=["Firebird"]
        )

    # MySQL (Index.xml)
    mysql_xml = download_mysql_index_xml(MYSQL_TARBALL_URL)
    mysql_charsets, mysql_collations = parse_mysql_index_xml(mysql_xml)
    for cs in mysql_charsets:
        name = cs["name"]
        canonical = canonical_charset(name)
        maxlen = int(cs["maxlen"])
        upsert_charset(
            charsets,
            canonical,
            max_bytes=maxlen,
            min_bytes=1,
            is_variable=maxlen > 1,
            encoding_type=charset_encoding_type(canonical, maxlen),
            description=cs.get("comment", ""),
            aliases=[name],
            supported_by=["MySQL", "MariaDB"]
        )
    for coll in mysql_collations:
        coll_name = coll["name"]
        charset_name = canonical_charset(coll["charset"])
        case_ins, accent_ins = mysql_collation_flags(coll_name)
        upsert_collation(
            collations,
            coll_name,
            charset=charset_name,
            case_insensitive=case_ins,
            accent_insensitive=accent_ins,
            language="",
            description=coll.get("comment", ""),
            supported_by=["MySQL", "MariaDB"]
        )

    # PostgreSQL encodings
    pg_src = download_pg_encnames(PG_TARBALL_URL)
    pg_encodings = parse_pg_encnames(pg_src)
    for name in pg_encodings:
        canonical = canonical_charset(name)
        max_bytes = 4 if "UTF8" in name or "UTF" in canonical.upper() else 1
        upsert_charset(
            charsets,
            canonical,
            max_bytes=max_bytes,
            min_bytes=1,
            is_variable=max_bytes > 1,
            encoding_type=charset_encoding_type(canonical, max_bytes),
            description=f"PostgreSQL encoding {name}",
            aliases=[name],
            supported_by=["PostgreSQL"]
        )

    # PostgreSQL collations from OS locales
    try:
        locale_list = subprocess.check_output(["locale", "-a"], text=True)
        locales = [line.strip() for line in locale_list.splitlines() if line.strip()]
    except Exception:
        locales = []

    for locale_name in locales:
        charset_name = charset_from_locale(locale_name)
        case_ins, accent_ins = pg_collation_flags(locale_name)
        upsert_collation(
            collations,
            locale_name,
            charset=charset_name,
            case_insensitive=case_ins,
            accent_insensitive=accent_ins,
            language="",
            description="PostgreSQL system locale collation",
            supported_by=["PostgreSQL"]
        )

    # Always include C/POSIX collations for PostgreSQL
    for name in ["C", "POSIX", "default"]:
        upsert_collation(
            collations,
            name,
            charset="ASCII",
            case_insensitive=False,
            accent_insensitive=False,
            language="",
            description="PostgreSQL default collation",
            supported_by=["PostgreSQL"]
        )

    # Sort for stable output
    charsets_list = sorted(charsets.values(), key=lambda x: x["name"].lower())
    collations_list = sorted(collations.values(), key=lambda x: x["name"].lower())

    save_json(CHARSETS_JSON, {"character_sets": charsets_list})
    save_json(COLLATIONS_JSON, {"collations": collations_list})


if __name__ == "__main__":
    main()
