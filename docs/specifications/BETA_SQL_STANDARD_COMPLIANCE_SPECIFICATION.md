# ScratchBird Beta: SQL Standard Compliance Implementation Specification

**Document Version:** 1.0
**Date:** 2026-02-04
**Status:** Beta Implementation Specification
**Owner:** Core Engineering Team
**Related Documents:**
- `/ScratchBird-Analysis-Reports/16-SQL-STANDARD-COMPLIANCE-GAP-ANALYSIS.md`
- `/docs/specifications/reference/firebird/firebird_docs_split/10_Window_Functions.md`

---

## Document Purpose

This specification defines the **complete implementation** of all missing SQL standard features identified in the gap analysis. This includes CRITICAL (must-have), HIGH (should-have), MEDIUM (nice-to-have), and LOW priority (future consideration) features.

**All features in this specification are targeted for Beta implementation**, though they may be delivered in multiple Beta releases (Beta 1.0, 1.1, 1.2).

---

## Table of Contents

1. [GROUPING SETS / CUBE / ROLLUP](#1-grouping-sets--cube--rollup)
2. [Full-Text Search SQL Syntax](#2-full-text-search-sql-syntax)
3. [DEFERRABLE Constraints](#3-deferrable-constraints)
4. [MATCH_RECOGNIZE Pattern Matching](#4-match_recognize-pattern-matching)
5. [EXCLUSION Constraints](#5-exclusion-constraints)
6. [CHECK Constraints with Subqueries](#6-check-constraints-with-subqueries)
7. [Polymorphic Table Functions](#7-polymorphic-table-functions)
8. [Testing Requirements](#8-testing-requirements)
9. [Documentation Requirements](#9-documentation-requirements)
10. [Migration and Compatibility](#10-migration-and-compatibility)

---

## 1. GROUPING SETS / CUBE / ROLLUP

**Priority:** 🔴 CRITICAL (Beta 1.0)
**SQL Standard:** SQL:1999
**Implementation Effort:** 3-4 weeks
**Assignee:** Core Query Engine Team

### 1.1 Overview

Implement SQL:1999 advanced grouping features for OLAP/business intelligence workloads. These features allow efficient generation of multiple grouping levels in a single query.

### 1.2 SQL Syntax

#### 1.2.1 GROUPING SETS

```sql
SELECT column1, column2, aggregate_function(column3)
FROM table_name
GROUP BY GROUPING SETS (
    (column1, column2),
    (column1),
    ()
);
```

**Semantics:**
- Generates one result set per grouping set
- Equivalent to multiple GROUP BY queries with UNION ALL
- NULLs used as placeholder for non-grouped columns

#### 1.2.2 ROLLUP

```sql
SELECT year, quarter, month, SUM(sales)
FROM sales_data
GROUP BY ROLLUP (year, quarter, month);

-- Equivalent to:
GROUP BY GROUPING SETS (
    (year, quarter, month),
    (year, quarter),
    (year),
    ()
)
```

**Semantics:**
- Generates hierarchical subtotals from right to left
- N columns → N+1 grouping levels
- Common for hierarchical dimensions (geography, time)

#### 1.2.3 CUBE

```sql
SELECT region, product, customer_type, SUM(sales)
FROM sales
GROUP BY CUBE (region, product, customer_type);

-- Generates 2^3 = 8 grouping sets
```

**Semantics:**
- Generates all possible combinations of grouping columns
- N columns → 2^N grouping levels
- Used for multi-dimensional analysis

#### 1.2.4 Mixed Syntax

```sql
-- Combine GROUPING SETS, ROLLUP, CUBE
SELECT year, region, product, SUM(sales)
FROM sales
GROUP BY year, ROLLUP(region, product);

-- Combine multiple grouping operations
GROUP BY GROUPING SETS ((a, b), (a)), CUBE(c, d);
```

#### 1.2.5 GROUPING() Function

```sql
SELECT
    department,
    job_title,
    COUNT(*) AS employee_count,
    GROUPING(department) AS is_dept_total,
    GROUPING(job_title) AS is_job_total
FROM employees
GROUP BY ROLLUP (department, job_title);
```

**Returns:**
- `1` if column is aggregated (NULL is from grouping)
- `0` if column value is from detail row

**Use Cases:**
- Distinguish NULL from data vs. NULL from aggregation
- Apply different formatting to subtotal rows

#### 1.2.6 GROUPING_ID() Function

```sql
SELECT
    region,
    product,
    customer_type,
    SUM(sales),
    GROUPING_ID(region, product, customer_type) AS grouping_level
FROM sales
GROUP BY CUBE (region, product, customer_type);
```

**Returns:**
- Bitmask representing which columns are aggregated
- Binary: 000 = detail, 001 = region aggregated, 010 = product aggregated, etc.
- Integer: 0-7 for 3 columns

**Use Cases:**
- Filter specific aggregation levels
- Sort by aggregation hierarchy
- Conditional formatting in reports

### 1.3 Parser Implementation

**Files to Modify:**
- `/src/parser/sql_parser.y` (Bison grammar)
- `/src/parser/sql_lexer.l` (Flex lexer)
- `/src/parser/parse_nodes.h` (AST node definitions)

#### 1.3.1 New Keywords

Add to lexer (`sql_lexer.l`):
```c
"GROUPING"      { return KW_GROUPING; }
"SETS"          { return KW_SETS; }
"ROLLUP"        { return KW_ROLLUP; }
"CUBE"          { return KW_CUBE; }
"GROUPING_ID"   { return KW_GROUPING_ID; }
```

#### 1.3.2 Grammar Rules

Add to parser (`sql_parser.y`):
```yacc
group_by_clause:
    GROUP BY grouping_element_list
    ;

grouping_element_list:
    grouping_element
    | grouping_element_list ',' grouping_element
    ;

grouping_element:
    ordinary_grouping_set
    | rollup_list
    | cube_list
    | grouping_sets_specification
    | empty_grouping_set
    ;

ordinary_grouping_set:
    expr_list
    | '(' expr_list ')'
    ;

rollup_list:
    ROLLUP '(' expr_list ')'
    ;

cube_list:
    CUBE '(' expr_list ')'
    ;

grouping_sets_specification:
    GROUPING SETS '(' grouping_element_list ')'
    ;

empty_grouping_set:
    '(' ')'
    ;
```

#### 1.3.3 AST Node Definitions

Add to `parse_nodes.h`:
```cpp
enum GroupingType {
    GROUPING_TYPE_ORDINARY,
    GROUPING_TYPE_ROLLUP,
    GROUPING_TYPE_CUBE,
    GROUPING_TYPE_GROUPING_SETS,
    GROUPING_TYPE_EMPTY
};

struct GroupingElement {
    GroupingType type;
    std::vector<Expression*> columns;
    std::vector<GroupingElement*> nested_elements;  // For GROUPING SETS
};

struct GroupByClause {
    std::vector<GroupingElement*> elements;
};
```

### 1.4 Query Planning

**Files to Create/Modify:**
- `/src/query_planner/grouping_expansion.cpp` (NEW)
- `/src/query_planner/query_planner.cpp` (MODIFY)

#### 1.4.1 Expansion Algorithm

**Step 1: Expand ROLLUP to GROUPING SETS**
```
ROLLUP(a, b, c) →
GROUPING SETS (
    (a, b, c),
    (a, b),
    (a),
    ()
)
```

**Step 2: Expand CUBE to GROUPING SETS**
```
CUBE(a, b, c) →
GROUPING SETS (
    (a, b, c), (a, b), (a, c), (a),
    (b, c), (b), (c), ()
)
```

**Step 3: Flatten nested GROUPING SETS**
```
GROUPING SETS (
    (a, b),
    GROUPING SETS ((c), (d))
) →
GROUPING SETS (
    (a, b),
    (c),
    (d)
)
```

#### 1.4.2 Normalization

Normalize grouping sets to canonical form:
1. Remove duplicate grouping sets
2. Sort columns within each grouping set
3. Sort grouping sets by specificity (most specific first)

Example:
```
Input:  GROUPING SETS ((b, a), (), (a, b), (a))
Output: GROUPING SETS ((a, b), (a), ())
```

#### 1.4.3 Optimization Strategies

**Strategy 1: Shared Aggregation**

For query:
```sql
SELECT region, product, SUM(sales)
FROM sales
GROUP BY GROUPING SETS ((region, product), (region));
```

Generate plan:
```
1. Sort by (region, product)
2. Aggregate at (region, product) level
3. For each region boundary:
   - Emit detail row: (region, product, sum)
   - Accumulate region total
4. At region change:
   - Emit subtotal row: (region, NULL, sum)
```

**Benefits:**
- Single scan of data
- Share aggregation work between levels
- Minimize memory usage

**Strategy 2: Materialized Intermediate Results**

For CUBE with many columns:
```sql
GROUP BY CUBE (a, b, c, d, e)  -- 2^5 = 32 grouping sets
```

Generate plan:
```
1. Compute most detailed level: (a,b,c,d,e)
2. Materialize result in temp table
3. Re-aggregate from temp table for coarser levels
```

**Benefits:**
- Avoid re-scanning base table
- Better for large data volumes
- Predictable memory usage

**Strategy 3: Parallel Execution**

For independent grouping sets:
```sql
GROUP BY GROUPING SETS ((a), (b), (c))
```

Generate plan:
```
1. Partition data into N threads
2. Each thread computes all grouping sets
3. Merge results
```

### 1.5 Execution Engine

**Files to Create/Modify:**
- `/src/executor/agg_executor.cpp` (MODIFY)
- `/src/executor/grouping_executor.cpp` (NEW)

#### 1.5.1 Data Structures

```cpp
// Grouping set representation
struct GroupingSet {
    std::vector<int> column_indices;  // Indices into SELECT list
    uint64_t grouping_id;              // Bitmask for GROUPING_ID()

    bool is_aggregated(int column_index) const {
        return std::find(column_indices.begin(),
                        column_indices.end(),
                        column_index) == column_indices.end();
    }
};

// Aggregation state for multiple grouping sets
class MultiGroupAggState {
public:
    void add_grouping_set(const GroupingSet& gs);
    void accumulate(const Tuple& input_row);
    void emit_results(TupleStream& output);

private:
    std::vector<GroupingSet> grouping_sets_;
    std::map<Tuple, AggState*> agg_states_;  // Key = grouping columns
};
```

#### 1.5.2 Execution Algorithm

**Single-Pass Algorithm (for shared aggregation):**

```cpp
void execute_multi_group_aggregation() {
    // Phase 1: Accumulate
    for (auto& input_row : input_stream) {
        for (auto& grouping_set : grouping_sets) {
            // Extract grouping key
            Tuple key = extract_key(input_row, grouping_set);

            // Get or create aggregation state
            AggState* state = get_agg_state(key, grouping_set);

            // Accumulate
            state->accumulate(input_row);
        }
    }

    // Phase 2: Emit
    for (auto& [key, state] : agg_states) {
        Tuple result = create_result_tuple(key, state);
        emit(result);
    }
}
```

**Two-Pass Algorithm (for materialized intermediates):**

```cpp
void execute_two_pass_aggregation() {
    // Phase 1: Compute most detailed grouping set
    auto detailed_results = compute_detailed_grouping();

    // Materialize
    TempTable* temp = materialize(detailed_results);

    // Phase 2: Re-aggregate for coarser levels
    for (auto& grouping_set : coarser_grouping_sets) {
        auto results = re_aggregate(temp, grouping_set);
        emit(results);
    }
}
```

#### 1.5.3 NULL Marker Generation

For aggregated columns, emit SQL NULL:
```cpp
Tuple create_result_tuple(const Tuple& key, AggState* state,
                          const GroupingSet& gs) {
    Tuple result;

    for (int i = 0; i < num_group_columns; i++) {
        if (gs.is_aggregated(i)) {
            result[i] = TypedValue::Null();  // Aggregated column
        } else {
            result[i] = key[i];              // Detail column
        }
    }

    // Add aggregate values
    for (auto& agg : state->aggregates) {
        result.append(agg.finalize());
    }

    return result;
}
```

### 1.6 GROUPING() Function Implementation

**Function Signature:**
```sql
GROUPING(column_expression) → INTEGER
```

**Implementation:**

```cpp
class GroupingFunction : public ScalarFunction {
public:
    TypedValue evaluate(const EvalContext& ctx) override {
        int column_index = get_column_index(arg_);
        GroupingSet* current_gs = ctx.get_current_grouping_set();

        if (current_gs->is_aggregated(column_index)) {
            return TypedValue::Integer(1);
        } else {
            return TypedValue::Integer(0);
        }
    }

private:
    Expression* arg_;
};
```

**Catalog Entry:**
```sql
INSERT INTO RDB$FUNCTIONS (
    RDB$FUNCTION_NAME,
    RDB$RETURN_TYPE,
    RDB$DESCRIPTION
) VALUES (
    'GROUPING',
    'INTEGER',
    'Returns 1 if argument is aggregated, 0 otherwise'
);
```

### 1.7 GROUPING_ID() Function Implementation

**Function Signature:**
```sql
GROUPING_ID(column_expression, ...) → BIGINT
```

**Implementation:**

```cpp
class GroupingIdFunction : public ScalarFunction {
public:
    TypedValue evaluate(const EvalContext& ctx) override {
        GroupingSet* current_gs = ctx.get_current_grouping_set();
        uint64_t grouping_id = 0;

        for (int i = 0; i < args_.size(); i++) {
            int column_index = get_column_index(args_[i]);

            if (current_gs->is_aggregated(column_index)) {
                grouping_id |= (1ULL << (args_.size() - 1 - i));
            }
        }

        return TypedValue::BigInt(grouping_id);
    }

private:
    std::vector<Expression*> args_;
};
```

**Example:**
```sql
-- For CUBE(a, b, c):
-- Detail row (a,b,c):     GROUPING_ID(a,b,c) = 0b000 = 0
-- (a,b) subtotal:         GROUPING_ID(a,b,c) = 0b001 = 1
-- (a,c) subtotal:         GROUPING_ID(a,b,c) = 0b010 = 2
-- (a) subtotal:           GROUPING_ID(a,b,c) = 0b011 = 3
-- Grand total ():         GROUPING_ID(a,b,c) = 0b111 = 7
```

### 1.8 Catalog Schema Changes

**No catalog changes required** - GROUPING SETS is a runtime feature only.

### 1.9 Error Handling

**Parse Errors:**
```sql
-- Error: Empty GROUPING SETS
SELECT a, SUM(b) FROM t GROUP BY GROUPING SETS ();
→ ERROR: GROUPING SETS cannot be empty

-- Error: Aggregate in GROUP BY
SELECT a, SUM(b) FROM t GROUP BY GROUPING SETS (SUM(a));
→ ERROR: Aggregate functions not allowed in GROUP BY

-- Error: Window function in GROUP BY
SELECT a, SUM(b) FROM t GROUP BY GROUPING SETS (ROW_NUMBER() OVER());
→ ERROR: Window functions not allowed in GROUP BY
```

**Execution Errors:**
```sql
-- Warning: Large CUBE
SELECT * FROM t GROUP BY CUBE (a,b,c,d,e,f,g,h,i,j);  -- 2^10 = 1024 sets
→ WARNING: CUBE with 10 columns generates 1024 grouping sets
```

### 1.10 Testing Requirements

**Test Categories:**

1. **Basic Functionality (100 tests)**
   - GROUPING SETS with 1-5 sets
   - ROLLUP with 1-10 columns
   - CUBE with 1-5 columns (2^5 = 32 sets)
   - Empty grouping set `()`
   - Mixed grouping elements

2. **GROUPING() Function (50 tests)**
   - Single column
   - Multiple columns
   - NULL from data vs. NULL from aggregation
   - Use in WHERE, HAVING, ORDER BY

3. **GROUPING_ID() Function (50 tests)**
   - 1-10 arguments
   - Filter by grouping level
   - Sort by grouping hierarchy

4. **Optimization (50 tests)**
   - Shared aggregation correctness
   - Materialized intermediate correctness
   - Performance vs. UNION ALL baseline

5. **Edge Cases (50 tests)**
   - NULL handling
   - Empty result sets
   - Single-row tables
   - Large CUBE (warning generation)

6. **Aggregate Functions (100 tests)**
   - COUNT, SUM, AVG, MIN, MAX
   - COUNT(DISTINCT)
   - STRING_AGG
   - Window functions (should fail)

**Total:** 400+ tests

**Performance Benchmarks:**
- GROUPING SETS vs. UNION ALL: **10-100× faster** (target)
- ROLLUP on 1M rows: **<5 seconds** (target)
- CUBE(5 columns) on 1M rows: **<30 seconds** (target)

### 1.11 Documentation

**Files to Create:**
- `/docs/specifications/dml/DML_SELECT_GROUPING.md`
- `/docs/user_guide/sql_reference/grouping_sets.md`

**Content Requirements:**
- Syntax diagrams
- 10+ examples covering all features
- Performance tuning guide
- Comparison with UNION ALL
- BI tool integration examples (Tableau, Power BI)

---

## 2. Full-Text Search SQL Syntax

**Priority:** 🟡 HIGH (Beta 1.1)
**SQL Standard:** Vendor-specific (no SQL standard)
**Implementation Effort:** 6-8 weeks (phased)
**Assignee:** Search and Indexing Team

### 2.1 Overview

Implement PostgreSQL-style full-text search with TSVECTOR/TSQUERY types and operators. This builds on existing InvertedIndex and FSTIndex infrastructure.

**Design Decision:** Use PostgreSQL syntax as primary, with MySQL-style `MATCH() AGAINST()` as convenience wrapper.

### 2.2 SQL Syntax

#### 2.2.1 Data Types

**TSVECTOR:**
```sql
-- Document representation: lexeme → positions[]
SELECT 'a fat cat sat on a mat and ate a fat rat'::TSVECTOR;
→ 'a':1,6,9,12 'and':8 'ate':9 'cat':3 'fat':2,11 'mat':7 'on':5 'rat':12 'sat':4
```

**TSQUERY:**
```sql
-- Query representation: boolean expression
SELECT 'fat & (cat | rat)'::TSQUERY;
→ 'fat' & ( 'cat' | 'rat' )
```

#### 2.2.2 Functions

**to_tsvector():**
```sql
to_tsvector(config regconfig, document text) → tsvector
to_tsvector(document text) → tsvector  -- Uses default config

-- Example:
SELECT to_tsvector('english', 'The quick brown fox jumps over the lazy dog');
→ 'brown':3 'dog':9 'fox':4 'jump':5 'lazi':8 'quick':2
```

**to_tsquery():**
```sql
to_tsquery(config regconfig, query text) → tsquery
to_tsquery(query text) → tsquery

-- Example:
SELECT to_tsquery('english', 'fox & dog');
→ 'fox' & 'dog'
```

**plainto_tsquery():**
```sql
plainto_tsquery(config regconfig, query text) → tsquery

-- Simpler: no operator syntax needed
SELECT plainto_tsquery('english', 'fox dog');
→ 'fox' & 'dog'
```

**ts_rank():**
```sql
ts_rank(tsvector, tsquery) → float4
ts_rank(weights float4[], tsvector, tsquery) → float4

-- Example:
SELECT ts_rank(to_tsvector('english', body), to_tsquery('database')) AS rank
FROM documents
WHERE to_tsvector('english', body) @@ to_tsquery('database')
ORDER BY rank DESC;
```

**ts_headline():**
```sql
ts_headline(config regconfig, document text, query tsquery) → text

-- Example:
SELECT ts_headline('english',
    'The quick brown fox jumps over the lazy dog',
    to_tsquery('fox & dog')
);
→ 'The quick brown <b>fox</b> jumps over the lazy <b>dog</b>'
```

#### 2.2.3 Operators

**@@ (Match):**
```sql
tsvector @@ tsquery → boolean

-- Example:
SELECT title
FROM articles
WHERE to_tsvector('english', body) @@ to_tsquery('database & performance');
```

**|| (Concatenate TSVECTOR):**
```sql
tsvector || tsvector → tsvector

-- Example:
SELECT to_tsvector('english', title) || to_tsvector('english', body) AS doc
FROM articles;
```

**&& (AND TSQUERY):**
```sql
tsquery && tsquery → tsquery

-- Example:
SELECT to_tsquery('cat') && to_tsquery('dog');
→ 'cat' & 'dog'
```

**|| (OR TSQUERY):**
```sql
tsquery || tsquery → tsquery

-- Example:
SELECT to_tsquery('cat') || to_tsquery('dog');
→ 'cat' | 'dog'
```

**!! (NOT TSQUERY):**
```sql
!! tsquery → tsquery

-- Example:
SELECT !! to_tsquery('cat');
→ !'cat'
```

#### 2.2.4 TSQUERY Syntax

**Boolean Operators:**
```sql
-- AND
'database & performance'

-- OR
'database | performance'

-- NOT
'database & !performance'

-- Parentheses
'(database | postgresql) & performance'
```

**Phrase Search (Adjacent):**
```sql
-- Words must be adjacent
'database <-> performance'
→ Matches "database performance" but not "database and performance"
```

**Proximity Search:**
```sql
-- Words within N words of each other
'database <2> performance'
→ Matches "database performance", "database and performance"
→ Does not match "database optimization and performance"
```

**Prefix Match:**
```sql
-- Match word prefixes
'data:*'
→ Matches "data", "database", "databases", "datatype"
```

**Weight Filtering:**
```sql
-- TSVECTOR can have weights A, B, C, D
'database:A & performance:B'
→ Matches only if "database" has weight A and "performance" has weight B
```

#### 2.2.5 Index Creation

```sql
-- GIN index on tsvector column
CREATE INDEX idx_documents_search ON documents USING GIN(search_vector);

-- Generated column + GIN index
ALTER TABLE documents
ADD COLUMN search_vector TSVECTOR
GENERATED ALWAYS AS (to_tsvector('english', title || ' ' || body)) STORED;

CREATE INDEX idx_documents_search ON documents USING GIN(search_vector);

-- Index on expression (PostgreSQL style)
CREATE INDEX idx_documents_search ON documents
USING GIN(to_tsvector('english', title || ' ' || body));
```

#### 2.2.6 MySQL-Style Convenience Syntax

```sql
-- MySQL compatibility: MATCH() AGAINST()
SELECT title, MATCH(title, body) AGAINST ('database performance') AS score
FROM documents
WHERE MATCH(title, body) AGAINST ('database performance')
ORDER BY score DESC;

-- Internally translated to:
SELECT title,
       ts_rank(to_tsvector('english', title || ' ' || body),
               plainto_tsquery('english', 'database performance')) AS score
FROM documents
WHERE to_tsvector('english', title || ' ' || body) @@
      plainto_tsquery('english', 'database performance')
ORDER BY score DESC;
```

### 2.3 Type System Implementation

**Files to Create:**
- `/src/types/tsvector.h` (NEW)
- `/src/types/tsvector.cpp` (NEW)
- `/src/types/tsquery.h` (NEW)
- `/src/types/tsquery.cpp` (NEW)

#### 2.3.1 TSVECTOR Format

**In-Memory Representation:**
```cpp
struct TSVectorEntry {
    std::string lexeme;             // Normalized word
    std::vector<uint16_t> positions; // Word positions in document
    char weight;                     // 'A', 'B', 'C', 'D', or '\0'
};

class TSVector {
public:
    void add_lexeme(const std::string& lexeme, uint16_t position, char weight = '\0');
    bool contains(const std::string& lexeme) const;
    const std::vector<uint16_t>& get_positions(const std::string& lexeme) const;

    // Serialization
    std::string to_string() const;
    static TSVector from_string(const std::string& str);

private:
    std::vector<TSVectorEntry> entries_;  // Sorted by lexeme
};
```

**Binary Storage Format:**
```
[Header: 4 bytes]
  - Version: 1 byte
  - Flags: 1 byte
  - NumLexemes: 2 bytes

[Lexeme Entries: Variable]
For each lexeme:
  - LexemeLength: 1 byte
  - Lexeme: N bytes (UTF-8)
  - Weight: 1 byte (0 = no weight, 1-4 = A-D)
  - NumPositions: 2 bytes
  - Positions: NumPositions * 2 bytes (uint16)
```

**String Representation (SQL output):**
```
'lexeme1':pos1,pos2 'lexeme2':pos3,pos4 'lexeme3'Weight:pos5
```

#### 2.3.2 TSQUERY Format

**In-Memory Representation:**
```cpp
enum TSQueryNodeType {
    TSQUERY_NODE_LEXEME,
    TSQUERY_NODE_AND,
    TSQUERY_NODE_OR,
    TSQUERY_NODE_NOT,
    TSQUERY_NODE_PHRASE,
    TSQUERY_NODE_PROXIMITY
};

struct TSQueryNode {
    TSQueryNodeType type;
    std::string lexeme;        // For LEXEME nodes
    TSQueryNode* left;         // For binary operators
    TSQueryNode* right;
    int distance;              // For PHRASE/PROXIMITY
    char weight;               // For weighted search
    bool prefix_match;         // For 'word:*' syntax
};

class TSQuery {
public:
    bool matches(const TSVector& doc) const;
    std::string to_string() const;
    static TSQuery from_string(const std::string& str);

private:
    TSQueryNode* root_;
};
```

**Binary Storage Format:**
```
[Header: 4 bytes]
  - Version: 1 byte
  - Flags: 1 byte
  - NumNodes: 2 bytes

[Tree Nodes: Variable]
Pre-order traversal of expression tree:
  - NodeType: 1 byte
  - For LEXEME: LexemeLength (1 byte) + Lexeme + Weight + PrefixFlag
  - For PHRASE/PROXIMITY: Distance (2 bytes)
  - For AND/OR/NOT: (no additional data)
```

### 2.4 Parser Implementation

**Files to Modify:**
- `/src/parser/sql_parser.y`
- `/src/parser/sql_lexer.l`
- `/src/parser/tsquery_parser.y` (NEW - dedicated parser for TSQUERY syntax)

#### 2.4.1 New Keywords

```c
"TSVECTOR"      { return KW_TSVECTOR; }
"TSQUERY"       { return KW_TSQUERY; }
"REGCONFIG"     { return KW_REGCONFIG; }
"@@"            { return OP_MATCH; }
```

#### 2.4.2 Type Declaration

```yacc
type_name:
    | TSVECTOR
    | TSQUERY
    | REGCONFIG
    ;
```

#### 2.4.3 TSQUERY Parser (Dedicated)

Create `/src/parser/tsquery_parser.y`:
```yacc
%{
#include "tsquery.h"
%}

%token LEXEME NUMBER
%left '|'
%left '&'
%right '!'
%left FOLLOWED_BY NEAR

%%

tsquery:
    tsquery '&' tsquery         { $$ = make_and_node($1, $3); }
    | tsquery '|' tsquery       { $$ = make_or_node($1, $3); }
    | '!' tsquery               { $$ = make_not_node($2); }
    | tsquery FOLLOWED_BY tsquery  { $$ = make_phrase_node($1, $3, 1); }
    | tsquery NEAR NUMBER tsquery  { $$ = make_proximity_node($1, $4, $3); }
    | '(' tsquery ')'           { $$ = $2; }
    | LEXEME                    { $$ = make_lexeme_node($1); }
    | LEXEME ':' '*'            { $$ = make_prefix_node($1); }
    | LEXEME ':' 'A'            { $$ = make_weighted_node($1, 'A'); }
    ;

%%
```

### 2.5 Function Implementation

**Files to Create:**
- `/src/functions/fulltext_functions.cpp` (NEW)

#### 2.5.1 to_tsvector()

```cpp
TSVector to_tsvector(const std::string& config, const std::string& document) {
    // 1. Get language configuration
    TextSearchConfig* cfg = get_config(config);

    // 2. Tokenize document
    std::vector<Token> tokens = cfg->tokenizer->tokenize(document);

    // 3. Filter stop words
    tokens = cfg->filter_stop_words(tokens);

    // 4. Stem words
    for (auto& token : tokens) {
        token.text = cfg->stemmer->stem(token.text);
    }

    // 5. Build TSVECTOR
    TSVector result;
    for (const auto& token : tokens) {
        result.add_lexeme(token.text, token.position);
    }

    return result;
}
```

#### 2.5.2 to_tsquery()

```cpp
TSQuery to_tsquery(const std::string& config, const std::string& query_text) {
    // 1. Parse query syntax
    TSQueryParser parser;
    TSQueryNode* ast = parser.parse(query_text);

    // 2. Normalize lexemes (stemming)
    TextSearchConfig* cfg = get_config(config);
    normalize_lexemes(ast, cfg);

    // 3. Build TSQUERY
    return TSQuery(ast);
}
```

#### 2.5.3 ts_rank()

**BM25 Ranking Algorithm:**
```cpp
float ts_rank(const TSVector& doc, const TSQuery& query) {
    // BM25: Best Match 25 algorithm
    // score = Σ (IDF(qi) * (f(qi,D) * (k1 + 1)) / (f(qi,D) + k1 * (1 - b + b * |D| / avgdl)))

    const float k1 = 1.2;    // Term frequency saturation
    const float b = 0.75;    // Length normalization

    float score = 0.0;

    // For each lexeme in query
    for (const auto& lexeme : query.get_lexemes()) {
        // f(qi, D): Term frequency in document
        int tf = doc.contains(lexeme) ? doc.get_positions(lexeme).size() : 0;
        if (tf == 0) continue;

        // IDF: Inverse document frequency
        float idf = calculate_idf(lexeme);  // log((N - df + 0.5) / (df + 0.5))

        // Document length normalization
        int doc_length = doc.num_lexemes();
        float avg_doc_length = get_avg_doc_length();
        float norm = 1 - b + b * (doc_length / avg_doc_length);

        // BM25 score component
        score += idf * (tf * (k1 + 1)) / (tf + k1 * norm);
    }

    return score;
}
```

#### 2.5.4 ts_headline()

```cpp
std::string ts_headline(const std::string& config, const std::string& document,
                        const TSQuery& query) {
    // 1. Find matching positions
    std::vector<int> match_positions = find_matches(document, query);

    // 2. Select best snippet (most matches in shortest span)
    Snippet best = select_best_snippet(match_positions, MAX_SNIPPET_LENGTH);

    // 3. Extract snippet
    std::string snippet = extract_snippet(document, best.start, best.end);

    // 4. Highlight matches
    snippet = highlight_matches(snippet, match_positions, "<b>", "</b>");

    return snippet;
}
```

### 2.6 Operator Implementation

**Files to Modify:**
- `/src/executor/operators.cpp`

#### 2.6.1 @@ (Match Operator)

```cpp
bool operator_match(const TSVector& doc, const TSQuery& query) {
    return query.matches(doc);
}

// TSQuery::matches() implementation
bool TSQuery::matches(const TSVector& doc) const {
    return evaluate_node(root_, doc);
}

bool TSQuery::evaluate_node(TSQueryNode* node, const TSVector& doc) const {
    switch (node->type) {
        case TSQUERY_NODE_LEXEME:
            if (node->prefix_match) {
                return doc.has_prefix(node->lexeme);
            } else {
                return doc.contains(node->lexeme);
            }

        case TSQUERY_NODE_AND:
            return evaluate_node(node->left, doc) &&
                   evaluate_node(node->right, doc);

        case TSQUERY_NODE_OR:
            return evaluate_node(node->left, doc) ||
                   evaluate_node(node->right, doc);

        case TSQUERY_NODE_NOT:
            return !evaluate_node(node->right, doc);

        case TSQUERY_NODE_PHRASE:
            return check_phrase(node, doc);

        case TSQUERY_NODE_PROXIMITY:
            return check_proximity(node, doc);
    }
}

bool TSQuery::check_phrase(TSQueryNode* node, const TSVector& doc) const {
    // Both lexemes must appear adjacently
    auto left_positions = doc.get_positions(node->left->lexeme);
    auto right_positions = doc.get_positions(node->right->lexeme);

    for (int left_pos : left_positions) {
        if (std::find(right_positions.begin(), right_positions.end(),
                      left_pos + 1) != right_positions.end()) {
            return true;  // Found adjacent occurrence
        }
    }
    return false;
}
```

### 2.7 Integration with InvertedIndex

**Files to Modify:**
- `/src/indexes/inverted_index.cpp`

#### 2.7.1 Index Structure

```cpp
class InvertedIndex {
public:
    void insert(RowId row_id, const TSVector& document);
    std::vector<RowId> search(const TSQuery& query);

private:
    // Lexeme → Posting List
    std::map<std::string, PostingList> postings_;
};

struct PostingList {
    std::vector<Posting> postings;
};

struct Posting {
    RowId row_id;
    std::vector<uint16_t> positions;  // Positions within document
    float weight;                      // Pre-computed weight (optional)
};
```

#### 2.7.2 Query Execution

```cpp
std::vector<RowId> InvertedIndex::search(const TSQuery& query) {
    // 1. Extract lexemes from query
    std::vector<std::string> lexemes = query.get_lexemes();

    // 2. Lookup posting lists
    std::vector<PostingList*> lists;
    for (const auto& lexeme : lexemes) {
        lists.push_back(&postings_[lexeme]);
    }

    // 3. Intersect posting lists (for AND queries)
    std::vector<RowId> candidates = intersect_posting_lists(lists);

    // 4. Verify matches (for phrase/proximity queries)
    std::vector<RowId> results;
    for (RowId row_id : candidates) {
        TSVector doc = fetch_document(row_id);
        if (query.matches(doc)) {
            results.push_back(row_id);
        }
    }

    return results;
}
```

### 2.8 Language Configuration

**Files to Create:**
- `/src/fulltext/text_search_config.cpp` (NEW)
- `/data/text_search/english.dict` (NEW)
- `/data/text_search/spanish.dict` (NEW)
- ... (additional languages)

#### 2.8.1 Configuration Structure

```cpp
struct TextSearchConfig {
    std::string name;               // "english", "spanish", etc.
    Tokenizer* tokenizer;           // Breaks text into words
    StopWordFilter* stop_filter;    // Removes common words
    Stemmer* stemmer;               // Normalizes words (running → run)
    Dictionary* dictionary;         // Custom word mappings
};
```

#### 2.8.2 Stemming

Use **Snowball stemming library** (libstemmer):
```cpp
class SnowballStemmer : public Stemmer {
public:
    SnowballStemmer(const std::string& language) {
        stemmer_ = sb_stemmer_new(language.c_str(), "UTF_8");
    }

    std::string stem(const std::string& word) override {
        const sb_symbol* result = sb_stemmer_stem(stemmer_,
                                                  (const sb_symbol*)word.c_str(),
                                                  word.length());
        return std::string((const char*)result, sb_stemmer_length(stemmer_));
    }

private:
    struct sb_stemmer* stemmer_;
};
```

**Supported Languages (Phase 1):**
- English
- Spanish
- French
- German
- Italian
- Portuguese

#### 2.8.3 Stop Words

Load from configuration files:
```
# english_stopwords.txt
a
an
and
are
as
at
be
but
by
for
if
in
into
is
it
...
```

```cpp
class StopWordFilter {
public:
    StopWordFilter(const std::string& filename) {
        load_stop_words(filename);
    }

    bool is_stop_word(const std::string& word) const {
        return stop_words_.count(word) > 0;
    }

private:
    std::unordered_set<std::string> stop_words_;
};
```

### 2.9 Catalog Schema

**New Tables:**

```sql
-- Text search configurations
CREATE TABLE RDB$TEXT_SEARCH_CONFIGS (
    RDB$CONFIG_NAME VARCHAR(63) PRIMARY KEY,
    RDB$PARSER_NAME VARCHAR(63) NOT NULL,
    RDB$DICTIONARY_NAME VARCHAR(63),
    RDB$STOPWORDS_FILE VARCHAR(255),
    RDB$DESCRIPTION VARCHAR(255)
);

-- Default configuration
INSERT INTO RDB$TEXT_SEARCH_CONFIGS VALUES
('english', 'default', 'english_stem', 'english_stopwords.txt', 'English full-text search'),
('spanish', 'default', 'spanish_stem', 'spanish_stopwords.txt', 'Spanish full-text search');

-- Text search dictionaries
CREATE TABLE RDB$TEXT_SEARCH_DICTS (
    RDB$DICT_NAME VARCHAR(63) PRIMARY KEY,
    RDB$DICT_TYPE VARCHAR(31) NOT NULL,  -- 'stemmer', 'synonym', 'thesaurus'
    RDB$DICT_LANGUAGE VARCHAR(31),
    RDB$DICT_FILE VARCHAR(255),
    RDB$DESCRIPTION VARCHAR(255)
);
```

### 2.10 Testing Requirements

**Test Categories:**

1. **Type System (100 tests)**
   - TSVECTOR creation and serialization
   - TSQUERY parsing and execution
   - NULL handling

2. **Functions (150 tests)**
   - to_tsvector() with various languages
   - to_tsquery() with all operators
   - plainto_tsquery()
   - ts_rank() correctness and consistency
   - ts_headline() snippet generation

3. **Operators (100 tests)**
   - @@ match operator
   - ||, &&, !! for TSQUERY
   - Precedence and associativity

4. **Query Features (200 tests)**
   - Boolean queries (AND, OR, NOT)
   - Phrase queries (<->)
   - Proximity queries (<N>)
   - Prefix matching (:*)
   - Weighted search
   - Complex nested queries

5. **Stemming (100 tests)**
   - English stemming correctness
   - Multi-language support
   - Edge cases (short words, numbers, punctuation)

6. **Performance (50 tests)**
   - Index scan vs. sequential scan
   - Large document handling (1MB+)
   - Large corpus (1M+ documents)
   - Ranking performance

**Total:** 700+ tests

**Performance Benchmarks:**
- Search 1M documents: **<100ms** (target with index)
- Rank 10K results: **<1 second** (target)
- Index build (1M documents): **<5 minutes** (target)

### 2.11 Documentation

**Files to Create:**
- `/docs/specifications/dml/DML_FULLTEXT_SEARCH.md`
- `/docs/user_guide/fulltext_search.md`

**Content:**
- Complete syntax reference
- 20+ examples
- Language configuration guide
- Performance tuning guide
- Index maintenance guide

---

## 3. DEFERRABLE Constraints

**Priority:** 🟡 MEDIUM (Beta 1.1)
**SQL Standard:** SQL:1992
**Implementation Effort:** 2-3 weeks
**Assignee:** Constraint System Team

### 3.1 Overview

Implement DEFERRABLE constraints that can be checked at COMMIT rather than after each statement. Essential for ETL pipelines and complex multi-table transactions.

### 3.2 SQL Syntax

```sql
-- Create table with deferrable constraint
CREATE TABLE orders (
    order_id INT PRIMARY KEY DEFERRABLE INITIALLY IMMEDIATE,
    customer_id INT REFERENCES customers(id) DEFERRABLE INITIALLY DEFERRED,
    amount DECIMAL(10,2) CHECK (amount > 0) NOT DEFERRABLE  -- Default
);

-- Alter constraint deferability
ALTER TABLE orders
ALTER CONSTRAINT orders_customer_id_fkey
DEFERRABLE INITIALLY DEFERRED;

-- Control constraint checking within transaction
BEGIN;

-- Defer all deferrable constraints
SET CONSTRAINTS ALL DEFERRED;

-- Or defer specific constraints
SET CONSTRAINTS orders_customer_id_fkey DEFERRED;

-- Check constraints immediately
SET CONSTRAINTS orders_customer_id_fkey IMMEDIATE;

-- Insert data that temporarily violates FK
INSERT INTO orders (order_id, customer_id) VALUES (1, 999);  -- customer 999 doesn't exist yet
INSERT INTO customers (id, name) VALUES (999, 'New Customer');  -- Now it exists

COMMIT;  -- All deferred constraints checked here
```

### 3.3 Constraint Types Affected

**Deferrable:**
- PRIMARY KEY constraints
- UNIQUE constraints
- FOREIGN KEY constraints
- CHECK constraints (optional - some DBs don't support)

**NOT Deferrable:**
- NOT NULL constraints (always immediate)
- Column-level constraints on system columns

### 3.4 Catalog Schema Changes

**Modify RDB$RELATION_CONSTRAINTS:**
```sql
ALTER TABLE RDB$RELATION_CONSTRAINTS
ADD COLUMN RDB$CONSTRAINT_DEFERRABLE CHAR(1) DEFAULT 'N',  -- 'Y' or 'N'
ADD COLUMN RDB$INITIALLY_DEFERRED CHAR(1) DEFAULT 'N';     -- 'Y' or 'N'
```

**Modify RDB$REF_CONSTRAINTS:**
```sql
ALTER TABLE RDB$REF_CONSTRAINTS
ADD COLUMN RDB$CONSTRAINT_DEFERRABLE CHAR(1) DEFAULT 'N',
ADD COLUMN RDB$INITIALLY_DEFERRED CHAR(1) DEFAULT 'N';
```

**Modify RDB$CHECK_CONSTRAINTS:**
```sql
ALTER TABLE RDB$CHECK_CONSTRAINTS
ADD COLUMN RDB$CONSTRAINT_DEFERRABLE CHAR(1) DEFAULT 'N',
ADD COLUMN RDB$INITIALLY_DEFERRED CHAR(1) DEFAULT 'N';
```

### 3.5 Parser Implementation

**Add Keywords:**
```c
"DEFERRABLE"         { return KW_DEFERRABLE; }
"NOT DEFERRABLE"     { return KW_NOT_DEFERRABLE; }
"INITIALLY"          { return KW_INITIALLY; }
"IMMEDIATE"          { return KW_IMMEDIATE; }
"DEFERRED"           { return KW_DEFERRED; }
```

**Grammar:**
```yacc
constraint_attributes:
    | DEFERRABLE
    | NOT DEFERRABLE
    | INITIALLY IMMEDIATE
    | INITIALLY DEFERRED
    | DEFERRABLE INITIALLY IMMEDIATE
    | DEFERRABLE INITIALLY DEFERRED
    ;

set_constraints_stmt:
    SET CONSTRAINTS constraint_name_list deferred_mode
    | SET CONSTRAINTS ALL deferred_mode
    ;

deferred_mode:
    IMMEDIATE | DEFERRED
    ;

constraint_name_list:
    constraint_name
    | constraint_name_list ',' constraint_name
    ;
```

### 3.6 Constraint Enforcement

**Files to Modify:**
- `/src/constraints/constraint_manager.cpp`
- `/src/transaction/transaction_manager.cpp`

#### 3.6.1 Transaction State

```cpp
struct ConstraintViolation {
    std::string constraint_name;
    std::string table_name;
    RowId row_id;
    std::string error_message;
};

class TransactionConstraintState {
public:
    // Set constraint mode
    void set_deferred(const std::string& constraint_name);
    void set_immediate(const std::string& constraint_name);
    void set_all_deferred();
    void set_all_immediate();

    // Check if constraint is deferred
    bool is_deferred(const std::string& constraint_name) const;

    // Record deferred violation
    void record_violation(const ConstraintViolation& violation);

    // Validate all deferred constraints at COMMIT
    void validate_all();

private:
    std::set<std::string> deferred_constraints_;
    std::vector<ConstraintViolation> pending_violations_;
};
```

#### 3.6.2 Constraint Checking Logic

```cpp
void check_constraint(Constraint* constraint, Row* row, TransactionState* txn) {
    if (txn->is_deferred(constraint->name)) {
        // Defer validation
        if (!validate_constraint(constraint, row)) {
            ConstraintViolation violation = {
                .constraint_name = constraint->name,
                .table_name = row->table_name,
                .row_id = row->row_id,
                .error_message = format_error(constraint, row)
            };
            txn->record_violation(violation);
        }
    } else {
        // Immediate validation
        if (!validate_constraint(constraint, row)) {
            throw ConstraintViolationError(constraint, row);
        }
    }
}
```

#### 3.6.3 COMMIT-Time Validation

```cpp
void Transaction::commit() {
    // Validate all deferred constraints
    try {
        constraint_state_->validate_all();
    } catch (ConstraintViolationError& e) {
        // Rollback transaction
        rollback();
        throw;
    }

    // Commit transaction
    commit_changes();
}

void TransactionConstraintState::validate_all() {
    for (const auto& violation : pending_violations_) {
        // Re-validate constraint (data may have changed since recording)
        Row* row = fetch_row(violation.table_name, violation.row_id);
        Constraint* constraint = get_constraint(violation.constraint_name);

        if (!validate_constraint(constraint, row)) {
            throw ConstraintViolationError(violation.error_message);
        }
    }

    pending_violations_.clear();
}
```

### 3.7 Foreign Key Handling

**Special Considerations:**

1. **Circular Dependencies:**
   ```sql
   CREATE TABLE employees (
       emp_id INT PRIMARY KEY DEFERRABLE,
       manager_id INT REFERENCES employees(emp_id) DEFERRABLE INITIALLY DEFERRED
   );

   BEGIN;
   SET CONSTRAINTS ALL DEFERRED;
   INSERT INTO employees (emp_id, manager_id) VALUES (1, 2);  -- Manager doesn't exist yet
   INSERT INTO employees (emp_id, manager_id) VALUES (2, 1);  -- Circular reference
   COMMIT;  -- Both rows validated together
   ```

2. **CASCADE Actions:**
   - ON DELETE CASCADE: Execute immediately (even if deferred)
   - ON UPDATE CASCADE: Execute immediately
   - Rationale: Cascaded changes affect other rows, must happen immediately

### 3.8 SET CONSTRAINTS Implementation

```cpp
void execute_set_constraints(SetConstraintsStmt* stmt, TransactionState* txn) {
    if (stmt->all_constraints) {
        if (stmt->deferred_mode == DEFERRED) {
            txn->set_all_deferred();
        } else {
            txn->set_all_immediate();
        }
    } else {
        for (const auto& constraint_name : stmt->constraint_names) {
            if (stmt->deferred_mode == DEFERRED) {
                txn->set_deferred(constraint_name);
            } else {
                txn->set_immediate(constraint_name);
                // Validate immediately
                validate_constraint_now(constraint_name, txn);
            }
        }
    }
}
```

### 3.9 Locking Considerations

**Problem:** Deferred constraints may need to lock referenced rows to prevent TOCTOU (time-of-check-to-time-of-use) issues.

**Example:**
```sql
-- Transaction 1:
BEGIN;
SET CONSTRAINTS ALL DEFERRED;
INSERT INTO orders (customer_id) VALUES (123);  -- Deferred FK check

-- Transaction 2 (concurrent):
DELETE FROM customers WHERE id = 123;  -- Customer deleted
COMMIT;

-- Transaction 1:
COMMIT;  -- FK validation fails! Customer no longer exists
```

**Solution:** Acquire shared locks on referenced rows:
```cpp
void record_deferred_fk_violation(ForeignKey* fk, Row* row) {
    // Acquire shared lock on referenced row
    Row* referenced_row = fetch_row(fk->referenced_table, row->fk_value);
    acquire_shared_lock(referenced_row);  // Prevent deletion until commit

    // Record violation
    record_violation(...);
}
```

### 3.10 Error Messages

```sql
-- Immediate violation
INSERT INTO orders (customer_id) VALUES (999);
→ ERROR: foreign key violation: customer 999 does not exist
  DETAIL: Key (customer_id)=(999) is not present in table "customers"

-- Deferred violation at COMMIT
BEGIN;
SET CONSTRAINTS ALL DEFERRED;
INSERT INTO orders (customer_id) VALUES (999);
COMMIT;
→ ERROR: foreign key violation at COMMIT: customer 999 does not exist
  DETAIL: Key (customer_id)=(999) is not present in table "customers"
  HINT: Transaction rolled back due to deferred constraint violation
```

### 3.11 Testing Requirements

**Test Categories:**

1. **Basic Functionality (50 tests)**
   - DEFERRABLE / NOT DEFERRABLE parsing
   - INITIALLY IMMEDIATE / INITIALLY DEFERRED
   - SET CONSTRAINTS ALL DEFERRED
   - SET CONSTRAINTS specific_constraint IMMEDIATE

2. **Foreign Keys (100 tests)**
   - Simple FK deferral
   - Circular dependencies
   - Multi-table dependencies
   - CASCADE actions with deferred constraints

3. **CHECK Constraints (50 tests)**
   - Simple CHECK deferral
   - CHECK with subqueries (if supported)

4. **Unique/Primary Key (50 tests)**
   - Temporary duplicates allowed
   - Validation at COMMIT

5. **Transaction Interactions (50 tests)**
   - ROLLBACK clears deferred violations
   - SAVEPOINT and ROLLBACK TO SAVEPOINT
   - Nested transactions

6. **Concurrency (50 tests)**
   - Lock acquisition for deferred FK
   - TOCTOU prevention
   - Deadlock scenarios

**Total:** 350+ tests

### 3.12 Documentation

**Files to Create:**
- `/docs/specifications/ddl/DDL_DEFERRABLE_CONSTRAINTS.md`

**Content:**
- Complete syntax
- 10+ examples
- ETL pipeline guide
- Performance implications
- Comparison with triggers

---

## 4. MATCH_RECOGNIZE Pattern Matching

**Priority:** 🟡 MEDIUM (Beta 1.2)
**SQL Standard:** SQL:2016
**Implementation Effort:** 8-12 weeks (simplified: 4-6 weeks)
**Assignee:** Advanced Analytics Team

### 4.1 Overview

Implement SQL:2016 MATCH_RECOGNIZE for pattern matching on ordered data. Start with **simplified subset** covering 80% of use cases.

### 4.2 SQL Syntax (Simplified Subset)

```sql
SELECT *
FROM table_name
MATCH_RECOGNIZE (
    PARTITION BY column_list
    ORDER BY column_list
    MEASURES
        measure_definition AS measure_name, ...
    ONE ROW PER MATCH | ALL ROWS PER MATCH
    AFTER MATCH SKIP TO NEXT ROW
    PATTERN (pattern_expression)
    DEFINE
        variable_name AS condition, ...
)
```

**Phase 1 Scope (Simplified):**
- ✅ Basic patterns: `A B C` (sequence), `A+` (one or more), `A*` (zero or more), `A?` (zero or one)
- ✅ Alternation: `A | B`
- ✅ Grouping: `(A B)+`
- ❌ Complex quantifiers: `{n}`, `{n,m}`, `{n,}`
- ❌ Lookahead/lookbehind
- ❌ Reluctant quantifiers

### 4.3 Examples

#### 4.3.1 Stock Pattern: Head and Shoulders

```sql
-- Detect head-and-shoulders pattern in stock prices
SELECT *
FROM stock_prices
MATCH_RECOGNIZE (
    PARTITION BY ticker
    ORDER BY trade_date
    MEASURES
        FIRST(S1.price) AS left_shoulder_price,
        FIRST(H.price) AS head_price,
        FIRST(S2.price) AS right_shoulder_price
    ONE ROW PER MATCH
    PATTERN (RISE S1 FALL RISE H FALL RISE S2 FALL)
    DEFINE
        RISE AS price > PREV(price),
        FALL AS price < PREV(price),
        S1 AS price > FIRST(price) * 1.05,  -- Left shoulder: 5% above start
        H AS price > FIRST(S1.price) * 1.1,  -- Head: 10% above left shoulder
        S2 AS price BETWEEN FIRST(S1.price) * 0.95 AND FIRST(S1.price) * 1.05  -- Right shoulder: similar to left
);
```

#### 4.3.2 IoT: Equipment Failure Prediction

```sql
-- Detect degrading sensor readings (3+ consecutive drops)
SELECT *
FROM sensor_readings
MATCH_RECOGNIZE (
    PARTITION BY sensor_id
    ORDER BY timestamp
    MEASURES
        FIRST(DROP1.temperature) AS start_temp,
        LAST(DROP.temperature) AS end_temp,
        COUNT(*) AS num_drops
    ONE ROW PER MATCH
    PATTERN (DROP+)
    DEFINE
        DROP AS temperature < PREV(temperature)
);
```

#### 4.3.3 User Behavior: Cart Abandonment

```sql
-- Detect cart abandonment pattern: view → add to cart → leave (no purchase)
SELECT *
FROM user_events
MATCH_RECOGNIZE (
    PARTITION BY user_id
    ORDER BY event_time
    MEASURES
        FIRST(VIEW.event_time) AS viewed_at,
        FIRST(ADD_CART.event_time) AS added_at,
        LAST(event_time) AS abandoned_at
    ONE ROW PER MATCH
    PATTERN (VIEW+ ADD_CART+ IDLE)
    DEFINE
        VIEW AS event_type = 'product_view',
        ADD_CART AS event_type = 'add_to_cart',
        IDLE AS event_time > LAST(ADD_CART.event_time) + INTERVAL '1 hour' AND event_type != 'purchase'
);
```

### 4.4 Parser Implementation

**Files to Create:**
- `/src/parser/match_recognize_parser.y` (NEW - dedicated pattern parser)

**Add Keywords:**
```c
"MATCH_RECOGNIZE"    { return KW_MATCH_RECOGNIZE; }
"MEASURES"           { return KW_MEASURES; }
"ONE ROW PER MATCH"  { return KW_ONE_ROW_PER_MATCH; }
"ALL ROWS PER MATCH" { return KW_ALL_ROWS_PER_MATCH; }
"AFTER MATCH SKIP"   { return KW_AFTER_MATCH_SKIP; }
"PATTERN"            { return KW_PATTERN; }
"DEFINE"             { return KW_DEFINE; }
"PREV"               { return KW_PREV; }
"FIRST"              { return KW_FIRST; }
"LAST"               { return KW_LAST; }
```

**Grammar:**
```yacc
table_reference:
    | table_name MATCH_RECOGNIZE '(' match_recognize_spec ')'
    ;

match_recognize_spec:
    partition_clause
    order_by_clause
    measures_clause
    row_mode_clause
    after_match_clause
    pattern_clause
    define_clause
    ;

measures_clause:
    MEASURES measure_definition_list
    ;

measure_definition:
    expr AS column_alias
    ;

pattern_clause:
    PATTERN '(' pattern_expression ')'
    ;

pattern_expression:
    pattern_term
    | pattern_expression '|' pattern_term
    ;

pattern_term:
    pattern_factor
    | pattern_term pattern_factor
    ;

pattern_factor:
    pattern_primary
    | pattern_primary '*'
    | pattern_primary '+'
    | pattern_primary '?'
    ;

pattern_primary:
    pattern_variable
    | '(' pattern_expression ')'
    ;

define_clause:
    DEFINE pattern_variable AS condition [, ...]
    ;
```

### 4.5 Pattern Matching Engine

**Files to Create:**
- `/src/match_recognize/pattern_matcher.cpp` (NEW)
- `/src/match_recognize/nfa.cpp` (NEW)

#### 4.5.1 NFA Construction

Convert pattern to Non-deterministic Finite Automaton:

```cpp
class NFA {
public:
    struct State {
        int id;
        bool is_accepting;
        std::vector<Transition> transitions;
    };

    struct Transition {
        State* to_state;
        std::string variable;  // Pattern variable name
        bool is_epsilon;       // ε-transition
    };

    NFA* compile_pattern(PatternExpression* pattern);
};

// Example: Pattern "A B+ C"
// States: 0 → A → 1 → B → 2 → B → 2 (loop) → C → 3 (accepting)
```

**Thompson's Construction:**
```cpp
NFA* compile_sequence(std::vector<PatternFactor*> factors) {
    NFA* nfa = new NFA();
    State* current = nfa->start_state;

    for (auto* factor : factors) {
        State* next = new State();
        current->add_transition(next, factor->variable);
        current = next;
    }

    current->is_accepting = true;
    return nfa;
}

NFA* compile_plus(PatternFactor* factor) {
    // A+ = A A*
    NFA* nfa = new NFA();
    State* s0 = nfa->start_state;
    State* s1 = new State();
    State* s2 = new State();

    s0->add_transition(s1, factor->variable);  // First A
    s1->add_epsilon_transition(s2);            // Exit loop
    s1->add_epsilon_transition(s0);            // Loop back
    s2->is_accepting = true;

    return nfa;
}
```

#### 4.5.2 Pattern Matching Execution

```cpp
class PatternMatcher {
public:
    std::vector<Match> match(const std::vector<Row>& rows, NFA* nfa, DefineClause* defines);

private:
    struct Match {
        std::vector<Row> matched_rows;
        std::map<std::string, std::vector<Row>> variable_bindings;
    };
};

std::vector<Match> PatternMatcher::match(const std::vector<Row>& rows,
                                         NFA* nfa, DefineClause* defines) {
    std::vector<Match> matches;

    for (int start = 0; start < rows.size(); start++) {
        // Try to match starting from this row
        Match m = try_match_from(rows, start, nfa, defines);
        if (m.is_valid()) {
            matches.push_back(m);
        }
    }

    return matches;
}

Match PatternMatcher::try_match_from(const std::vector<Row>& rows, int start,
                                     NFA* nfa, DefineClause* defines) {
    // Backtracking search through NFA
    std::vector<State*> current_states = {nfa->start_state};
    int current_row = start;
    Match match;

    while (current_row < rows.size()) {
        std::vector<State*> next_states;

        for (State* state : current_states) {
            for (Transition& t : state->transitions) {
                // Check if row matches transition variable
                if (evaluate_define(t.variable, rows[current_row], match, defines)) {
                    next_states.push_back(t.to_state);
                    match.variable_bindings[t.variable].push_back(rows[current_row]);
                }
            }
        }

        if (next_states.empty()) {
            break;  // No transitions possible
        }

        current_states = next_states;
        current_row++;
    }

    // Check if any current state is accepting
    for (State* state : current_states) {
        if (state->is_accepting) {
            match.matched_rows = std::vector<Row>(rows.begin() + start,
                                                   rows.begin() + current_row);
            return match;
        }
    }

    return Match();  // No match
}
```

### 4.6 MEASURES Evaluation

```cpp
std::vector<Column> evaluate_measures(MeasuresClause* measures, Match& match) {
    std::vector<Column> result;

    for (MeasureDefinition& measure : measures->definitions) {
        // Evaluate measure expression in context of match
        EvalContext ctx;
        ctx.set_variable_bindings(match.variable_bindings);

        TypedValue value = measure.expression->evaluate(ctx);
        result.push_back(Column{measure.alias, value});
    }

    return result;
}
```

**Special Functions:**
- `FIRST(variable.column)`: First row bound to variable
- `LAST(variable.column)`: Last row bound to variable
- `PREV(column)`: Previous row (within current match)
- `COUNT(*)`: Number of rows in match

### 4.7 Integration with Query Engine

```cpp
class MatchRecognizeOperator : public PhysicalOperator {
public:
    void open() override {
        // 1. Sort input by PARTITION BY + ORDER BY
        sorted_input = sort(input, partition_columns, order_columns);

        // 2. Compile pattern to NFA
        nfa = compile_pattern(pattern);
    }

    Tuple next() override {
        // 3. For each partition
        while (current_partition = get_next_partition()) {
            // 4. Match pattern on partition rows
            matches = pattern_matcher.match(current_partition, nfa, defines);

            // 5. For each match, emit rows
            for (Match& match : matches) {
                if (row_mode == ONE_ROW_PER_MATCH) {
                    return create_result_row(match);
                } else {
                    // ALL ROWS PER MATCH: emit multiple rows
                    return create_result_rows(match);
                }
            }
        }

        return null;  // No more rows
    }

private:
    NFA* nfa;
    PatternMatcher pattern_matcher;
};
```

### 4.8 Optimization

**Optimization 1: Early Termination**
```cpp
// If pattern is "A B C D E" and current row doesn't match A, skip
if (!matches_first_variable(row, pattern)) {
    skip();
}
```

**Optimization 2: Compiled State Machine**
```cpp
// Pre-compute state transitions (instead of interpreting NFA at runtime)
struct CompiledStateMachine {
    std::vector<CompiledState> states;
};

struct CompiledState {
    int id;
    bool is_accepting;
    std::function<bool(Row&)> condition;  // Pre-compiled condition
    std::vector<int> next_states;
};
```

### 4.9 Testing Requirements

**Test Categories:**

1. **Basic Patterns (100 tests)**
   - Simple sequence: `A B C`
   - Plus: `A+`, `A+ B+`
   - Star: `A*`, `A* B*`
   - Question: `A?`, `A? B?`
   - Alternation: `A | B`

2. **Pattern Combinations (100 tests)**
   - `(A B)+`
   - `A (B | C) D`
   - `A+ B* C?`

3. **MEASURES (50 tests)**
   - FIRST, LAST
   - PREV
   - COUNT(*)
   - Aggregates

4. **Edge Cases (50 tests)**
   - Empty partition
   - No matches
   - Overlapping matches
   - Backtracking

5. **Use Cases (50 tests)**
   - Stock patterns
   - IoT sensor analysis
   - User behavior analysis

**Total:** 350+ tests

### 4.10 Documentation

**Files to Create:**
- `/docs/specifications/dml/DML_MATCH_RECOGNIZE.md`

**Content:**
- Complete syntax
- 10+ real-world examples
- Pattern design guide
- Performance tuning

---

## 5-7. Remaining Features (Abbreviated)

Due to document length constraints, I'll provide abbreviated specifications for the remaining features:

---

## 5. EXCLUSION Constraints

**Priority:** 🟢 LOW (Beta 1.2)
**Effort:** 3-4 weeks (simplified: 1-2 weeks)

**Simplified Scope:**
- Support TSRANGE with `&&` operator only
- No full GiST integration (hardcoded overlap check)

**SQL Syntax:**
```sql
CREATE TABLE reservations (
    room_id INT,
    reserved_period TSRANGE,
    EXCLUDE (room_id WITH =, reserved_period WITH &&)
);
```

**Implementation:**
- Add `EXCLUDE` constraint type to catalog
- During INSERT/UPDATE, scan existing rows for conflicts
- Use B-tree index on (room_id, lower(reserved_period), upper(reserved_period))

**Testing:** 100+ tests

---

## 6. CHECK Constraints with Subqueries

**Priority:** 🟢 VERY LOW (Post-Beta)
**Effort:** 1-2 weeks
**Recommendation:** Issue WARNING, do not implement initially

**SQL Syntax:**
```sql
CREATE TABLE orders (
    customer_id INT,
    CHECK (customer_id IN (SELECT id FROM customers WHERE status = 'ACTIVE'))
);
```

**Implementation:**
- Parse subquery in CHECK
- Execute subquery on every INSERT/UPDATE
- Acquire locks on referenced tables

**Issues:**
- Severe performance impact
- Race conditions
- Not recommended by any database

**Alternative:** Recommend triggers instead

---

## 7. Polymorphic Table Functions

**Priority:** 🟢 VERY LOW (Post-Beta)
**Effort:** 10-12 weeks
**Recommendation:** Defer to post-Beta

**SQL Syntax:**
```sql
CREATE FUNCTION filter_table(input_table TABLE, condition VARCHAR)
RETURN TABLE
AS ...
```

**Implementation:**
- Generic table types (schema at runtime)
- Function interface with metadata passing
- Streaming execution

**Complexity:** Very high, minimal adoption

---

## 8. Testing Requirements

**Overall Test Summary:**

| Feature | Tests | Performance Benchmarks |
|---------|-------|------------------------|
| GROUPING SETS/CUBE/ROLLUP | 400+ | 10-100× vs UNION ALL |
| Full-Text Search | 700+ | <100ms for 1M docs |
| DEFERRABLE Constraints | 350+ | N/A |
| MATCH_RECOGNIZE | 350+ | <1s for 10K rows |
| EXCLUSION Constraints | 100+ | N/A |
| CHECK with Subqueries | 50+ | N/A (not recommended) |
| Polymorphic Table Functions | 0 | Deferred |

**Total Tests:** 1,950+

**Test Infrastructure:**
- Automated test suite (pytest/C++ test framework)
- Performance regression tests
- Compatibility tests with PostgreSQL/Oracle
- Fuzzing for parser and pattern matcher

---

## 9. Documentation Requirements

**User-Facing Documentation:**
- SQL Reference Manual updates
- Tutorial examples for each feature
- Performance tuning guides
- Migration guides from PostgreSQL/Oracle

**Internal Documentation:**
- Architecture design documents
- Code comments and API docs
- Test specifications

**Total Documentation:** 500+ pages

---

## 10. Migration and Compatibility

**PostgreSQL Compatibility:**
- Full-text search: 100% compatible
- DEFERRABLE constraints: 100% compatible
- GROUPING SETS: 100% compatible

**Oracle Compatibility:**
- MATCH_RECOGNIZE: Syntax compatible (subset)
- GROUPING SETS: 100% compatible

**MySQL Compatibility:**
- Full-text search: MATCH() AGAINST() wrapper
- GROUPING SETS: 100% compatible

**Migration Tools:**
- Schema converter (PostgreSQL → ScratchBird)
- Query rewriter (Oracle → ScratchBird)

---

## Appendix A: Implementation Schedule

**Beta 1.0 (Weeks 1-4):**
- GROUPING SETS / CUBE / ROLLUP ✅

**Beta 1.1 (Weeks 5-17):**
- Full-Text Search (Weeks 5-12)
- DEFERRABLE Constraints (Weeks 13-15)
- Testing and documentation (Weeks 16-17)

**Beta 1.2 (Weeks 18-30):**
- MATCH_RECOGNIZE (simplified) (Weeks 18-23)
- EXCLUSION Constraints (simplified) (Weeks 24-25)
- Testing and documentation (Weeks 26-30)

**Post-Beta:**
- CHECK with Subqueries (optional)
- Polymorphic Table Functions (deferred)

**Total:** 30 weeks (7.5 months) for full Beta SQL compliance

---

## Appendix B: Team Assignments

**Core Query Engine Team (3 engineers):**
- GROUPING SETS / CUBE / ROLLUP

**Search and Indexing Team (2 engineers):**
- Full-Text Search

**Constraint System Team (1 engineer):**
- DEFERRABLE Constraints
- EXCLUSION Constraints

**Advanced Analytics Team (2 engineers):**
- MATCH_RECOGNIZE

**QA Team (2 engineers):**
- Test automation
- Performance benchmarking

**Total:** 10 engineers

---

**End of Specification**
