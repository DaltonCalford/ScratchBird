# Firebird BLR to ScratchBird SBLR Mapping

Status: Draft (compatibility guidance)

This document maps Firebird BLR opcodes to ScratchBird SBLR concepts and opcodes.
It is intended for emulation paths that must accept BLR (from Firebird clients,
metadata, or stored code) and translate it into SBLR for execution.

Sources:
- Firebird BLR constants: `Firebird-6.0.0.1124-1ccdf1c-source/src/include/firebird/impl/blr.h`
- ScratchBird SBLR opcodes: `ScratchBird/include/scratchbird/sblr/opcodes.h`
- SBLR type system: `ScratchBird/docs/specifications/Appendix_A_SBLR_BYTECODE.md`

## Principles
- BLR is a low-level engine bytecode; SBLR is a higher-level executor bytecode.
  Mapping is semantic, not 1:1.
- BLR `blr_message` blocks describe input/output message formats. SBLR carries
  parameter metadata outside the opcode stream; map BLR messages to
  `EXT_PARAM_IN/OUT/INOUT` metadata, not inline opcodes.
- When BLR has no direct SBLR equivalent, the adapter must either:
  1) rewrite into supported SBLR sequences, or
  2) reject and record the opcode as an implementation gap.

## Type Mapping (BLR type codes -> SBLR types)
SBLR type IDs are from Appendix A; opcode markers are from `opcodes.h`.

| BLR type | Meaning | SBLR type id / opcode |
| --- | --- | --- |
| blr_short | INT16 | SBLR_TYPE_SHORT + `Opcode::TYPE_INT16` |
| blr_long | INT32 | SBLR_TYPE_LONG + `Opcode::TYPE_INTEGER` |
| blr_int64 | INT64 | SBLR_TYPE_INT8 + `Opcode::TYPE_BIGINT` |
| blr_int128 | INT128 | SBLR_TYPE_INT128 (no base opcode marker) |
| blr_dec64 / blr_dec128 | decimal float | SBLR_TYPE_DECIMAL (precision/scale); no dec64/dec128 marker |
| blr_float | FLOAT32 | SBLR_TYPE_FLOAT + `Opcode::TYPE_FLOAT32` |
| blr_double / blr_d_float | FLOAT64 | SBLR_TYPE_DOUBLE + `Opcode::TYPE_DOUBLE` |
| blr_text / blr_text2 | CHAR(n) | SBLR_TYPE_TEXT or `Opcode::TYPE_CHAR` (with length/charset) |
| blr_varying / blr_varying2 | VARCHAR(n) | SBLR_TYPE_VARYING + `Opcode::TYPE_VARCHAR` |
| blr_cstring / blr_cstring2 | CSTRING(n) | Map to TEXT + NUL termination (no direct opcode) |
| blr_blob / blr_blob2 | BLOB | SBLR_TYPE_BLOB + `Opcode::TYPE_BLOB` |
| blr_sql_date | DATE | SBLR_TYPE_SQL_DATE + `Opcode::TYPE_DATE` |
| blr_sql_time | TIME | SBLR_TYPE_SQL_TIME + `Opcode::TYPE_TIME` |
| blr_timestamp | TIMESTAMP | SBLR_TYPE_TIMESTAMP + `Opcode::TYPE_TIMESTAMP` |
| blr_sql_time_tz / blr_timestamp_tz / blr_ex_* | time zone variants | No SBLR type marker yet (gap) |
| blr_bool | BOOLEAN | SBLR_TYPE_BOOLEAN + `Opcode::TYPE_BOOLEAN` |
| blr_domain_name* / blr_column_name* | TYPE OF | SBLR_TYPE_DOMAIN (requires catalog lookup) |

## Statement and Control-Flow Mapping

| BLR opcode(s) | Semantics | SBLR mapping | Notes |
| --- | --- | --- | --- |
| blr_begin / blr_end / blr_block | block | `Opcode::EXT_BLOCK` + `Opcode::END` | block boundaries |
| blr_if | IF statement | `Opcode::EXT_IF` + jump ops | use `EXT_JUMP_IF_TRUE/FALSE` |
| blr_value_if | IIF expression | `Opcode::CASE_WHEN` | expression form |
| blr_loop | LOOP | `Opcode::EXT_LOOP` + jumps | |
| blr_continue_loop | CONTINUE | `Opcode::EXT_JUMP` | no direct opcode |
| blr_leave | EXIT | `Opcode::EXT_EXIT` | |
| blr_for | FOR (cursor loop) | SELECT + loop | no dedicated SBLR opcode |
| blr_for_range | FOR range | needs extension | no SBLR opcode |
| blr_assignment | assignment | `Opcode::ASSIGNMENT` / `Opcode::EXT_VAR_STORE` | column vs variable |
| blr_dcl_variable / blr_init_variable | declare/init | `Opcode::EXT_DECLARE` + `EXT_VAR_STORE` | |
| blr_dcl_cursor / blr_cursor_stmt | cursor declaration | needs extension | no cursor opcodes |
| blr_handler / blr_error_handler | exception handler | `Opcode::EXT_TRY` + `EXT_EXCEPT_HANDLER` | |
| blr_abort / blr_raise / blr_exception* | raise/abort | `Opcode::EXT_RAISE` | map status payload separately |
| blr_start_savepoint / blr_end_savepoint | savepoints | ExtendedOpcode::EXT_SAVEPOINT_BEGIN / EXT_SAVEPOINT_END | implicit savepoint scope |

