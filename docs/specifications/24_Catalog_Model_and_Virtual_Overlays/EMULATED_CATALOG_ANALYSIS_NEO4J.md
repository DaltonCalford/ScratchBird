# Emulated Catalog Analysis: Neo4j 5.x

## Purpose
Identify Neo4j schema and catalog surfaces and how they map to ScratchBird canonical catalog data.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data.
- `runtime`: derived from runtime state.
- `gated`: exposed only if feature is enabled.

## Mapping Table
| Neo4j surface | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| SHOW DATABASES | Database list | `database` | virtual | Derived view. |
| SHOW INDEXES | Index metadata | `index`, `index_column`, `index_stats` | virtual | Index overlay. |
| SHOW CONSTRAINTS | Constraints | `table_constraint` | virtual | Constraint overlay. |
| SHOW FUNCTIONS | Functions | `function` | virtual | Function overlay. |
| SHOW PROCEDURES | Procedures | `procedure` | virtual | Procedure overlay. |
| db.schema.nodeTypeProperties | Labels/property keys | `object_name`, `column` | virtual | Derived from object/type registry. |
| db.schema.relationshipTypeProperties | Relationship types | `object_name` | virtual | Derived mapping. |
| db.stats.retrieve | Stats/metrics | `table_stats`, `index_stats` | runtime | Derived runtime metrics. |

## Notes
- Neo4j’s schema is exposed via procedures and SHOW commands; ScratchBird provides **virtual overlays** from canonical catalogs.
- Graph labels, relationship types, and property keys are derived from ScratchBird schema metadata and name registry.

## Resolved Decisions
- Alpha Neo4j emulation index scope is fixed to `LOOKUP`, `TEXT`, `RANGE`, `POINT`, and `VECTOR`, mapped to canonical index types defined in section 18.

## Open Questions
- None.
