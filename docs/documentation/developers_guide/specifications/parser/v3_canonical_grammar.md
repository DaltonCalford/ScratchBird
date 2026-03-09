# Specification: V3 Canonical SQL Grammar

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | Generated from source analysis |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:573` - Main parse entry point
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:657` - Statement dispatch logic
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/parser_v3.h:84` - Parser class definition
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/lexer_v3.cpp:189` - Gatekeeper keyword table
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/lexer_v3.h:63` - TokenType enum definition
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_canonical_rejections.cpp` - Canonical rejection tests
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/v3/parser/test_ambiguity_resolution.md` - Ambiguity resolution tests

## Synopsis

This specification defines the V3 canonical SQL grammar for ScratchBird, implementing a "Smart Parser, Dumb Lexer" architecture with Gatekeeper keyword model. The grammar supports standard SQL DDL/DML/DCL statements along with ScratchBird-specific extensions while maintaining strict canonical form enforcement.

## Scope

### In Scope

- Gatekeeper keyword model (~35 reserved keywords)
- Statement recognition and dispatch
- DDL statements (CREATE, ALTER, DROP, TRUNCATE)
- DML statements (SELECT, INSERT, UPDATE, DELETE, MERGE)
- Transaction and session control
- PSQL (Procedural SQL) statements
- Canonical form enforcement and rejection of legacy aliases

### Out of Scope

- Semantic analysis and type checking
- Query optimization
- Physical execution planning

## Background

The V3 parser uses a "Smart Parser, Dumb Lexer" architecture where:
- The lexer recognizes only ~35 Gatekeeper keywords (truly reserved)
- All other SQL keywords are contextual and resolved by the parser
- This allows natural use of words like `NAME`, `TYPE`, `VALUE` as identifiers
- Canonical form enforcement ensures consistent SQL dialect

## Specification

### Data Structures

#### TokenType Enum (Gatekeeper Keywords)

```cpp
// From include/scratchbird/parser/lexer_v3.h:63
enum class TokenType : uint16_t {
    // Special Tokens
    END_OF_FILE = 0,
    ERROR,

    // Literals
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BLOB_LITERAL,
    IDENTIFIER,
    PARAMETER,

    // Operators (arithmetic, comparison, bitwise, JSON, etc.)
    PLUS, MINUS, STAR, SLASH, PERCENT, CARET, DOUBLE_PIPE,
    EQUAL, NOT_EQUAL, LESS_THAN, GREATER_THAN, LESS_EQUAL, GREATER_EQUAL,
    // ... additional operators

    // Punctuation
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACKET, RIGHT_BRACKET,
    LEFT_BRACE, RIGHT_BRACE, COMMA, SEMICOLON, DOT, DOUBLE_DOT, EXCLAIM_COLON, COLON,

    // Gatekeeper Keywords (~35 reserved words)
    KW_SELECT, KW_INSERT, KW_UPDATE, KW_DELETE, KW_MERGE,
    KW_CREATE, KW_ALTER, KW_DROP, KW_TRUNCATE, KW_COPY,
    KW_GRANT, KW_REVOKE, KW_COMMIT, KW_ROLLBACK, KW_BEGIN, KW_END,
    KW_DECLARE, KW_SET, KW_SHOW, KW_EXPLAIN, KW_ANALYZE,
    KW_CALL, KW_EXECUTE, KW_PREPARE,
    KW_FROM, KW_WHERE, KW_GROUP, KW_HAVING, KW_ORDER, KW_LIMIT, KW_OFFSET,
    KW_UNION, KW_INTERSECT, KW_EXCEPT, KW_WITH,
    KW_AND, KW_OR, KW_NOT, KW_IS, KW_IN, KW_BETWEEN, KW_LIKE,
    KW_CASE, KW_WHEN, KW_THEN, KW_ELSE, KW_NULL, KW_TRUE, KW_FALSE,
    KW_EXISTS, KW_CAST, KW_AS,
    KW_JOIN, KW_ON, KW_USING, KW_LATERAL,
    KW_VALUES, KW_INTO, KW_DEFAULT,
    KW_START, KW_IF, KW_RETURN,
};
```

#### ParseResult Class

```cpp
// From include/scratchbird/parser/parser_v3.h:51
class ParseResult {
public:
    ParseResult() = default;

