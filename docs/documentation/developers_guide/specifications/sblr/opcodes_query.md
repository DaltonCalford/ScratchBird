# Specification: SBLR SELECT & Query Opcodes

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:116,215-283`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:714`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:833-840`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_jit_functions.cpp`

## Synopsis

This specification defines the SELECT statement and query processing opcodes for SBLR v3. These opcodes handle query construction including FROM, WHERE, GROUP BY, HAVING, ORDER BY, LIMIT/OFFSET, and set operations (UNION, INTERSECT, EXCEPT).

## Scope

### In Scope

- SELECT statement structure and execution
- Table references and JOIN operations
- Filter clauses (WHERE, HAVING)
- Grouping operations (GROUP BY, ROLLUP, CUBE)
- Sorting (ORDER BY) and pagination (LIMIT/OFFSET)
- Set operations (UNION, INTERSECT, EXCEPT)
- Common Table Expressions (CTE) with RECURSIVE

### Out of Scope

- Expression evaluation (see opcodes_expressions.md)
- Aggregate and window functions (see opcodes_expressions.md)
- DML operations (see opcodes_dml.md)

## Background

Query processing in SBLR follows the standard SQL execution pipeline. The executor builds a result set by scanning tables, applying filters, grouping, sorting, and projecting columns. Query plans may use various join algorithms and access methods.

## Specification

### Core Query Opcodes

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_SELECT | 0x0212 | Begin SELECT statement |
| SBLR3_SELECT_STAR | 0x0658 | SELECT * |
| SBLR3_SELECT_TABLE_STAR | 0x062D | SELECT table.* |
| SBLR3_TABLE_REF | 0x065C | Table reference |
| SBLR3_COLUMN_REF | 0x0604 | Column reference |
| SBLR3_INSERTED_COLUMN_REF | 0x0617 | Reference to INSERTED pseudo-table |
| SBLR3_WHERE_CLAUSE | 0x065D | WHERE filter |
| SBLR3_GROUP_BY | 0x0649 | GROUP BY clause |
| SBLR3_HAVING | 0x064B | HAVING filter |
| SBLR3_ORDER_BY | 0x0655 | ORDER BY clause |
| SBLR3_LIMIT | 0x0650 | LIMIT clause |
| SBLR3_OFFSET | 0x0654 | OFFSET clause |
| SBLR3_DISTINCT_ON | 0x060A | DISTINCT ON expression |

---

### SBLR3_SELECT (0x0212)

```cpp
// Source: src/sblr/executor.cpp:714
void executeSelect();
```

**Purpose**: Execute a SELECT query.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| select_list_type | uint8_t | 0=STAR, 1=expressions, 2=TABLE_STAR |
| distinct | uint8_t | 0=ALL, 1=DISTINCT, 2=DISTINCT ON |
| select_count | uint16_t | Number of select items |
| select_items | SelectItem[] | Column expressions |
| from_present | uint8_t | 0=no FROM, 1=FROM clause |
| from_data | bytes | FROM clause bytecode |
| where_present | uint8_t | 0=no WHERE, 1=WHERE clause |
| where_data | bytes | WHERE expression bytecode |
| group_by_present | uint8_t | 0=no GROUP BY, 1=present |
| group_by_data | bytes | GROUP BY bytecode |
| having_present | uint8_t | 0=no HAVING, 1=present |
| having_data | bytes | HAVING expression bytecode |
| window_present | uint8_t | 0=no WINDOW, 1=present |
| window_data | bytes | WINDOW clause bytecode |
| order_by_present | uint8_t | 0=no ORDER BY, 1=present |
| order_by_data | bytes | ORDER BY bytecode |
| limit_present | uint8_t | 0=no LIMIT, 1=present |
| limit_data | bytes | LIMIT expression bytecode |
| offset_present | uint8_t | 0=no OFFSET, 1=present |
| offset_data | bytes | OFFSET expression bytecode |
| for_clause_present | uint8_t | 0=no FOR, 1=present |
| for_clause_type | uint8_t | 1=UPDATE, 2=SHARE, 3=KEY SHARE, 4=NO KEY SHARE |

