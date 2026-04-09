# Embedded Field Accessors

## Purpose

This file defines the current read-only accessor contract for section `14`.

## Authoritative Accessor Forms

`EXTRACT(field FROM value)` is the primary accessor surface.

Composite-record field extraction by name is a separate runtime surface through `DomainManager::extractField(...)`.

## `EXTRACT(...)` Evaluation Rules

`ExpressionEvaluator::evaluateExtract(...)` resolves the field either from the explicit field id or by name through `resolveExtractFieldName(...)`.

If a field name is supplied and resolution fails, evaluation raises an unknown-field error.

The evaluator computes argument values, computes the source value, and then calls `extractElement(...)`.

If `extractElement(...)` fails, deterministic extract errors are surfaced directly. Other failures are normalized through the extract field value-constraint formatter.

## Current Selector Families

The shipped selector inventory includes temporal selectors such as `YEAR`, `MONTH`, `DAY`, `HOUR`, `MINUTE`, `SECOND`, `MICROSECOND`, `MILLISECOND`, `DOY`, `QUARTER`, `TIMEZONE_HOUR`, and `TIMEZONE_MINUTE`.

The shipped selector inventory includes UUID selectors such as `VERSION`, `VARIANT`, `TIMESTAMP`, `NODE`, `CLOCK_SEQ`, `TIME_LOW`, `TIME_MID`, `TIME_HIGH`, `RAND_A`, and `RAND_B`.

The shipped selector inventory includes network and MAC selectors such as `FAMILY`, `NETMASK`, `ADDRESS`, `NETWORK`, `BROADCAST`, `HOSTMASK`, `OUI`, `NIC`, `IS_IPV4`, and `IS_IPV6`.

The shipped selector inventory includes spatial selectors such as `X`, `Y`, `SRID`, `NUM_POINTS`, `START_POINT`, `END_POINT`, `NUM_RINGS`, and related geometry counters.

The shipped runtime also includes range selectors such as `LOWER`, `UPPER`, `LOWER_INC`, `UPPER_INC`, `ISEMPTY`, `LOWER_INF`, and `UPPER_INF`.

## Composite Field Extraction Rules

`DomainManager::extractField(...)` requires a non-empty field name.

`DomainManager::extractField(...)` requires the input value to be `COMPOSITE`.

If the input composite value is null, the extracted result is null.

If field-name and field-value counts disagree, the operation fails as corrupted data.

If the named field is not present, the operation fails with `NOT_FOUND`.

## Explicit Exclusions

This file does not authorize universal scalar `value.field` syntax.

This file does not authorize write or mutation semantics for element access.

`ALTER_ELEMENT` is outside this file's authority boundary.