    bool success() const { return errors_.empty() && statement_ != nullptr; }
    Statement* statement() const { return statement_; }
    const std::vector<ParseError>& errors() const { return errors_; }

    void setStatement(Statement* stmt) { statement_ = stmt; }
    void addError(const ParseError& error) { errors_.push_back(error); }

private:
    Statement* statement_ = nullptr;
    std::vector<ParseError> errors_;
};
```

#### ParserOptions (Capability Profile)

```cpp
// From include/scratchbird/parser/parser_v3.h:73
struct ParserOptions {
    std::string active_profile = "native";
    std::set<std::string> enabled_feature_keys;
    std::set<std::string> disabled_feature_keys;
};
```

### Grammar (EBNF)

#### Top-Level Statement

```ebnf
<statement> ::= <ddl_statement>
              | <dml_statement>
              | <transaction_statement>
              | <session_statement>
              | <dcl_statement>
              | <psql_statement>
              | <utility_statement>
```

#### DDL Statements

```ebnf
<ddl_statement> ::= <create_statement>
                  | <alter_statement>
                  | <drop_statement>
                  | <truncate_statement>

<create_statement> ::= CREATE [ OR REPLACE | OR ALTER ] <create_object>
<create_object> ::= TABLE <create_table>
                  | INDEX [ UNIQUE ] <create_index>
                  | VIEW <create_view>
                  | SEQUENCE <create_sequence>
                  | SCHEMA <create_schema>
                  | FUNCTION <create_function>
                  | PROCEDURE <create_procedure>
                  | TRIGGER <create_trigger>
                  | USER <create_user>
                  | ROLE <create_role>
                  | JOB <create_job>

<alter_statement> ::= ALTER <alter_object>
<alter_object> ::= TABLE <alter_table>
                 | INDEX <alter_index>
                 | VIEW <alter_view>
                 | SEQUENCE <alter_sequence>
                 | SCHEMA <alter_schema>
                 | FUNCTION <alter_function>
                 | PROCEDURE <alter_procedure>

<drop_statement> ::= DROP <drop_object>
<drop_object> ::= TABLE <drop_table>
                | INDEX <drop_index>
                | VIEW <drop_view>
                | SEQUENCE <drop_sequence>
                | SCHEMA <drop_schema>
                | FUNCTION <drop_function>
                | PROCEDURE <drop_procedure>
```

#### DML Statements

```ebnf
<dml_statement> ::= <select_statement>
                  | <insert_statement>
                  | <update_statement>
                  | <delete_statement>
                  | <merge_statement>

<select_statement> ::= SELECT [ DISTINCT ] <select_list>
                       [ FROM <from_clause> ]
                       [ WHERE <where_clause> ]
                       [ GROUP BY <group_by_clause> ]
                       [ HAVING <having_clause> ]
                       [ ORDER BY <order_by_clause> ]
                       [ LIMIT <limit_clause> ]
                       [ OFFSET <offset_clause> ]

<insert_statement> ::= INSERT INTO <table_path>
                       [ ( <column_list> ) ]
                       <insert_source>
                       [ ON CONFLICT <conflict_action> ]

<update_statement> ::= UPDATE <table_path>
                       SET <set_clause>
                       [ WHERE <where_clause> ]
                       [ RETURNING <returning_clause> ]

<delete_statement> ::= DELETE FROM <table_path>
                       [ WHERE <where_clause> ]
                       [ RETURNING <returning_clause> ]
