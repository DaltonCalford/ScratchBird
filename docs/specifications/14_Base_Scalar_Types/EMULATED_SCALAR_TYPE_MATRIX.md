# Emulated Scalar Type Matrix

## Purpose

This file defines the current audited engine/type mapping and mutation-boundary contract.

## Current runtime authority

`TypeSystem::resolveEmulatedType(...)` resolves engine and emulated-type pairs against the runtime emulation matrix.

Engine names and emulated type names are normalized before resolution.

If resolution succeeds, the returned row includes `engine_name`, `emulated_type`, `storage_kind`, `canonical_type`, `domain_hint`, and `parser_rule_hint`.

`TypeSystem::requiresWholeValueUpdate(...)` resolves the row and applies `parserRuleRequiresWholeValueUpdate(...)` to the row's parser-rule hint.

`TypeSystem::allowsElementLevelMutation(...)` is the inverse of `requiresWholeValueUpdate(...)`.

## Audited engine families

The currently audited emulated-engine surface includes:

`FIREBIRD`

`POSTGRESQL`

`MYSQL`

`MILVUS`

`MONGODB`

`NEO4J`

## Direct protocol and wire mappings

The current direct mapping surfaces are:

PostgreSQL via `toPostgreSQL(...)` and `fromPostgreSQL(...)`

MySQL via `toMySQL(...)` and `fromMySQL(...)`

Firebird via `toFirebird(...)` and `fromFirebird(...)`

SBWP via `toSBWP(...)` and `fromSBWP(...)`

## Current mapping examples

PostgreSQL maps `BOOLEAN -> BOOL`, `SMALLINT -> INT2`, `INTEGER -> INT4`, `BIGINT -> INT8`, `TEXT -> TEXT`, `UUID -> UUID`, `INET -> INET`, and `CIDR -> CIDR`.

MySQL maps `BOOLEAN/TINYINT -> MYSQL_TYPE_TINY`, `INTEGER/MEDIUMINT -> MYSQL_TYPE_LONG`, `BIGINT -> MYSQL_TYPE_LONGLONG`, `JSON -> MYSQL_TYPE_JSON`, and geometry families -> `MYSQL_TYPE_GEOMETRY`.

Firebird maps `BOOLEAN -> BLR BOOLEAN`, `SMALLINT -> BLR SHORT`, `INTEGER -> BLR LONG`, `BIGINT -> BLR INT64`, `INT128 -> BLR INT128`, `TIME_WITH_ZONE -> BLR TIME_TZ`, `TIMESTAMP_WITH_ZONE -> BLR TIMESTAMP_TZ`, and `BLOB` lanes -> `BLR BLOB`.

SBWP uses an explicit protocol code map and does not rely on `DataType` enum ordinals.

## Boundary

This file authorizes type resolution, mutation-boundary truth, and concrete protocol mapping truth for the audited surfaces above.

This file does not authorize a broader semantic-parity claim for every donor engine or every historical matrix row outside the current runtime mapping surfaces.