**SelectItem Structure:**
| Field | Type | Description |
|-------|------|-------------|
| kind | uint8_t | 0=EXPR, 1=STAR, 2=TABLE_STAR |
| expr_len | uint32_t | Expression bytecode length |
| expr_data | bytes | Expression bytecode |
| alias_len | uint16_t | Output column alias length |
| alias | char[] | Column alias |
| table_id | UUID | For TABLE_STAR |

**Execution Semantics:**

```
Input: SELECT payload with all clauses
Output: ResultSet with rows and columns

1. Process FROM clause:
   a. Resolve table references
   b. Execute JOIN operations
   c. Build row source

2. Apply WHERE filter:
   a. For each candidate row
   b. Evaluate WHERE expression
   c. Discard rows where result is FALSE or NULL

3. Process GROUP BY:
   a. If aggregates without GROUP BY:
      - Compute scalar aggregates
   b. If GROUP BY present:
      - Group rows by grouping columns
      - Compute aggregates per group
   c. If ROLLUP/CUBE/GROUPING SETS:
      - Generate grouping sets
      - Compute aggregates for each set

4. Apply HAVING filter:
   a. For each grouped result
   b. Evaluate HAVING expression
   c. Discard groups where FALSE

5. Compute SELECT list:
   a. For each surviving row/group
   b. Evaluate select expressions
   c. Apply DISTINCT if specified
   d. Build output row

6. Process window functions:
   a. Partition rows
   b. Order within partitions
   c. Compute window functions

7. Sort (ORDER BY):
   a. Sort result set by sort keys

8. Apply LIMIT/OFFSET:
   a. Skip OFFSET rows
   b. Return at most LIMIT rows

9. Return final ResultSet
```

---

### FROM Clause Opcodes

#### SBLR3_TABLE_REF (0x065C)

**Purpose**: Reference a table in FROM clause.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_id | UUID | Table UUID (if known) |
| name_len | uint16_t | Table name length |
| name | char[] | Qualified table name |
| alias_len | uint16_t | Table alias length |
| alias | char[] | Alias for this reference |
| sample_present | uint8_t | 0=no, 1=TABLESAMPLE |
| sample_method_len | uint16_t | Sampling method length |
| sample_method | char[] | SYSTEM or BERNOULLI |
| sample_percent | double | Sample percentage |
| sample_seed_present | uint8_t | 0=no, 1=yes |
| sample_seed | int64_t | Random seed |

#### SBLR3_JOIN_TYPE (0x064E)

**Purpose**: Specify JOIN type.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| join_type | uint8_t | 1=CROSS, 2=INNER, 3=LEFT, 4=RIGHT, 5=FULL |
| natural | uint8_t | 0=false, 1=true |

#### SBLR3_JOIN_CONDITION (0x064D)

**Purpose**: Specify JOIN condition (ON clause).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| condition_len | uint32_t | Expression bytecode length |
| condition | bytes | Boolean expression bytecode |

#### SBLR3_JOIN_USING (0x064F)

**Purpose**: Specify USING columns for JOIN.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| column_count | uint16_t | Number of columns |
| columns | char[][] | Column names |

#### SBLR3_NESTED_LOOP_JOIN (0x0651)

```cpp
// Source: src/sblr/executor.cpp:724
void executeNestedLoopJoin();
```

**Purpose**: Execute nested loop join algorithm.

**Execution:**
```
For each outer row:
  For each inner row:
    If join condition matches:
      Output combined row
```

#### SBLR3_HASH_JOIN (0x064A)

```cpp
// Source: src/sblr/executor.cpp:725
void executeHashJoin();
```

**Purpose**: Execute hash join algorithm.

**Execution:**
```
Build phase:
  For each inner row:
    Compute hash on join key
    Add to hash table

Probe phase:
  For each outer row:
    Compute hash on join key
    Probe hash table
    For each match:
      Verify condition
      Output combined row
```

---