```

### Interface Contracts

#### Function: `Parser::parseStatement()`

```cpp
// Source: src/parser/parser_v3.cpp:573
ParseResult parseStatement();
```

**Preconditions:**
- Parser initialized with valid SQL input
- Lexer positioned at start of statement

**Postconditions:**
- Returns ParseResult containing AST or error list
- On success, result.statement() contains valid Statement pointer
- On failure, result.errors() contains error descriptions

**Error Handling:**
- Lexical errors reported via error reporter
- Syntax errors collected in ParseResult.errors()
- Partial recovery via synchronize() for multi-statement parsing

#### Function: `Parser::parseStatementInternal()`

```cpp
// Source: src/parser/parser_v3.cpp:657
Statement* parseStatementInternal();
```

**Preconditions:**
- ParseModeGuard active for STATEMENT mode
- Current token is at statement start

**Postconditions:**
- Returns parsed Statement AST node or nullptr on error
- Advances lexer past end of statement

**Algorithm:**
```
Input:  Parser state positioned at statement start
Output: Statement AST node

1. Check for JDBC escape blocks ({fn ...}) → reject with PRS_0505
2. Check for REPLACE INTO → reject with PRS_0505
3. Check statement initiator keywords:
   - KW_WITH → parseWithStatement()
   - RECREATE → parseRecreate()
   - KW_CREATE → parseCreate()
   - KW_ALTER → parseAlter()
   - KW_DROP → parseDrop()
   - KW_TRUNCATE → parseTruncate()
   - KW_SELECT → parseSelect()
   - KW_INSERT → parseInsert()
   - KW_UPDATE → parseUpdate()
   - KW_DELETE → parseDelete()
4. Check for contextual keyword statements:
   - DOC → parseDocPathFilterSurface() (requires F_DOC_PATH_FILTER)
   - TS → parseTimeBucketAggSurface() (requires F_TS_BUCKET_AGG)
   - SEARCH → parseSearchDslSurface() (requires F_SEARCH_QUERY_DSL)
   - VECTOR → parseVectorAnnSurface() (requires F_VECTOR_ANN)
   - REDIS → parseNoSqlSurface()
   - etc.
5. If no match → error("Expected SQL statement")
```

### Canonical Form Enforcement

The parser enforces canonical SQL forms and rejects legacy aliases:

| Rejected Form | Canonical Form | Error Code |
|---------------|----------------|------------|
| `{fn ...}` | Use canonical SQL | PRS_0505 |
| `REPLACE INTO` | `INSERT ... ON CONFLICT` | PRS_0505 |
| `FILTER DOC PATH` | `DOC PATH FILTER` | PRS_0505 |
| `AGGREGATE TIME BUCKET` | `TS BUCKET AGG` | PRS_0505 |
| `ANN` | `VECTOR ANN QUERY` | PRS_0505 |
| `CQL`, `MONGO`, `CYPHER`, `MILVUS` prefixes | Engine-specific canonical forms | PRS_0505 |
| `CREATE MATERIALIZED VIEW` | `CREATE VIEW ... MATERIALIZED` | PRS_0505 |
| `DESC` | `DESCRIBE` | PRS_0505 |
| `VACUUM` | `SWEEP DATABASE` | PRS_0505 |

Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:660-706`

### State Machines

#### Statement Dispatch State Machine

```
                    ┌─────────────────────────────────────┐
                    │         Statement Start             │
                    └───────────────┬─────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            │                       │                       │
            ▼                       ▼                       ▼
    ┌───────────────┐      ┌───────────────┐      ┌───────────────┐
    │ Gatekeeper    │      │ Contextual    │      │ Rejected      │
    │ Keyword       │      │ Keyword       │      │ Legacy        │
    └───────┬───────┘      └───────┬───────┘      └───────┬───────┘
            │                       │                       │
     ┌──────┴──────┐          ┌──────┴──────┐         ┌──────┴──────┐
     │ SELECT      │          │ DOC         │         │ {fn...}     │
     │ INSERT      │          │ TS          │         │ REPLACE     │
     │ UPDATE      │          │ SEARCH      │         │ VACUUM      │
     │ DELETE      │          │ VECTOR      │         └─────────────┘
     │ CREATE      │          │ REDIS       │
     │ ALTER       │          └─────────────┘
     │ DROP        │
     └─────────────┘
```

