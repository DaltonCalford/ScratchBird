### ScratchBird User Documentation

This guide is sourced entirely from the repository. It explains what each SQL/PSQL element is, why you would use it, and how to use it, with cross-links across the docs. The code is authoritative; where parser acceptance differs from runtime semantics, that is called out explicitly.

Audience: end users of the SQL surface and developers extending or integrating the engine.

How to navigate:
- Start with the Overview, then explore Core SQL (Lexical, Operators, Reserved Words, Data Types), SELECT and DML, and finally DDL by object type. Session/Transaction and PSQL cover procedural and control surfaces.
- Each page ends with a See also linking to related sections.

Core language:
- [Overview](./sql-overview.md)
- [Lexical](./sql-lexical.md)
- [Operators](./sql-operators.md)
- [Reserved words](./sql-reserved-words.md)
- [Data types](./sql-data-types.md)
- [SELECT](./sql-select.md)
- [DML](./sql-dml.md)

DDL by object type:
- [Tables](./ddl-tables.md) · [Indexes](./ddl-indexes.md) · [Schemas](./ddl-schemas.md) · [Views](./ddl-views.md)
- [Sequences](./ddl-sequences.md) · [Domains](./ddl-domains.md) · [Collations & charsets](./ddl-collations-charsets.md)
- [Exceptions & comments](./ddl-exceptions-and-comments.md) · [Roles, users, grants](./ddl-roles-users-grants.md)
- [Tablespaces](./ddl-tablespaces.md)
- [Foreign data](./ddl-foreign-data.md)
- [Materialized views](./ddl-materialized-views.md)
- [Publication & subscription](./ddl-publication-subscription.md) · [Policies (RLS)](./ddl-policies-rls.md)
- [Cluster](./ddl-cluster.md) · [Database links](./ddl-database-links.md)
- [Blob filter, mapping, GTT](./ddl-blob-filter-mapping-gtt.md)

Operational and procedural:
- [Session & transaction](./session-and-transaction.md)
- [PSQL runtime](./psql-runtime.md) · [Routines & triggers](./psql-routines-and-triggers.md)
- [EXPLAIN / ANALYZE](./explain-analyze.md)

Environment:
- [Configuration](./configuration.md) · [CLI tools](./cli-tools.md) · [Installation](./installation.md)
- [Developer tools](./dev-tools.md)

Meta:
- [Missing and future](./missing-and-future.md)