### Filter Clause Opcodes

#### SBLR3_WHERE_CLAUSE (0x065D)

**Purpose**: Filter rows before grouping.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| condition_len | uint32_t | Expression bytecode length |
| condition | bytes | Boolean expression bytecode |

**Execution:**
```
For each candidate row:
  Evaluate condition
  If result is TRUE: keep row
  If result is FALSE or NULL: discard row
```

#### SBLR3_HAVING (0x064B)

**Purpose**: Filter groups after aggregation.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| condition_len | uint32_t | Expression bytecode length |
| condition | bytes | Boolean expression bytecode |

**Execution:**
```
For each grouped result:
  Evaluate condition (can reference aggregates)
  If result is TRUE: keep group
  If result is FALSE or NULL: discard group
```

---

### Grouping Opcodes

#### SBLR3_GROUP_BY (0x0649)

**Purpose**: Group rows for aggregation.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| grouping_type | uint8_t | 0=SIMPLE, 1=ROLLUP, 2=CUBE, 3=GROUPING SETS |
| set_count | uint16_t | Number of grouping sets |
| sets | GroupingSet[] | Grouping set definitions |

**GroupingSet Structure:**
| Field | Type | Description |
|-------|------|-------------|
| column_count | uint16_t | Columns in this set |
| columns | uint16_t[] | Column indices/expressions |

#### SBLR3_GROUP_ROLLUP (0x0616)

**Purpose**: ROLLUP grouping specification.

**Semantics:** Generates hierarchical subtotals.

```
ROLLUP(a, b, c) produces:
  GROUP BY (a, b, c)
  GROUP BY (a, b)
  GROUP BY (a)
  GROUP BY ()
```

#### SBLR3_GROUP_CUBE (0x0612)

**Purpose**: CUBE grouping specification.

**Semantics:** Generates all possible combinations.

```
CUBE(a, b, c) produces:
  GROUP BY (a, b, c)
  GROUP BY (a, b)
  GROUP BY (a, c)
  GROUP BY (b, c)
  GROUP BY (a)
  GROUP BY (b)
  GROUP BY (c)
  GROUP BY ()
```

#### SBLR3_GROUP_GROUPING_SETS (0x0614)

**Purpose**: Explicit grouping sets.

**Semantics:** Each set is grouped independently.

#### SBLR3_GROUPING_FUNC (0x0610)

**Purpose**: GROUPING() function for super-aggregate rows.

**Execution:**
```
GROUPING(column) returns:
  0 if column is part of current grouping
  1 if column is aggregated (super-aggregate)
```

---

### Sorting Opcodes

#### SBLR3_ORDER_BY (0x0655)

**Purpose**: Sort result set.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| key_count | uint16_t | Number of sort keys |
| keys | SortKey[] | Sort key definitions |

**SortKey Structure:**
| Field | Type | Description |
|-------|------|-------------|
| expr_len | uint32_t | Expression bytecode length |
| expr_data | bytes | Expression bytecode |
| direction | uint8_t | 1=ASC, 2=DESC |
| nulls | uint8_t | 1=FIRST, 2=LAST |

#### SBLR3_SORT_ASC (0x0659)

**Purpose**: ASCending sort modifier.

**Payload:** None (marker)

#### SBLR3_SORT_DESC (0x065A)

**Purpose**: DESCending sort modifier.

**Payload:** None (marker)

#### SBLR3_NULLS_FIRST (0x0652)

**Purpose**: NULLS FIRST sort modifier.

**Payload:** None (marker)

#### SBLR3_NULLS_LAST (0x0653)

**Purpose**: NULLS LAST sort modifier.

**Payload:** None (marker)

**Sorting Execution:**

```cpp
// Source: src/sblr/executor.cpp:833
void executeSort(std::unique_ptr<ResultSet> input_result_set, bool apply_limit = true);
```

```
Input: Unsorted result set
Output: Sorted result set

1. Determine sort key values for each row
2. Apply sort algorithm (timsort/quicksort)
3. Comparison rules:
   a. Compare first key according to direction
   b. If equal, compare second key
   c. Continue for all keys
   d. NULLs according to NULLS FIRST/LAST
4. Return sorted result
```

