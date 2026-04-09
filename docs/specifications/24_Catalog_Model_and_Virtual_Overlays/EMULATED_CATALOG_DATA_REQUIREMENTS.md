# Emulated Catalog Data Requirements (Canonical Backing)

## Purpose
Define the **canonical ScratchBird data** required to populate each emulated engine’s catalog views. The goal is to avoid mirroring foreign catalog schemas while still providing all required data via optimized canonical storage.

## Conventions
- Canonical tables are ScratchBird-owned and persisted unless marked runtime.
- Emulated catalogs are exposed as views derived from canonical data.
- “Required fields” lists canonical columns or sub‑entities that must exist to populate the emulated catalog correctly.

---

## Firebird 5.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| RDB$DATABASE | `database` | db_uuid, db_name, ods_version, default_charset, sql_security, linger | Single row per database. |
| RDB$RELATIONS | `table`, `view`, `object` | relation_uuid, relation_name, relation_type, owner, security_class, view_source, flags | Covers tables and views. |
| RDB$RELATION_FIELDS | `column` | column_uuid, relation_uuid, column_name, domain_uuid, is_nullable, default_expr, identity, generator | Firebird column metadata. |
| RDB$FIELDS | `domain`, `type`, `domain_parameter` | domain_uuid, domain_name, base_type, precision, scale, length, charset, collation, default, validation | Firebird domain metadata. |
| RDB$INDICES | `index` | index_uuid, index_name, relation_uuid, unique, inactive, index_type, expression, condition, statistics | Includes expression and partial index data. |
| RDB$INDEX_SEGMENTS | `index_column` | index_uuid, column_uuid, column_position, statistics | Column order and stats. |
| RDB$VIEW_RELATIONS | `dependency` | view_uuid, relation_uuid, context_name, context_type | Derived from dependency graph. |
| RDB$DEPENDENCIES | `dependency` | dependent_uuid, depended_on_uuid, dep_type, field_uuid | Dependency graph. |
| RDB$TRIGGERS | `trigger` | trigger_uuid, relation_uuid, trigger_type, source, blr, active, sequence | Trigger metadata. |
| RDB$PROCEDURES | `procedure` | procedure_uuid, procedure_name, owner, source, blr, package_uuid | Procedure metadata. |
| RDB$PROCEDURE_PARAMETERS | `procedure_param` | procedure_uuid, param_name, param_type, domain_uuid, default | Param metadata. |
| RDB$FUNCTIONS | `function` | function_uuid, function_name, module, entrypoint, return_type | UDF metadata. |
| RDB$FUNCTION_ARGUMENTS | `function_param` | function_uuid, param_name, param_type, domain_uuid, position | Param metadata. |
| RDB$GENERATORs | `sequence` | sequence_uuid, sequence_name, current_value | Sequences. |
| RDB$RELATION_CONSTRAINTS | `table_constraint` | constraint_uuid, relation_uuid, constraint_type, name, deferrable | Constraints. |
| RDB$REF_CONSTRAINTS | `fk_constraint` | fk_uuid, update_rule, delete_rule, match_type | FK rules. |
| RDB$CHECK_CONSTRAINTS | `check_constraint` | check_uuid, check_source, check_blr | Check expressions. |
| RDB$CHARACTER_SETS | `charset` | charset_uuid, name, bytes_per_char, default_collation | Charset registry. |
| RDB$COLLATIONS | `collation` | collation_uuid, name, charset_uuid, attributes | Collation registry. |
| RDB$EXCEPTIONS | `exception` | exception_uuid, name, message | Exception definitions. |
| RDB$ROLES | `role` | role_uuid, name, owner, admin_option | Role registry. |
| RDB$USER_PRIVILEGES | `permission` | grantee, grantor, object_uuid, privilege, grant_option | Privileges. |
| RDB$PACKAGES | `package` | package_uuid, name, header_source, body_source | Package metadata. |
| RDB$SECURITY_CLASSES | `security_class` | class_uuid, name, acl_blob | Security class ACLs. |
| RDB$SCHEMAS | `schema` | schema_uuid, name, owner, parent_uuid | Schema registry. |

---

