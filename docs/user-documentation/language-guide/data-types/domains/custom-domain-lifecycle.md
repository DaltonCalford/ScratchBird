# Custom Domain Lifecycle
Last modified: 2026-02-19

Back links:
- [Domains README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Default Domain Bindings](default-domain-bindings.md)

Domain lifecycle command docs:
- [DDL DOMAIN object README](../../ddl/data-storage/domain/README.md)
- [CREATE DOMAIN](../../ddl/data-storage/domain/create.md)
- [ALTER DOMAIN](../../ddl/data-storage/domain/alter.md)
- [SHOW DOMAIN](../../ddl/data-storage/domain/show.md)
- [DROP DOMAIN](../../ddl/data-storage/domain/drop.md)

CREATE DOMAIN capabilities in native v3 include:
- kinds: BASIC (`AS <type>`), RECORD, ENUM, SET OF, VARIANT
- inheritance: `INHERITS(<parent_domain>)`
- constraints: `NOT NULL`, `DEFAULT`, `CHECK`, named constraints
- optional blocks: `WITH DIALECT`, `WITH COMPAT`, `WITH INTEGRITY`, `WITH SECURITY`, `WITH VALIDATION`, `WITH QUALITY`, `WITH OPTIONS`