---

### Pagination Opcodes

#### SBLR3_LIMIT (0x0650)

**Purpose**: Limit result row count.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| count_is_expression | uint8_t | 0=constant, 1=expression |
| count_constant | uint64_t | Constant limit value |
| count_expr_len | uint32_t | Expression bytecode length |
| count_expr | bytes | Expression bytecode |
| with_ties | uint8_t | 0=false, 1=true (include ties) |

#### SBLR3_OFFSET (0x0654)

**Purpose**: Skip initial rows.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| count_is_expression | uint8_t | 0=constant, 1=expression |
| count_constant | uint64_t | Constant offset value |
| count_expr_len | uint32_t | Expression bytecode length |
| count_expr | bytes | Expression bytecode |

**Limit/Offset Execution:**

```cpp
// Source: src/sblr/executor.cpp:836
void executeLimit(std::unique_ptr<ResultSet> input_result_set);
```

```
Input: Result set
Output: Paginated result set

1. Evaluate LIMIT expression (if dynamic)
2. Evaluate OFFSET expression (if dynamic)
3. If OFFSET > 0:
   - Skip first OFFSET rows
4. Return at most LIMIT rows
5. If WITH TIES and last row has peers:
   - Include all tied rows
```

---

### Set Operation Opcodes

#### SBLR3_UNION (0x063B)

```cpp
// Source: src/sblr/executor.cpp:746
void executeUnion();
```

**Purpose**: Combine queries, remove duplicates.

**Execution:**
```
1. Execute left query
2. Execute right query
3. Ensure column count and types match
4. Concatenate results
5. Remove duplicate rows
6. Apply ORDER BY if specified
```

#### SBLR3_UNION_ALL (0x063D)

```cpp
// Source: src/sblr/executor.cpp:745
void executeUnionAll();
```

**Purpose**: Combine queries, keep duplicates.

**Execution:** Same as UNION but skip duplicate removal.

#### SBLR3_INTERSECT (0x0619)

```cpp
// Source: src/sblr/executor.cpp:748
void executeIntersect();
```

**Purpose**: Rows present in both queries.

**Execution:**
```
1. Execute both queries
2. Build hash set from left results
3. For each right row:
   - If in left set: output
4. Remove duplicates
```

#### SBLR3_INTERSECT_ALL (0x061B)

```cpp
// Source: src/sblr/executor.cpp:747
void executeIntersectAll();
```

**Purpose**: Rows present in both, with duplicates (min count).

#### SBLR3_EXCEPT (0x060C)

```cpp
// Source: src/sblr/executor.cpp:750
void executeExcept();
```

**Purpose**: Rows in left but not right.

**Execution:**
```
1. Execute both queries
2. Build hash set from right results
3. For each left row:
   - If NOT in right set: output
4. Remove duplicates
```

#### SBLR3_EXCEPT_ALL (0x060E)

```cpp
// Source: src/sblr/executor.cpp:749
void executeExceptAll();
```

**Purpose**: Rows in left but not right, with duplicates.

---

### CTE (Common Table Expression) Opcodes

#### SBLR3_WITH_CLAUSE (0x063F)

**Purpose**: Begin CTE specification.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| recursive | uint8_t | 0=false, 1=true |
| cte_count | uint16_t | Number of CTEs |

#### SBLR3_CTE_DEF (0x0607)

**Purpose**: Define a CTE.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| name_len | uint16_t | CTE name length |
| name | char[] | CTE identifier |
| column_count | uint16_t | Explicit column count (0=infer) |
| columns | char[][] | Column names |
| query_len | uint32_t | Query bytecode length |
| query | bytes | SELECT statement bytecode |
| search_present | uint8_t | For recursive CTE |
| cycle_present | uint8_t | For recursive CTE |

#### SBLR3_CTE_SCAN (0x0609)

