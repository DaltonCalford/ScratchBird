#!/usr/bin/env python3
# ScratchBird
# Copyright (c) 2025-2026 Dalton Calford
#
# Licensed under the Initial Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

import json
import os
import sys


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)

def normalize(value):
    return "".join(ch.lower() for ch in value if ch.isalnum())


def fail(msg):
    print(f"ERROR: {msg}")
    return 1


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    charset_path = os.path.join(repo_root, "resources", "charsets", "charsets.json")
    collations_path = os.path.join(repo_root, "resources", "collations", "collations.json")
    mappings_dir = os.path.join(repo_root, "resources", "charsets", "mappings")

    errors = 0
    charsets = load_json(charset_path).get("character_sets", [])
    collations = load_json(collations_path).get("collations", [])

    name_map = {}
    alias_map = {}
    charset_meta = {}

    for charset in charsets:
        name = charset.get("name", "")
        if not name:
            errors += fail("Charset entry missing name")
            continue
        key = normalize(name)
        if key in name_map:
            errors += fail(f"Duplicate charset name: {name}")
        name_map[key] = name
        charset_meta[key] = {
            "max_bytes": charset.get("max_bytes", 1),
            "encoding_type": charset.get("encoding_type", "")
        }

        for alias in charset.get("aliases", []):
            alias_key = normalize(alias)
            if alias_key in alias_map:
                if alias_map[alias_key] == name:
                    continue
                if alias_key in name_map:
                    continue
                errors += fail(f"Alias '{alias}' reused by {name} and {alias_map[alias_key]}")
                continue
            alias_map[alias_key] = name

    for collation in collations:
        cname = collation.get("name", "")
        charset = collation.get("charset", "")
        if not cname:
            errors += fail("Collation entry missing name")
        if charset:
            charset_key = normalize(charset)
            if charset_key not in name_map and charset_key not in alias_map:
                errors += fail(f"Collation '{cname}' references unknown charset '{charset}'")

    mapping_files = [
        f for f in os.listdir(mappings_dir)
        if f.endswith(".map.json") and f != "charset_mapping.schema.json"
    ]
    mapping_charset_names = set()
    for fname in mapping_files:
        path = os.path.join(mappings_dir, fname)
        try:
            payload = load_json(path)
        except json.JSONDecodeError as exc:
            errors += fail(f"Mapping file {fname} invalid JSON: {exc}")
            continue
        charset = payload.get("charset", "")
        if not charset:
            errors += fail(f"Mapping file {fname} missing charset field")
            continue
        charset_key = normalize(charset)
        if charset_key in mapping_charset_names:
            errors += fail(f"Duplicate mapping table for charset '{charset}'")
        mapping_charset_names.add(charset_key)
        if charset_key not in name_map:
            errors += fail(f"Mapping file {fname} references unknown charset '{charset}'")
            continue
        meta = charset_meta.get(charset_key, {})
        if meta.get("max_bytes", 1) > 1:
            validation = payload.get("validation")
            if not isinstance(validation, dict):
                errors += fail(f"Mapping file {fname} missing validation rules for multibyte charset")
            else:
                has_rules = bool(validation.get("lead_byte_ranges") or
                                 validation.get("trail_byte_ranges") or
                                 validation.get("multi_byte_sequences"))
                if not has_rules:
                    errors += fail(f"Mapping file {fname} missing validation ranges for multibyte charset")

    if errors:
        print(f"Validation failed with {errors} errors")
        return 1

    print("i18n resource validation passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
