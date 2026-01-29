#!/usr/bin/env python3
import argparse
import json
import os
import sys


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def normalize(name):
    return "".join(ch.lower() for ch in name if ch.isalnum())


def main():
    parser = argparse.ArgumentParser(description="Report i18n charset/collation coverage.")
    parser.add_argument("--output", help="Write report to a file instead of stdout.")
    args = parser.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    charset_path = os.path.join(repo_root, "resources", "charsets", "charsets.json")
    collation_path = os.path.join(repo_root, "resources", "collations", "collations.json")
    mappings_dir = os.path.join(repo_root, "resources", "charsets", "mappings")

    charsets = load_json(charset_path).get("character_sets", [])
    collations = load_json(collation_path).get("collations", [])

    charset_by_name = {normalize(cs.get("name", "")): cs for cs in charsets}
    mapping_files = {
        normalize(load_json(os.path.join(mappings_dir, fname)).get("charset", ""))
        for fname in os.listdir(mappings_dir)
        if fname.endswith(".map.json") and fname != "charset_mapping.schema.json"
    }

    engines = {
        "Firebird": {"Firebird"},
        "MySQL": {"MySQL", "MariaDB"},
        "PostgreSQL": {"PostgreSQL"},
    }

    lines = []
    lines.append("# i18n Conformance Report")
    lines.append("")

    for engine, supported_by in engines.items():
        engine_charsets = [
            cs for cs in charsets
            if supported_by.intersection(cs.get("supported_by", []))
        ]
        engine_collations = [
            col for col in collations
            if supported_by.intersection(col.get("supported_by", []))
        ]

        missing_mappings = []
        for cs in engine_charsets:
            name_key = normalize(cs.get("name", ""))
            encoding_type = cs.get("encoding_type", "")
            if encoding_type == "unicode":
                continue
            if name_key not in mapping_files:
                missing_mappings.append(cs.get("name", ""))

        missing_collation_charsets = []
        for col in engine_collations:
            charset_name = col.get("charset", "")
            if charset_name and normalize(charset_name) not in charset_by_name:
                missing_collation_charsets.append((col.get("name", ""), charset_name))

        lines.append(f"## {engine}")
        lines.append("")
        lines.append(f"- Charsets: {len(engine_charsets)}")
        lines.append(f"- Collations: {len(engine_collations)}")
        lines.append("")

        if missing_mappings:
            lines.append("Missing mapping tables:")
            for name in sorted(missing_mappings):
                lines.append(f"- {name}")
        else:
            lines.append("Missing mapping tables: none")

        lines.append("")
        if missing_collation_charsets:
            lines.append("Collations referencing unknown charset:")
            for collation_name, charset_name in sorted(missing_collation_charsets):
                lines.append(f"- {collation_name}: {charset_name}")
        else:
            lines.append("Collations referencing unknown charset: none")

        lines.append("")

    report = "\n".join(lines).rstrip() + "\n"
    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(report)
    else:
        sys.stdout.write(report)


if __name__ == "__main__":
    sys.exit(main())