## PostgreSQL 18.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| pg_database | `database` | db_uuid, db_name, owner, encoding, collate, ctype | Database registry. |
| pg_namespace | `schema` | schema_uuid, schema_name, owner | Schema registry. |
| pg_class | `object`, `table`, `view`, `index`, `sequence` | object_uuid, relname, relkind, relpages, reltuples, reloptions | Relation metadata. |
| pg_attribute | `column` | column_uuid, rel_uuid, attname, atttypid, attnum, attnotnull, atthasdef | Column definitions. |
| pg_type | `type`, `domain` | type_uuid, typname, typlen, typbyval, typtype, typcategory | Type registry. |
| pg_proc | `function`, `procedure` | proc_uuid, proname, prorettype, proargtypes, prosrc, prolang | Routine registry. |
| pg_operator | `operator` | opr_uuid, oprname, oprleft, oprright, oprresult, oprcode | Operator registry. |
| pg_cast | `type_cast` | cast_uuid, castsource, casttarget, castcontext, castmethod | Cast graph. |
| pg_constraint | `table_constraint`, `fk_constraint`, `check_constraint` | con_uuid, conname, contype, conrelid, confrelid, condeferrable | Constraints. |
| pg_index | `index`, `index_column` | index_uuid, table_uuid, indkey, indclass, indisunique, indpred | Index details. |
| pg_am | `index_access_method` | am_uuid, amname, amtype | Access method registry. |
| pg_opclass | `index_opclass` | opclass_uuid, opcname, opcfamily, opcintype | Operator classes. |
| pg_opfamily | `index_opclass` | opfamily_uuid, opfname | Operator families. |
| pg_amop | `index_opclass` | opfamily_uuid, amopopr, strategy | AM operator mapping. |
| pg_amproc | `index_opclass_fn` | opfamily_uuid, support_function | AM support functions. |
| pg_collation | `collation` | coll_uuid, collname, collprovider | Collation registry. |
| pg_language | `language` | lang_uuid, name, handler, validator, trust | Language registry. |
| pg_depend | `dependency` | dependent_uuid, depended_on_uuid, deptype | Dependency graph. |
| pg_description | `object_comment` | object_uuid, sub_id, comment_text, language | Comments. |
| pg_default_acl | `default_privilege` | role_uuid, object_type, acl | Default privileges. |
| pg_policy | `policy` | policy_uuid, rel_uuid, cmd, qual, with_check | RLS policies. |
| pg_ts_* | `ts_config`, `ts_config_map`, `ts_dictionary`, `ts_parser`, `ts_template` | all ids + config fields | Full text support. |
| pg_statistic | `column_stats` | rel_uuid, attnum, null_frac, ndistinct, mcv, histogram | Column stats. |
| pg_publication* | `publication`, `publication_schema`, `publication_table` | pub_uuid, name, rels | Logical replication metadata. |
| pg_subscription* | `subscription`, `subscription_table` | sub_uuid, name, rels | Subscription metadata. |

---

## MySQL 8.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| information_schema.SCHEMATA | `schema` | schema_uuid, schema_name, owner, default_charset, default_collation | Schema metadata. |
| information_schema.TABLES | `table`, `view` | table_uuid, table_name, table_type, row_format, create_time | Table metadata. |
| information_schema.COLUMNS | `column` | column_uuid, table_uuid, name, data_type, nullable, default | Column metadata. |
| information_schema.STATISTICS | `index`, `index_column`, `index_stats` | index_uuid, index_name, non_unique, seq_in_index, cardinality | Index metadata. |
| information_schema.TABLE_CONSTRAINTS | `table_constraint` | constraint_uuid, name, type, table_uuid | Constraints. |
| information_schema.KEY_COLUMN_USAGE | `fk_constraint`, `index_column` | fk_uuid, table_uuid, column_uuid | Key usage. |
| information_schema.REFERENTIAL_CONSTRAINTS | `fk_constraint` | fk_uuid, update_rule, delete_rule | FK metadata. |
| information_schema.CHECK_CONSTRAINTS | `check_constraint` | check_uuid, check_source | Check constraints. |
| information_schema.ROUTINES | `function`, `procedure` | routine_uuid, name, type, body, sql_security | Routines. |
| information_schema.PARAMETERS | `function_param`, `procedure_param` | routine_uuid, param_name, data_type, mode | Routine parameters. |
| information_schema.EVENTS | `event` | event_uuid, name, schedule, status | Event scheduler. |
| information_schema.TRIGGERS | `trigger` | trigger_uuid, name, timing, event, body | Triggers. |
| information_schema.VIEWS | `view` | view_uuid, name, definition | Views. |
| information_schema.PARTITIONS | `partition` | partition_uuid, table_uuid, method, expression | Partitions. |
| information_schema.CHARACTER_SETS | `charset` | charset_uuid, name, maxlen | Charset registry. |
| information_schema.COLLATIONS | `collation` | collation_uuid, name, charset_uuid | Collation registry. |

