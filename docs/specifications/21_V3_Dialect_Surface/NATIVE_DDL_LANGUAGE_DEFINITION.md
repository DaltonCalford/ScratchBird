# Native DDL Language Definition

## Current code-backed truth
- Real parser entry points exist for core DDL families including create, alter, drop, and truncate.
- Real native create-path entry points include table, index, view, sequence, schema, database, tablespace, domain, function, procedure, trigger, package, exception, type, user, role, group, policy, foreign-server, foreign-table, foreign-data-wrapper, user-mapping, synonym, UDR, and job surfaces.
- Real alter and drop entry points exist for a similarly broad set of object families.

## Proven anchors
- `include/scratchbird/parser/parser_v3.h`
- `src/parser/parser_v3.cpp`
- `include/scratchbird/parser/ast_v3.h`

## Boundary
- Parser-family presence is proven.
- Exact clause-order completeness, semantic bind coverage, and runtime catalog parity for every object family remain partial.
- Treat this file as parser-definition authority, not as proof that every DDL clause executes end to end today.
