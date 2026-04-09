# CAT-008 Object Parentage and Uniqueness Tests

## Implemented Test Coverage
1. `CatalogParentageAndNameUniquenessTest.TriggerNameCollisionIsParentScoped`
- Verifies duplicate trigger names under the same table parent fail deterministically.

2. `CatalogParentageAndNameUniquenessTest.SameTriggerNameOnDifferentTablesIsAllowed`
- Verifies same trigger names under different table parents succeed.
- Verifies unscoped trigger lookup is rejected as ambiguous.

3. `CatalogParentageAndNameUniquenessTest.IndexNameCollisionIsParentScoped`
- Verifies duplicate index names fail in same table scope and succeed across tables.

4. `CatalogRenameMoveTest.RenameTriggerUpdatesResolver`
- Regression guard for trigger rename + resolver/map consistency.

5. `CatalogDatabaseBootstrapTest.*`
- Regression guard ensuring parentage validation changes do not break canonical bootstrap/object sync.
