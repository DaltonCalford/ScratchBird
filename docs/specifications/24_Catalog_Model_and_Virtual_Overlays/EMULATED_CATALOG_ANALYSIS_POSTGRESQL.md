# Emulated Catalog Analysis: PostgreSQL 18.x

## Purpose
Determine how PostgreSQL catalog tables map to ScratchBird canonical catalog data, and identify which catalogs are virtual overlays versus canonical storage requirements.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data or runtime state.
- `runtime`: derived from runtime state (sessions, stats).
- `gated`: only exposed if the feature is enabled in the emulation profile.

## Mapping Table
| PostgreSQL catalog | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| pg_database | Database registry | `database` | canonical | One row per DB. |
| pg_namespace | Schema registry | `schema` | canonical | Schema hierarchy. |
| pg_class | Tables/views/indexes/sequences | `object`, `table`, `view`, `index`, `sequence` | canonical | Relation registry. |
| pg_attribute | Column metadata | `column` | canonical | Column definitions. |
| pg_type | Type registry | `type`, `domain` | canonical | Types and domains. |
| pg_proc | Functions/procedures | `function`, `procedure` | canonical | Routine registry. |
| pg_aggregate | Aggregate metadata | `function` (aggregate flag) | canonical | Aggregate functions. |
| pg_operator | Operators | `operator` | canonical | Requires operator registry. |
| pg_opclass | Operator classes | `index_opclass` | canonical | Index operator classes. |
| pg_opfamily | Operator families | `index_opclass` | canonical | Family grouping. |
| pg_am | Access methods | `index_access_method` | canonical | Index access methods. |
| pg_amop | AM operator mapping | `index_opclass` | canonical | Operator mapping. |
| pg_amproc | AM support functions | `index_opclass_fn` | canonical | Support function bindings. |
| pg_index | Index metadata | `index`, `index_column` | canonical | Index definition details. |
| pg_constraint | Constraints | `table_constraint`, `fk_constraint`, `check_constraint` | canonical | Constraint registry. |
| pg_cast | Cast graph | `type_cast` | canonical | Cast rules. |
| pg_collation | Collation registry | `collation` | canonical | Collation catalog. |
| pg_conversion | Encoding conversions | `encoding_conversion` | canonical | Encoding conversions. |
| pg_language | Language registry | `language` | canonical | Stored proc languages. |
| pg_depend | Dependency graph | `dependency` | canonical | Object dependency. |
| pg_shdepend | Shared dependency graph | `dependency` | canonical | Shared dependencies. |
| pg_description | Object comments | `object_comment` | canonical | Object comments. |
| pg_shdescription | Shared object comments | `object_comment` | canonical | Shared comments. |
| pg_db_role_setting | Role settings | `role_setting` | canonical | Role/database settings. |
| pg_default_acl | Default ACLs | `default_privilege` | canonical | Default privileges. |
| pg_seclabel | Security labels | `security_label` | canonical | Security labels. |
| pg_policy | Row-level security | `policy` | canonical | RLS policies. |
| pg_event_trigger | Event triggers | `trigger` | canonical | Event triggers. |
| pg_rewrite | Rules | `rule` | canonical | View/rule rewrite rules. |
| pg_trigger | Triggers | `trigger` | canonical | Trigger registry. |
| pg_foreign_data_wrapper | FDW registry | `fdw` | canonical | FDW metadata. |
| pg_foreign_server | Foreign server | `fdw_server` | canonical | FDW server metadata. |
| pg_foreign_table | Foreign tables | `foreign_table` | canonical | Foreign table bindings. |
| pg_user_mapping | User mappings | `fdw_user_mapping` | canonical | FDW user mapping. |
| pg_tablespace | Tablespace registry | `filespaces` | canonical | Filespace mapping. |
| pg_inherits | Table inheritance | `table_inheritance` | canonical | Only if table inheritance supported. |
| pg_partitioned_table | Partition metadata | `partitioned_table` | canonical | Partitioned table metadata. |
| pg_range | Range type registry | `type` | canonical | Range types. |
| pg_enum | Enum type labels | `type` | canonical | Enum labels. |
| pg_transform | Type transforms | `type_transform` | canonical | Transform bindings. |
| pg_extension | Extensions | `extension` | canonical | Extension registry. |
| pg_publication | Publications | `publication` | canonical | Logical replication. |
| pg_publication_namespace | Publication schemas | `publication_schema` | canonical | Publication schema mapping. |
| pg_publication_rel | Publication relations | `publication_table` | canonical | Publication table mapping. |
| pg_subscription | Subscriptions | `subscription` | canonical | Subscription metadata. |
| pg_subscription_rel | Subscription relations | `subscription_table` | canonical | Subscription mapping. |
| pg_largeobject | Large object data | `lob_page` | canonical | LOB storage pages. |
| pg_largeobject_metadata | LOB metadata | `lob` | canonical | LOB metadata. |
| pg_statistic | Statistics | `table_stats`/`column_stats` | canonical | Optimizer stats. |
| pg_statistic_ext | Extended stats | `statistic_ext` | canonical | Extended stats metadata. |
| pg_statistic_ext_data | Extended stats data | `statistic_ext_data` | canonical | Extended stats payload. |
| pg_ts_config | Text search config | `ts_config` | canonical | Full text config. |
| pg_ts_config_map | Text search map | `ts_config_map` | canonical | Token mapping. |
| pg_ts_dict | Text search dictionary | `ts_dictionary` | canonical | Dictionary registry. |
| pg_ts_parser | Text search parser | `ts_parser` | canonical | Parser registry. |
| pg_ts_template | Text search template | `ts_template` | canonical | Template registry. |

## Notes
- Many PostgreSQL catalogs can be exposed as **views over canonical ScratchBird tables**, no PostgreSQL-specific storage is required.
- Catalogs marked `canonical` above indicate the **underlying data** must be persisted in ScratchBird; the PostgreSQL view layer remains virtual.
- Items such as `pg_inherits`, `pg_partitioned_table`, `pg_publication*`, `pg_subscription*` are `gated` by emulation profile if those features are enabled.

## Resolved Decisions
- `rule`, `extension`, `fdw*`, `language`, `type_transform`, and `encoding_conversion` are canonical and required.
- Logical replication catalog coverage is required in Alpha for PostgreSQL emulation and maps to:
  - `publication`, `publication_schema`, `publication_table`
  - `subscription`, `subscription_table`.

## Open Questions
- None.
