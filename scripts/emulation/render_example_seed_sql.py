#!/usr/bin/env python3
"""
Render example database seed artifacts from the installable auth manifest.

This keeps the example/test database bootstrap credentials in one executable
source of truth while still producing plain SQL files that the shell harness
can execute with sb_isql.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


@dataclass(frozen=True)
class Account:
    section: str
    username: str
    password: str
    is_superuser: bool
    seed_on_database_bootstrap: bool
    access_intent: str


def _load_manifest(path: Path) -> Dict[str, object]:
    with path.open("r", encoding="utf-8") as fh:
        doc = json.load(fh)
    if not isinstance(doc, dict):
        raise ValueError("manifest root must be a JSON object")
    if doc.get("format_version") != 1:
        raise ValueError("manifest format_version must be 1")
    defaults = doc.get("defaults")
    if not isinstance(defaults, dict):
        raise ValueError("manifest defaults must be a JSON object")
    return defaults


def _coerce_accounts(section: str, raw_entries: object) -> List[Account]:
    if not isinstance(raw_entries, list):
        raise ValueError(f"defaults.{section} must be an array")
    out: List[Account] = []
    for index, entry in enumerate(raw_entries):
        if not isinstance(entry, dict):
            raise ValueError(f"defaults.{section}[{index}] must be an object")
        username = entry.get("username")
        password = entry.get("password")
        is_superuser = entry.get("is_superuser")
        seed = entry.get("seed_on_database_bootstrap")
        access_intent = entry.get("access_intent", "")
        if not isinstance(username, str) or not username:
            raise ValueError(f"defaults.{section}[{index}].username must be a non-empty string")
        if not isinstance(password, str) or not password:
            raise ValueError(f"defaults.{section}[{index}].password must be a non-empty string")
        if not isinstance(is_superuser, bool):
            raise ValueError(f"defaults.{section}[{index}].is_superuser must be boolean")
        if not isinstance(seed, bool):
            raise ValueError(
                f"defaults.{section}[{index}].seed_on_database_bootstrap must be boolean"
            )
        if not isinstance(access_intent, str):
            raise ValueError(f"defaults.{section}[{index}].access_intent must be a string")
        out.append(
            Account(
                section=section,
                username=username,
                password=password,
                is_superuser=is_superuser,
                seed_on_database_bootstrap=seed,
                access_intent=access_intent,
            )
        )
    if not out:
        raise ValueError(f"defaults.{section} is empty")
    return out


def _pick_account(accounts: Iterable[Account], *, access_intent: str, is_superuser: bool) -> Account:
    candidates = list(accounts)
    for entry in candidates:
        if entry.access_intent == access_intent:
            return entry
    for entry in candidates:
        if entry.is_superuser == is_superuser:
            return entry
    raise ValueError(
        f"no account entry found for access_intent={access_intent!r} is_superuser={is_superuser}"
    )


def _env_override(name: str, default: str) -> str:
    value = os.environ.get(name)
    return value if value else default


def _effective_accounts(defaults: Dict[str, object]) -> Dict[str, Account]:
    scratchbird = _coerce_accounts("scratchbird", defaults.get("scratchbird"))
    postgresql = _coerce_accounts("postgresql", defaults.get("postgresql"))
    mysql = _coerce_accounts("mysql", defaults.get("mysql"))
    firebird = _coerce_accounts("firebird", defaults.get("firebird"))

    native_admin = _pick_account(scratchbird, access_intent="admin", is_superuser=True)
    native_public = _pick_account(scratchbird, access_intent="public", is_superuser=False)
    pg_admin = _pick_account(postgresql, access_intent="admin", is_superuser=True)
    pg_public = _pick_account(postgresql, access_intent="public", is_superuser=False)
    my_admin = _pick_account(mysql, access_intent="admin", is_superuser=True)
    my_public = _pick_account(mysql, access_intent="public", is_superuser=False)
    fb_admin = _pick_account(firebird, access_intent="admin", is_superuser=True)
    fb_public = _pick_account(firebird, access_intent="public", is_superuser=False)

    return {
        "native_admin": Account(
            section="scratchbird",
            username=_env_override("SCRATCHBIRD_EXAMPLE_ADMIN_USER", native_admin.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_ADMIN_PASSWORD", native_admin.password),
            is_superuser=True,
            seed_on_database_bootstrap=native_admin.seed_on_database_bootstrap,
            access_intent="admin",
        ),
        "native_public": Account(
            section="scratchbird",
            username=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_NATIVE_USER", native_public.username),
            password=_env_override(
                "SCRATCHBIRD_EXAMPLE_COMPAT_NATIVE_PASSWORD", native_public.password
            ),
            is_superuser=False,
            seed_on_database_bootstrap=native_public.seed_on_database_bootstrap,
            access_intent="public",
        ),
        "pg_admin": Account(
            section="postgresql",
            username=_env_override("SCRATCHBIRD_EXAMPLE_PG_USER", pg_admin.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_PG_PASSWORD", pg_admin.password),
            is_superuser=True,
            seed_on_database_bootstrap=pg_admin.seed_on_database_bootstrap,
            access_intent="admin",
        ),
        "pg_public": Account(
            section="postgresql",
            username=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_PG_USER", pg_public.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_PG_PASSWORD", pg_public.password),
            is_superuser=False,
            seed_on_database_bootstrap=pg_public.seed_on_database_bootstrap,
            access_intent="public",
        ),
        "my_admin": Account(
            section="mysql",
            username=_env_override("SCRATCHBIRD_EXAMPLE_MY_USER", my_admin.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_MY_PASSWORD", my_admin.password),
            is_superuser=True,
            seed_on_database_bootstrap=my_admin.seed_on_database_bootstrap,
            access_intent="admin",
        ),
        "my_public": Account(
            section="mysql",
            username=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_MY_USER", my_public.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_MY_PASSWORD", my_public.password),
            is_superuser=False,
            seed_on_database_bootstrap=my_public.seed_on_database_bootstrap,
            access_intent="public",
        ),
        "fb_admin": Account(
            section="firebird",
            username=_env_override("SCRATCHBIRD_EXAMPLE_FB_USER", fb_admin.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_FB_PASSWORD", fb_admin.password),
            is_superuser=True,
            seed_on_database_bootstrap=fb_admin.seed_on_database_bootstrap,
            access_intent="admin",
        ),
        "fb_public": Account(
            section="firebird",
            username=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_FB_USER", fb_public.username),
            password=_env_override("SCRATCHBIRD_EXAMPLE_COMPAT_FB_PASSWORD", fb_public.password),
            is_superuser=False,
            seed_on_database_bootstrap=fb_public.seed_on_database_bootstrap,
            access_intent="public",
        ),
    }


def _shell_assignment(name: str, value: str) -> str:
    return f"{name}={shlex.quote(value)}"


def _sql_ident(name: str) -> str:
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_$]*", name):
        return name
    return '"' + name.replace('"', '""') + '"'


def _sql_string(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def _render_env(accounts: Dict[str, Account]) -> str:
    lines = [
        "# Generated from resources/bootstrap/default_auth_manifest.json",
        _shell_assignment("ADMIN_USER", accounts["native_admin"].username),
        _shell_assignment("ADMIN_PASSWORD", accounts["native_admin"].password),
        _shell_assignment("PG_USER", accounts["pg_admin"].username),
        _shell_assignment("PG_PASSWORD", accounts["pg_admin"].password),
        _shell_assignment("MYSQL_USER", accounts["my_admin"].username),
        _shell_assignment("MYSQL_PASSWORD", accounts["my_admin"].password),
        _shell_assignment("FB_USER", accounts["fb_admin"].username),
        _shell_assignment("FB_PASSWORD", accounts["fb_admin"].password),
        _shell_assignment("COMPAT_NATIVE_USER", accounts["native_public"].username),
        _shell_assignment("COMPAT_NATIVE_PASSWORD", accounts["native_public"].password),
        _shell_assignment("COMPAT_PG_USER", accounts["pg_public"].username),
        _shell_assignment("COMPAT_PG_PASSWORD", accounts["pg_public"].password),
        _shell_assignment("COMPAT_MY_USER", accounts["my_public"].username),
        _shell_assignment("COMPAT_MY_PASSWORD", accounts["my_public"].password),
        _shell_assignment("COMPAT_FB_USER", accounts["fb_public"].username),
        _shell_assignment("COMPAT_FB_PASSWORD", accounts["fb_public"].password),
    ]
    return "\n".join(lines) + "\n"


def _ordered_accounts(accounts: Dict[str, Account]) -> List[Account]:
    return [
        accounts["native_admin"],
        accounts["pg_admin"],
        accounts["my_admin"],
        accounts["fb_admin"],
        accounts["native_public"],
        accounts["pg_public"],
        accounts["my_public"],
        accounts["fb_public"],
    ]


def _render_bootstrap_sql(accounts: Dict[str, Account]) -> str:
    ordered = _ordered_accounts(accounts)
    lines = [
        "-- ScratchBird example database bootstrap + seed data",
        "-- Generated from resources/bootstrap/default_auth_manifest.json.",
        "",
    ]
    for account in ordered:
        lines.append(f"DROP USER IF EXISTS {_sql_ident(account.username)};")
    lines.append("")
    for account in ordered:
        stmt = (
            f"CREATE USER {_sql_ident(account.username)} WITH PASSWORD {_sql_string(account.password)}"
        )
        if account.is_superuser:
            stmt += " SUPERUSER"
        stmt += ";"
        lines.append(stmt)
    lines.extend(
        [
            "",
            "-- Ensure scripted user DDL is durable for subsequent seed/login phases.",
            "COMMIT;",
            "",
            "-- NOTE:",
            "-- SUPERUSER accounts already have full database access.",
            "-- Keep bootstrap free of GRANT statements until the semantic-bridge",
            "-- closure path for SBLR3_GRANT is fully enabled in all test profiles.",
            "-- Post-bootstrap schema/data seeding runs in the runtime-generated",
            "-- post-bootstrap SQL emitted by scripts/example_db_manager.sh.",
            "",
        ]
    )
    return "\n".join(lines)


def _render_post_bootstrap_sql(args: argparse.Namespace, accounts: Dict[str, Account]) -> str:
    native_admin = accounts["native_admin"]
    pg_admin = accounts["pg_admin"]
    my_admin = accounts["my_admin"]
    fb_admin = accounts["fb_admin"]
    native_public = accounts["native_public"]
    pg_public = accounts["pg_public"]
    my_public = accounts["my_public"]
    fb_public = accounts["fb_public"]

    lines = [
        "-- ScratchBird example database post-bootstrap schema/data seed.",
        "-- Generated from resources/bootstrap/default_auth_manifest.json and example harness metadata.",
        f"-- This runs after bootstrap user creation using {native_admin.username} credentials.",
        "",
        "SET SCHEMA users.public;",
        "",
        "-- Identity mapping model for compatibility harnesses:",
        "--   - Canonical user identity is tracked in compat_identity_user_map_contract.canonical_userid.",
        "--   - Engine-facing login aliases are tracked per engine_scope.",
        "--   - This is a contract fixture only; runtime auth must not consume it yet.",
        "",
        "CREATE TABLE compat_identity_user_map_contract (",
        "    canonical_userid VARCHAR(64) NOT NULL,",
        "    canonical_user VARCHAR(96) NOT NULL,",
        "    engine_scope VARCHAR(24) NOT NULL,",
        "    login_name VARCHAR(128) NOT NULL,",
        "    external_alias VARCHAR(128),",
        "    auth_method VARCHAR(32) NOT NULL,",
        "    password_policy VARCHAR(48) NOT NULL,",
        "    permission_profile VARCHAR(48) NOT NULL,",
        "    is_superuser BOOLEAN NOT NULL,",
        "    PRIMARY KEY (canonical_userid, engine_scope, login_name)",
        ");",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_admin_userid)}, {_sql_string(args.compat_admin_user)}, 'native', {_sql_string(native_admin.username)}, NULL, 'password', 'native_v3_strict', 'cluster_admin', TRUE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_admin_userid)}, {_sql_string(args.compat_admin_user)}, 'postgresql', {_sql_string(pg_admin.username)}, NULL, 'scram_sha_256', 'pg_emulated_default', 'engine_admin', TRUE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_admin_userid)}, {_sql_string(args.compat_admin_user)}, 'mysql', {_sql_string(my_admin.username)}, NULL, 'password', 'mysql_emulated_default', 'engine_admin', TRUE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_admin_userid)}, {_sql_string(args.compat_admin_user)}, 'firebird', {_sql_string(fb_admin.username)}, NULL, 'password', 'firebird_emulated_default', 'engine_admin', TRUE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_canonical_userid)}, {_sql_string(args.compat_canonical_user)}, 'native', {_sql_string(native_public.username)}, NULL, 'password', 'native_v3_strict', 'public_only', FALSE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_canonical_userid)}, {_sql_string(args.compat_canonical_user)}, 'postgresql', {_sql_string(pg_public.username)}, NULL, 'scram_sha_256', 'pg_emulated_default', 'public_only', FALSE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_canonical_userid)}, {_sql_string(args.compat_canonical_user)}, 'mysql', {_sql_string(my_public.username)}, NULL, 'password', 'mysql_emulated_default', 'public_only', FALSE);",
        "",
        "INSERT INTO compat_identity_user_map_contract (",
        "    canonical_userid,",
        "    canonical_user,",
        "    engine_scope,",
        "    login_name,",
        "    external_alias,",
        "    auth_method,",
        "    password_policy,",
        "    permission_profile,",
        "    is_superuser",
        f") VALUES ({_sql_string(args.compat_canonical_userid)}, {_sql_string(args.compat_canonical_user)}, 'firebird', {_sql_string(fb_public.username)}, {_sql_string(args.compat_fb_external_alias)}, 'password', 'firebird_emulated_default', 'public_only', FALSE);",
        "",
        "CREATE TABLE customers (",
        "    customer_id INTEGER PRIMARY KEY,",
        "    customer_name VARCHAR(96) NOT NULL,",
        "    customer_tier VARCHAR(16) NOT NULL,",
        "    active BOOLEAN NOT NULL",
        ");",
        "",
        "CREATE TABLE orders (",
        "    order_id INTEGER PRIMARY KEY,",
        "    customer_id INTEGER NOT NULL,",
        "    order_total DECIMAL(12,2) NOT NULL,",
        "    order_status VARCHAR(24) NOT NULL",
        ");",
        "",
        "INSERT INTO customers (customer_id, customer_name, customer_tier, active)",
        "VALUES (1, 'Alice Ng', 'gold', TRUE);",
        "",
        "INSERT INTO customers (customer_id, customer_name, customer_tier, active)",
        "VALUES (2, 'Bruno Hale', 'silver', TRUE);",
        "",
        "INSERT INTO customers (customer_id, customer_name, customer_tier, active)",
        "VALUES (3, 'Carmen Ives', 'bronze', FALSE);",
        "",
        "INSERT INTO customers (customer_id, customer_name, customer_tier, active)",
        "VALUES (4, 'Diego Wu', 'gold', TRUE);",
        "",
        "INSERT INTO orders (order_id, customer_id, order_total, order_status)",
        "VALUES (101, 1, 120.50, 'paid');",
        "",
        "INSERT INTO orders (order_id, customer_id, order_total, order_status)",
        "VALUES (102, 1, 75.00, 'pending');",
        "",
        "INSERT INTO orders (order_id, customer_id, order_total, order_status)",
        "VALUES (103, 2, 225.20, 'paid');",
        "",
        "INSERT INTO orders (order_id, customer_id, order_total, order_status)",
        "VALUES (104, 4, 19.99, 'shipped');",
        "",
        "COMMIT;",
        "",
    ]
    return "\n".join(lines)


def _write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, help="Path to default_auth_manifest.json")
    parser.add_argument("--env-out", required=True, help="Shell fragment output path")
    parser.add_argument("--bootstrap-sql-out", help="Bootstrap SQL output path")
    parser.add_argument("--post-bootstrap-sql-out", help="Post-bootstrap SQL output path")
    parser.add_argument("--compat-admin-userid", default="u_sys_admin")
    parser.add_argument("--compat-admin-user", default="sys_admin")
    parser.add_argument("--compat-canonical-userid", default="u_public_user")
    parser.add_argument("--compat-canonical-user", default="public_user")
    parser.add_argument("--compat-fb-external-alias", default="public.user")
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    manifest_path = Path(args.manifest)
    if not manifest_path.is_file():
        print(f"manifest not found: {manifest_path}", file=sys.stderr)
        return 1

    try:
        defaults = _load_manifest(manifest_path)
        accounts = _effective_accounts(defaults)
        _write_text(Path(args.env_out), _render_env(accounts))
        if args.bootstrap_sql_out:
            _write_text(Path(args.bootstrap_sql_out), _render_bootstrap_sql(accounts))
        if args.post_bootstrap_sql_out:
            _write_text(Path(args.post_bootstrap_sql_out), _render_post_bootstrap_sql(args, accounts))
    except Exception as exc:  # pragma: no cover - surfaced directly to shell
        print(f"failed rendering example seed artifacts: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