### Index Type Recognition

```cpp
// Source: src/parser/parser_v3.cpp:330
static std::optional<IndexType> indexTypeFromName(std::string_view name) {
    if (caseInsensitiveEquals(name, "BTREE")) return IndexType::BTREE;
    if (caseInsensitiveEquals(name, "HASH")) return IndexType::HASH;
    if (caseInsensitiveEquals(name, "HNSW")) return IndexType::HNSW;
    if (caseInsensitiveEquals(name, "FULLTEXT")) return IndexType::FULLTEXT;
    if (caseInsensitiveEquals(name, "GIN")) return IndexType::GIN;
    if (caseInsensitiveEquals(name, "GIST")) return IndexType::GIST;
    if (caseInsensitiveEquals(name, "BRIN")) return IndexType::BRIN;
    // ... additional index types (58 total)
    if (caseInsensitiveEquals(name, "REDIS_GEO")) return IndexType::REDIS_GEO;
    return std::nullopt;
}
```

### Feature Gates

Feature keys control availability of parser extensions:

```cpp
// Source: src/parser/parser_v3.cpp:293
constexpr char kFeatureDocPathFilter[] = "F_DOC_PATH_FILTER";
constexpr char kFeatureTsBucketAgg[] = "F_TS_BUCKET_AGG";
constexpr char kFeatureSearchQueryDsl[] = "F_SEARCH_QUERY_DSL";
constexpr char kFeatureVectorAnn[] = "F_VECTOR_ANN";
constexpr char kFeatureHybridBridgeHint[] = "F_HYBRID_BRIDGE_HINT";
constexpr char kFeatureSecurityUserAccountDdl[] = "F_SECURITY_USER_ACCOUNT_DDL";
// ... etc.

bool requireFeature(const char* feature_key) {
    if (capability_feature_keys_.count(feature_key) != 0) {
        return true;
    }
    errorCode("PRS_0503", std::string("Feature ").append(feature_key)
              .append(" not enabled for active profile"));
    return false;
}
```

## Invariants

1. **Gatekeeper Keyword Invariant**: Only ~35 keywords are truly reserved; all others can be used as identifiers
   - Verification: Lexer test cases with contextual keywords as column names

2. **Canonical Form Invariant**: Legacy SQL aliases are rejected with PRS_0505
   - Verification: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_canonical_rejections.cpp`

3. **Feature Gate Invariant**: Parser extensions require explicit feature key enablement
   - Verification: Feature flag tests in parser capability contract tests

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| PRS_0503 | Feature not enabled for profile | Enable feature in ParserOptions |
| PRS_0504 | Syntax error in DDL statement | Check statement syntax |
| PRS_0505 | Legacy/alias form rejected | Use canonical SQL form |
| PRS_0507 | RRULE/schedule syntax error | Correct RRULE expression |
| PRS_0508 | Unsupported recurrence token | Use supported token |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_parser_v3_canonical_rejections.cpp` | Legacy form rejection |
| `tests/unit/test_parser_v3_gap_contracts.cpp` | Gap analysis contracts |
| `tests/unit/test_parser_v3_index_management.cpp` | Index type parsing |
| `tests/v3/parser/test_ambiguity_resolution.md` | Ambiguity resolution |
| `tests/v3/parser/test_type_and_literal_spec.md` | Type/literal parsing |

## Related Specifications

- [Path Resolution Logic](./path_resolution_logic.md) - Schema path navigation
- [Semantic Binding Flow](./semantic_binding_flow.md) - AST to catalog binding

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Gatekeeper Keyword | One of ~35 truly reserved SQL keywords recognized by lexer |
| Contextual Keyword | SQL keyword that can also be an identifier; resolved by parser context |
| Canonical Form | Standard SQL syntax that all input must conform to |
| Feature Gate | Capability flag controlling parser extension availability |

### References

- ScratchBird PARSER_V3_IMPLEMENTATION_PLAN.md
- SQL:2023 Standard (ISO/IEC 9075)

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | Source Analysis |
