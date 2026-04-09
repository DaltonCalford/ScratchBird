# Domain Extension Catalog Tests

Ticket: `CAT-011`
Gate: `CAT-GATE-03`

## Required coverage
- `domain_param_key` CRUD and uniqueness.
- `domain_parameter` type contract and FK-like linkage to `domain_param_key`.
- `domain_constraint` catalog persistence and retrieval.
- `domain_security` catalog persistence and retrieval.
- `domain_validation` catalog persistence and retrieval.
- `domain_integrity` catalog persistence and retrieval.
- Physical page bootstrap and reopen persistence for all six tables.

## Implemented tests
- `CatalogDatabaseBootstrapTest.CreatesDomainExtensionCatalogFamilyPages`
- `CatalogDomainExtensionContractTest.DomainParamKeyAndParameterContracts`
- `CatalogDomainExtensionContractTest.DomainConstraintCatalogContracts`
- `CatalogDomainExtensionContractTest.DomainSecurityValidationIntegrityContracts`

## Result
- All required CAT-011 tests passed in targeted gate run.