**Purpose**: Reference a CTE in FROM clause.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| cte_name_len | uint16_t | CTE name length |
| cte_name | char[] | CTE identifier |

**Recursive CTE Execution:**

```cpp
// Source: src/sblr/executor.cpp:753
void executeRecursiveCTE(const std::string& cte_name, size_t base_pc);
```

```
For recursive CTE (UNION/UNION ALL):

1. Initialize result set empty
2. Execute anchor member (non-recursive)
   - Add results to working table
   - Add to result set
3. While working table not empty:
   a. Execute recursive member
      - Reference working table
   b. Add new rows to result set
   c. Replace working table with new rows
   d. Detect cycles if CYCLE specified
4. Return complete result set
```

---

### Subquery Opcodes

#### SBLR3_SUBQUERY_SCALAR (0x0639)

**Purpose**: Scalar subquery (returns single value).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| query_len | uint32_t | Subquery bytecode length |
| query | bytes | SELECT statement bytecode |

**Execution:**
```
1. Execute subquery
2. If 0 rows: return NULL
3. If 1 row: return value
4. If >1 rows: ERROR (scalar subquery violation)
```

#### SBLR3_SUBQUERY_EXISTS (0x0633)

**Purpose**: EXISTS predicate.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| query_len | uint32_t | Subquery bytecode length |
| query | bytes | SELECT statement bytecode |

**Execution:**
```
1. Start subquery execution
2. If any row returned: return TRUE
3. If no rows: return FALSE
4. (Can optimize: stop at first row)
```

#### SBLR3_SUBQUERY_IN (0x0635)

**Purpose**: IN (subquery) predicate.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| left_expr_len | uint32_t | Left expression bytecode length |
| left_expr | bytes | Expression bytecode |
| query_len | uint32_t | Subquery bytecode length |
| query | bytes | SELECT statement bytecode |

**Execution:**
```
1. Evaluate left expression -> value
2. Execute subquery, collect all values
3. If value found in results: return TRUE
4. If value not found: return FALSE
5. If subquery has NULL and no match: return NULL
```

#### SBLR3_SUBQUERY_NOT_IN (0x0637)

**Purpose**: NOT IN (subquery) predicate.

**Execution:** Negation of IN semantics.

#### SBLR3_SUBQUERY_ARRAY (0x062F)

**Purpose**: Array constructor from subquery.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| query_len | uint32_t | Subquery bytecode length |
| query | bytes | SELECT statement bytecode |

**Execution:**
```
1. Execute subquery
2. Collect all row values
3. Build array from values
4. Return array
```

#### SBLR3_SUBQUERY_END (0x0631)

**Purpose**: Mark end of subquery.

**Payload:** None

---

### IN-List Opcode

#### SBLR3_IN_LIST (0x061D)

**Purpose**: IN (list) predicate.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| left_expr_len | uint32_t | Left expression bytecode length |
| left_expr | bytes | Expression bytecode |
| value_count | uint16_t | Number of values |
| values | bytes[] | Value expressions bytecode |

---

### Window Clause Opcodes

#### SBLR3_WINDOW (0x065E)

**Purpose**: Window function specification.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| window_name_len | uint16_t | Window name (for named windows) |
| window_name | char[] | Window identifier |
| spec_present | uint8_t | 0=reference only, 1=specification |
| spec | WindowSpec | Window specification |

**WindowSpec Structure:**
| Field | Type | Description |
|-------|------|-------------|
| partition_by_present | uint8_t | 0=no, 1=yes |
| partition_by_data | bytes | PARTITION BY expressions |
| order_by_present | uint8_t | 0=no, 1=yes |
| order_by_data | bytes | ORDER BY expressions |
| frame_present | uint8_t | 0=no, 1=yes |
| frame_mode | uint8_t | 1=ROWS, 2=RANGE, 3=GROUPS |
| frame_start_type | uint8_t | 1=UNBOUNDED PRECEDING, 2=expr PRECEDING, 3=CURRENT ROW, 4=expr FOLLOWING |
| frame_start_expr | bytes | Expression (if needed) |
| frame_end_type | uint8_t | 1=expr PRECEDING, 2=CURRENT ROW, 3=expr FOLLOWING, 4=UNBOUNDED FOLLOWING |
| frame_end_expr | bytes | Expression (if needed) |
| frame_exclusion | uint8_t | 0=none, 1=CURRENT ROW, 2=GROUP, 3=TIES, 4=NO OTHERS |