## DML and RSE (Query Structure)

| BLR opcode(s) | Semantics | SBLR mapping | Notes |
| --- | --- | --- | --- |
| blr_store / blr_store2 / blr_store3 | INSERT | `Opcode::INSERT` + `COLUMN_DEF` + `ASSIGNMENT` | store3 override flags need new payload |
| blr_modify / blr_modify2 | UPDATE | `Opcode::UPDATE` + `ASSIGNMENT` | |
| blr_erase / blr_erase2 | DELETE | `Opcode::DELETE` | |
| blr_rse / blr_project | SELECT | `Opcode::SELECT` + column list | |
| blr_relation* | table stream | `Opcode::TABLE_REF` | |
| blr_sort / blr_ascending / blr_descending | ORDER BY | `Opcode::ORDER_BY`, `SORT_KEY`, `SORT_ASC/DESC` | |
| blr_nullsfirst / blr_nullslast | NULL ordering | `Opcode::NULLS_FIRST/NULLS_LAST` | |
| blr_group_by / blr_aggregate | GROUP BY | `Opcode::GROUP_BY` + AGG_* | |
| blr_agg_* | aggregates | `Opcode::AGG_*` | DISTINCT variants need flag support |
| blr_agg_list* | LIST aggregate | needs extension | no string_agg opcode |
| blr_first | FIRST | `Opcode::LIMIT` | |
| blr_skip | SKIP | `Opcode::OFFSET` | |
| blr_union | UNION | `Opcode::EXT_UNION` / `EXT_UNION_ALL` | requires ALL flag |
| blr_join / blr_merge / blr_join_type | JOIN | `Opcode::NESTED_LOOP_JOIN/HASH_JOIN` + `JOIN_TYPE` + `JOIN_CONDITION` | |
| blr_plan / blr_retrieve / blr_indices / blr_sequential / blr_navigational | plan hints | `Opcode::SCAN_HINT` + `INDEX_REF` | partial |
| blr_lateral_rse | LATERAL | needs extension | no lateral opcode |

## Expressions and Predicates

| BLR opcode(s) | Semantics | SBLR mapping | Notes |
| --- | --- | --- | --- |
| blr_add / blr_subtract / blr_multiply / blr_divide | arithmetic | `Opcode::EXPR_ADD/SUBTRACT/MULTIPLY/DIVIDE` | |
| blr_negate | unary minus | needs extension | can be rewritten as 0 - expr |
| blr_concatenate | string concat | needs extension | no concat opcode |
| blr_substring | substring | `Opcode::FUNC_SUBSTRING` | |
| blr_strlen | length | `Opcode::FUNC_LENGTH/CHAR_LENGTH/OCTET_LENGTH` | use subcode |
| blr_trim | trim | `Opcode::FUNC_TRIM` | |
| blr_upcase / blr_lowcase | case conversion | `Opcode::FUNC_UPPER/FUNC_LOWER` | |
| blr_extract | extract | `Opcode::EXT_EXTRACT` | |
| blr_cast / blr_cast_format | cast | `Opcode::EXPR_CAST` | |
| blr_eql / blr_neq / blr_gtr / blr_geq / blr_lss / blr_leq | comparisons | `Opcode::EXPR_EQ/NE/GT/GE/LT/LE` | |
| blr_between | BETWEEN | rewrite to comparisons | no direct opcode |
| blr_and / blr_or | boolean | `Opcode::EXPR_AND/EXPR_OR` | |
| blr_not | NOT | ExtendedOpcode::EXT_EXPR_NOT | unary predicate |
| blr_null | NULL literal | `Opcode::LITERAL_NULL` | |
| blr_missing | IS NULL | ExtendedOpcode::EXT_EXPR_IS_NULL | unary predicate |
| blr_like / blr_ansi_like | LIKE | `Opcode::EXPR_LIKE` (+ `EXT_LIKE_ESCAPE`) | |
| blr_matching / blr_matching2 / blr_similar | regex-like | `Opcode::EXT_REGEX_*` | may need dedicated opcode |
| blr_containing / blr_starting | contains/starts | rewrite to LIKE/STRPOS | no direct opcode |
| blr_any / blr_ansi_any | ANY subquery | `Opcode::EXT_SUBQUERY_IN` | needs ALL/ANY handling |
| blr_ansi_all | ALL subquery | needs extension | no ALL opcode |
| blr_exists | EXISTS | `Opcode::EXT_SUBQUERY_EXISTS` | |
| blr_unique | UNIQUE | needs extension | no opcode |
| blr_coalesce | COALESCE | `Opcode::COALESCE` | |
| blr_decode | DECODE | `Opcode::CASE_WHEN` | |

