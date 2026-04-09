# Beta 2 Data Contract And Schema Validation UDR Model

## Purpose

This document defines the data-contract UDR family for payload validation,
column-contract validation, and schema-bound interchange contracts.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `JSON Schema`-style contract validation.

## Owning package

- `sb_pkg_contract_udr`

## Mandatory surfaces

The package shall provide:

- contract definition
- contract persistence
- contract versioning
- JSON/object payload validation
- rowset/column contract validation
- contract diff and compatibility checks

## Required routine families

- `sb_contract.define(...)`
- `sb_contract.validate_json(...)`
- `sb_contract.validate_rowset(...)`
- `sb_contract.diff(...)`
- `sb_contract.compatible(...)`

## Example contract

```sql
select *
from sb_contract.validate_json(
    contract_id => 'customer_v3',
    payload => :customer_json
);
```

## Operational rules

1. Contracts must be versioned and immutable by version.
2. Validation results shall be structured and machine-readable.
3. Compatibility checks must distinguish backward, forward, and breaking
   changes.

## Explicit exclusions

- arbitrary external schema registries as a baseline requirement
- dynamic code execution inside validators
