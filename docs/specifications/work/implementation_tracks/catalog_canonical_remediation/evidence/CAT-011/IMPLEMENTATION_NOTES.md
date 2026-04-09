# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-011 catalog root fields and persistence wiring:
  - `domain_param_keys_page`
  - `domain_parameters_page`
  - `domain_constraints_page`
  - `domain_security_page`
  - `domain_validation_page`
  - `domain_integrity_page`
- Added bootstrap allocation + legacy backfill allocation for all six CAT-011 tables.
- Added on-disk record contracts for CAT-011 families in `CatalogManager`.
- Added full CAT-011 CRUD/public APIs in `CatalogManager` for:
  - `domain_param_key`
  - `domain_parameter`
  - `domain_constraint`
  - `domain_security`
  - `domain_validation`
  - `domain_integrity`
- Enforced deterministic contracts:
  - `domain_param_key`: unique key id and unique name.
  - `domain_parameter`: FK-like key existence/type check and exactly-one typed `val_*` contract.
  - `domain_constraint/security/validation/integrity`: required domain UUID, valid kind enum, required expression payload.
- Added CAT-011 bootstrap persistence gate test and domain-extension CRUD contract tests.