---

## Cassandra 5.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| system_schema.keyspaces | `schema` | schema_uuid, name, replication, durable_writes | Keyspace registry. |
| system_schema.tables | `table` | table_uuid, name, schema_uuid, flags | Table registry. |
| system_schema.columns | `column` | column_uuid, table_uuid, name, type, kind, position | Column metadata. |
| system_schema.types | `type`, `domain` | type_uuid, name, field_list | UDTs. |
| system_schema.indexes | `index` | index_uuid, name, table_uuid, kind, options | Index metadata. |
| system_schema.views | `view` | view_uuid, name, base_table_uuid, where_clause | Materialized view metadata. |
| system_schema.functions | `function` | function_uuid, name, arg_types, return_type, body | UDF registry. |
| system_schema.aggregates | `function` | aggregate_uuid, name, state_type, final_func | Aggregate registry. |
| system_schema.triggers | `trigger` | trigger_uuid, name, table_uuid, class_name | Trigger metadata. |
| system_schema.dropped_columns | `column_drop_history` | table_uuid, column_name, drop_time | Dropped column registry. |
| system.peers / peers_v2 | `node`, `shard_replica` | node_uuid, host, datacenter, rack, state, tokens | Cluster peer map. |

---

## MongoDB 8.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| listDatabases | `database` | db_uuid, db_name | Database list. |
| listCollections | `table`, `object_name` | collection_uuid, name, type, options | Collection metadata. |
| listIndexes | `index`, `index_column`, `index_stats` | index_uuid, name, key_spec, options | Index metadata. |
| system.views | `view` | view_uuid, name, pipeline | View definitions. |
| system.js | `function` | function_uuid, name, body, language=js | Stored JS (if enabled). |
| config.databases | `database` | db_uuid, db_name, primary_shard | Sharding database map. |
| config.collections | `table`, `shard_key` | collection_uuid, shard_key, unique, balancing | Sharded collection metadata. |
| config.shards | `shard`, `shard_replica`, `node` | shard_uuid, shard_name, host list | Shard membership list. |
| config.chunks | `shard_range`, `shard_scope` | namespace, min, max, shard_uuid | Chunk ranges. |
| config.tags | `shard_zone`, `shard_zone_range` | zone_name, min, max | Zone tags. |
| config.settings | `shard_policy` | balancer, chunk_size, autosplit | Sharding policy. |

---

## Neo4j 5.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| SHOW INDEXES | `index`, `index_stats` | index_uuid, name, type, state, size | Index metadata. |
| SHOW CONSTRAINTS | `table_constraint` | constraint_uuid, name, type, entity_type | Constraints. |
| SHOW PROCEDURES | `procedure` | procedure_uuid, name, signature | Procedures. |
| SHOW FUNCTIONS | `function` | function_uuid, name, signature | Functions. |
| db.schema.nodeTypeProperties | `object_name`, `column` | label_name, property_name, type | Label/property mapping. |

---

## Redis 7.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| CONFIG GET/SET | `sys.config.key`, `sys.config.value` | key, value, scope | Configuration storage. |
| COMMAND | `emulation_profile` | command_name, flags | Supported command list. |

---

## Milvus 2.x
| Emulated catalog surface | Canonical backing tables | Required fields (canonical) | Notes |
| --- | --- | --- | --- |
| listCollections | `table` | collection_uuid, name, schema_uuid | Collections. |
| describeCollection | `table`, `column` | fields, indexes, params | Collection description. |
| listPartitions | `partition` | partition_uuid, table_uuid, name | Partitions. |
| showIndexes | `index`, `index_option` | index_uuid, type, params | Vector index metadata. |