## Functions and System Variables

| BLR opcode(s) | Semantics | SBLR mapping | Notes |
| --- | --- | --- | --- |
| blr_gen_id / blr_gen_id2 | generator | `Opcode::SEQUENCE_NEXTVAL` | step support needs extension |
| blr_set_generator | set generator | `Opcode::SEQUENCE_SETVAL` | |
| blr_current_date | CURRENT_DATE | `Opcode::FUNC_CURRENT_DATE` | |
| blr_current_time / blr_current_time2 | CURRENT_TIME | needs extension | no opcode |
| blr_current_timestamp / blr_current_timestamp2 | CURRENT_TIMESTAMP | `Opcode::FUNC_NOW` | |
| blr_local_time / blr_local_timestamp | local time | needs extension | no opcode |
| blr_at (AT TIME ZONE) | time zone conversion | `Opcode::FUNC_AT_TIME_ZONE` | |
| blr_user_name / blr_current_role | context vars | needs extension | no opcode |
| blr_internal_info | internal info | needs extension | no opcode |
| blr_json_function | JSON function | `Opcode::JSON_*` | partial |

## Procedures and Subroutines

| BLR opcode(s) | Semantics | SBLR mapping | Notes |
| --- | --- | --- | --- |
| blr_procedure* / blr_function* | definition | `Opcode::EXT_PROCEDURE/EXT_FUNCTION` | definition only |
| blr_exec_proc / blr_exec_proc2 / blr_invoke_procedure | execute proc | needs extension | no call opcode |
| blr_invoke_function | invoke function | needs extension | no call opcode |
| blr_subproc* / blr_subfunc* | nested routines | needs extension | no direct support |
| blr_exec_into | SELECT INTO | rewrite to variable assignment | no explicit opcode |

## Unmatched Items (Implementation Gaps)
The following BLR items have no direct SBLR opcode and require extensions or
rewrite rules:
- Cursor support: `blr_dcl_cursor`, `blr_cursor_stmt`, cursor actions.
- Dynamic SQL: `blr_exec_sql`, `blr_exec_stmt` (subcodes).
- Lock hints: `blr_writelock`, `blr_skip_locked`.
- Local tables: `blr_dcl_local_table`, `blr_local_table_truncate`, `blr_local_table_id`.
- Record metadata: `blr_dbkey`, `blr_record_version`, `blr_record_version2`.
- System context: `blr_user_name`, `blr_current_role`, `blr_internal_info`.
- Time zone types: `blr_sql_time_tz`, `blr_timestamp_tz`, `blr_ex_time_tz`, `blr_ex_timestamp_tz`.
- Distinct aggregates: `blr_agg_count_distinct`, `blr_agg_total_distinct`, `blr_agg_average_distinct`.
- LIST aggregate: `blr_agg_list`, `blr_agg_list_distinct`.
- String concat: `blr_concatenate`.
- LATERAL and advanced RSE: `blr_lateral_rse`, `blr_outer_map` family.
- FOR range: `blr_for_range` and subcodes.
- IN ALL/UNIQUE: `blr_ansi_all`, `blr_unique`.
- Substring SIMILAR: `blr_substring_similar`.
- Marks/instrumentation: `blr_marks`.

## Gap Prioritization (Initial)
- P1: Cursors (`blr_dcl_cursor`, `blr_cursor_stmt`) — required for PSQL cursor
  loops, FETCH semantics, and client-side cursor parity.
- P2: EXECUTE STATEMENT (`blr_exec_stmt` + subcodes, `blr_exec_sql`, `blr_exec_into`) —
  dynamic SQL depends on savepoints and a stable execution API.

Fixtures for these gaps live in `ScratchBird/docs/specifications/FIREBIRD_BLR_FIXTURES.md`.

## Notes for Implementers
- Use `include/scratchbird/sblr/opcodes.h` as the authority for SBLR opcodes.
- When rewriting BLR constructs into SBLR sequences, preserve Firebird semantics:
  NULL handling, string comparison rules, and type coercions differ from ANSI SQL.
- Track any unmatched BLR opcode encountered at runtime and surface a clear
  error so gaps can be prioritized for parity work.