#### SBLR3_PARTITION_BY (0x0656)

**Purpose**: PARTITION BY clause.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| expr_count | uint16_t | Number of partition expressions |
| expressions | bytes[] | Expression bytecode array |

#### SBLR3_WINDOW_ORDER_BY (0x065F)

**Purpose**: ORDER BY within window specification.

**Payload:** Same as ORDER_BY

#### SBLR3_WINDOW_SPEC (0x0660)

**Purpose**: Inline window specification.

**Payload:** Same as WindowSpec above

### Frame Clause Opcodes

| Opcode | Hex | Description |
|--------|-----|-------------|
| SBLR3_FRAME_CLAUSE | 0x0640 | Frame specification marker |
| SBLR3_FRAME_ROWS | 0x0646 | ROWS frame mode |
| SBLR3_FRAME_RANGE | 0x0645 | RANGE frame mode |
| SBLR3_FRAME_GROUPS | 0x0643 | GROUPS frame mode |
| SBLR3_FRAME_PRECEDING | 0x0644 | expr PRECEDING |
| SBLR3_FRAME_FOLLOWING | 0x0642 | expr FOLLOWING |
| SBLR3_FRAME_CURRENT_ROW | 0x0641 | CURRENT ROW |
| SBLR3_FRAME_UNBOUNDED_PRECEDING | 0x0648 | UNBOUNDED PRECEDING |
| SBLR3_FRAME_UNBOUNDED_FOLLOWING | 0x0647 | UNBOUNDED FOLLOWING |

---

### Column Reference Opcodes

#### SBLR3_COLUMN_REF (0x0604)

**Purpose**: Reference a column value.

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| table_ref_len | uint16_t | Table/alias name length (0 for unqualified) |
| table_ref | char[] | Table reference name |
| column_name_len | uint16_t | Column name length |
| column_name | char[] | Column identifier |
| column_id | uint16_t | Column ordinal (if resolved) |

#### SBLR3_INSERTED_COLUMN_REF (0x0617)

**Purpose**: Reference EXCLUDED/INSERTED pseudo-table (UPSERT).

**Payload Schema:**
| Field | Type | Description |
|-------|------|-------------|
| column_name_len | uint16_t | Column name length |
| column_name | char[] | Column identifier |

### Invariants

1. **Column Count Match**: Set operations require identical column counts
   - Verification: Parser and runtime validation

2. **Type Compatibility**: Combined columns must be compatible types
   - Verification: Type coercion rules

3. **Scope Isolation**: Subqueries cannot reference outer scope inappropriately
   - Verification: Scope analysis during compilation

4. **CTE Visibility**: CTEs are only visible in the query they define
   - Verification: Name resolution rules

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `E_CARDINALITY_VIOLATION` | Scalar subquery returns multiple rows | Use LIMIT 1 or different query |
| `E_SET_OPERATION_MISMATCH` | Set operation column count/type mismatch | Ensure queries return compatible columns |
| `E_UNDEFINED_COLUMN` | Column reference not found | Verify column name and table alias |
| `E_AMBIGUOUS_COLUMN` | Column reference matches multiple tables | Qualify with table alias |
| `E_WINDOW_FUNCTION_MISUSE` | Window function in wrong context | Move to SELECT list or ORDER BY |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_sblr_jit_functions.cpp` | Query execution paths |
| `tests/unit/test_sblr_v3_container.cpp` | Query encoding |

## Related Specifications

- [opcodes_expressions.md](./opcodes_expressions.md) - Expression and function evaluation
- [opcodes_dml.md](./opcodes_dml.md) - Data modification operations

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
