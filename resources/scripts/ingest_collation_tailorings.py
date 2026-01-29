#!/usr/bin/env python3
"""
Ingest MySQL and Firebird collation tailoring data into resources.
"""
from __future__ import annotations

import io
import os
import shutil
import tarfile
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAILORINGS_DIR = os.path.join(ROOT, "collations", "tailorings")

MYSQL_TARBALL_URL = "https://cdn.mysql.com/Downloads/MySQL-8.0/mysql-8.0.37.tar.gz"
MYSQL_CHARSET_PREFIX = "share/charsets/"

FIREBIRD_SRC = "/home/dcalford/CliWork/Firebird-6.0.0.1124-1ccdf1c-source"
FIREBIRD_COLLATIONS_DIR = os.path.join(FIREBIRD_SRC, "src", "intl", "collations")

USER_AGENT = "ScratchBird-Tailorings/1.0"


def ingest_mysql_tailorings() -> None:
    target_dir = os.path.join(TAILORINGS_DIR, "mysql")
    os.makedirs(target_dir, exist_ok=True)
    req = urllib.request.Request(MYSQL_TARBALL_URL, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req) as resp:
        data = resp.read()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tf:
        for member in tf.getmembers():
            if not member.name.endswith(".xml"):
                continue
            if MYSQL_CHARSET_PREFIX not in member.name:
                continue
            fname = os.path.basename(member.name)
            if fname.lower() == "index.xml":
                continue
            extracted = tf.extractfile(member)
            if not extracted:
                continue
            path = os.path.join(target_dir, fname)
            with open(path, "wb") as fh:
                fh.write(extracted.read())
    with open(os.path.join(target_dir, "source_version.txt"), "w", encoding="utf-8") as fh:
        fh.write(f"MySQL source: {MYSQL_TARBALL_URL}\n")


def ingest_firebird_tailorings() -> None:
    if not os.path.isdir(FIREBIRD_COLLATIONS_DIR):
        raise FileNotFoundError(f"Missing Firebird collations directory: {FIREBIRD_COLLATIONS_DIR}")
    target_dir = os.path.join(TAILORINGS_DIR, "firebird", "tables")
    os.makedirs(target_dir, exist_ok=True)
    for name in os.listdir(FIREBIRD_COLLATIONS_DIR):
        if not name.endswith(".h"):
            continue
        src = os.path.join(FIREBIRD_COLLATIONS_DIR, name)
        dst = os.path.join(target_dir, name)
        shutil.copyfile(src, dst)
    with open(os.path.join(TAILORINGS_DIR, "firebird", "source_version.txt"), "w", encoding="utf-8") as fh:
        fh.write(f"Firebird source: {FIREBIRD_SRC}\n")


def main() -> None:
    ingest_mysql_tailorings()
    ingest_firebird_tailorings()


if __name__ == "__main__":
    main()
