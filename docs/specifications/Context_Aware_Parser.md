# Context-Aware Parser Specification

## Overview

ScratchBird implements an intelligent context-aware parser that understands SQL semantics through positional context rather than rigid keyword reservation. This dramatically reduces reserved words and enables automatic statement termination.

## Core Concepts

### 1. Contextual Token Classification

Instead of having fixed reserved words, tokens are classified based on their position and surrounding context:

```sql
-- Traditional parser would reject these:
CREATE TABLE timestamp (timestamp timestamp);  -- ERROR in most DBs

-- Context-aware parser understands:
CREATE TABLE timestamp (timestamp timestamp);
--           ^^^^^^^    ^^^^^^^   ^^^^^^^
--           name       name      datatype
-- Position: table_name  column    type_spec

-- Even more complex:
CREATE TABLE select (from integer, where varchar(100), group integer);
-- All SQL keywords used as identifiers - perfectly valid!
```

### 2. State Machine Architecture

```cpp
class ContextAwareParser {
    enum ParseContext {
        STATEMENT_START,      // Expecting command
        CREATE_TYPE,         // After CREATE, expecting object type
        TABLE_NAME,          // Expecting table identifier
        COLUMN_LIST,         // Inside column definitions
        COLUMN_NAME,         // Expecting column identifier
        DATA_TYPE,           // Expecting type specification
        CONSTRAINT_SPEC,     // Processing constraints
        WHERE_CLAUSE,        // Inside WHERE
        EXPRESSION,          // General expression
        // ... hundreds more contexts
    };
    
    struct TokenContext {
        Token token;
        ParseContext context;
        vector<ParseContext> context_stack;
        map<string, TokenRole> role_map;
    };
    
    TokenRole classify_token(const Token& tok, ParseContext ctx) {
        // Context determines role, not keyword list
        if (ctx == STATEMENT_START) {
            if (tok.text_upper == "CREATE") return COMMAND_CREATE;
            if (tok.text_upper == "SELECT") return COMMAND_SELECT;
            // Not a command? Must be identifier
            return IDENTIFIER;
        }
        
        if (ctx == DATA_TYPE) {
            if (is_known_type(tok.text_upper)) return TYPE_NAME;
            // Could be custom type or domain
            return IDENTIFIER_OR_TYPE;
        }
        
        // In most contexts, anything can be an identifier
        return IDENTIFIER;
    }
};
```

### 3. Automatic Statement Termination

The parser knows when a statement is complete without requiring semicolons:

```sql
-- Parser understands these are complete:
CREATE TABLE users (id integer, name varchar(100))
SELECT * FROM users WHERE id = 1
INSERT INTO users VALUES (1, 'John')

-- Can still use semicolons for clarity or multiple statements:
CREATE TABLE t1 (id integer); CREATE TABLE t2 (id integer);

-- Or use newlines as implicit terminators in interactive mode:
CREATE TABLE products (
    id integer,
    name varchar(100)
)
↵ -- Statement complete, executed

SELECT * FROM products
↵ -- Statement complete, executed
```

### 4. Grammar State Transitions

```yaml
StateMachine:
  STATEMENT_START:
    CREATE: -> CREATE_TYPE
    ALTER: -> ALTER_TYPE
    DROP: -> DROP_TYPE
    SELECT: -> SELECT_LIST
    INSERT: -> INSERT_TARGET
    UPDATE: -> UPDATE_TARGET
    DELETE: -> DELETE_TARGET
    WITH: -> CTE_DEFINITION
    
  CREATE_TYPE:
    TABLE: -> TABLE_NAME
    VIEW: -> VIEW_NAME
    INDEX: -> INDEX_NAME
    FUNCTION: -> FUNCTION_NAME
    PROCEDURE: -> PROCEDURE_NAME
    TRIGGER: -> TRIGGER_NAME
    TYPE: -> TYPE_NAME
    DOMAIN: -> DOMAIN_NAME
    
  TABLE_NAME:
    <identifier>: -> TABLE_OPTIONS_OR_COLUMNS
    
  TABLE_OPTIONS_OR_COLUMNS:
    "(": -> COLUMN_LIST
    AS: -> SELECT_STATEMENT
    LIKE: -> TABLE_REFERENCE
    # No more tokens? Statement complete!
    
  COLUMN_LIST:
    <identifier>: -> DATA_TYPE
    CONSTRAINT: -> TABLE_CONSTRAINT
    ")": -> TABLE_OPTIONS_OR_END
    
  DATA_TYPE:
    INTEGER|VARCHAR|TIMESTAMP|...: -> COLUMN_OPTIONS
    <identifier>: -> COLUMN_OPTIONS  # Could be custom type
    
  COLUMN_OPTIONS:
    NOT: -> EXPECTING_NULL
    NULL: -> MORE_COLUMN_OPTIONS
    DEFAULT: -> DEFAULT_VALUE
    PRIMARY: -> EXPECTING_KEY
    UNIQUE: -> MORE_COLUMN_OPTIONS
    REFERENCES: -> FOREIGN_KEY_SPEC
    CHECK: -> CHECK_CONSTRAINT
    ",": -> COLUMN_LIST  # Next column
    ")": -> TABLE_OPTIONS_OR_END  # End of columns
```

### 5. Minimal Reserved Words

With context-aware parsing, only structural tokens need reservation:

```sql
-- These MUST be reserved (structural):
(  )  ,  .  ;  --  /*  */

-- These are contextual (not reserved):
CREATE SELECT INSERT UPDATE DELETE  -- Commands only at statement start
INTEGER VARCHAR TIMESTAMP           -- Types only in type position
FROM WHERE GROUP ORDER BY           -- Clause keywords only after SELECT
AND OR NOT                          -- Operators only in expressions

-- This means you can do:
CREATE TABLE from (
    select integer,
    from varchar(100),
    where timestamp,
    group integer,
    by varchar(50),
    order integer
);

SELECT select, from, where 
FROM from 
WHERE where = 'condition' 
GROUP BY group;
```

### 6. Intelligent Disambiguation

```cpp
class ContextParser {
    // Smart lookahead for ambiguous cases
    TokenRole disambiguate(TokenStream& stream, ParseContext ctx) {
        Token current = stream.current();
        Token next = stream.peek();
        
        // "CREATE" could be command or identifier
        if (current.text_upper == "CREATE") {
            if (ctx == STATEMENT_START) {
                // At statement start, CREATE is a command
                return COMMAND_CREATE;
            } else if (ctx == COLUMN_NAME) {
                // As column name, it's an identifier
                return IDENTIFIER;
            }
        }
        
        // "TIMESTAMP" could be type or identifier
        if (current.text_upper == "TIMESTAMP") {
            if (ctx == DATA_TYPE) {
                // After column name, it's a type
                return TYPE_TIMESTAMP;
            } else if (ctx == COLUMN_NAME) {
                // As column name, it's an identifier
                return IDENTIFIER;
            } else if (ctx == EXPRESSION) {
                // Could be function or column reference
                if (next.text == "(") {
                    return FUNCTION_NAME;  // TIMESTAMP(...)
                } else {
                    return COLUMN_REF;     // table.timestamp
                }
            }
        }
    }
};
```

### 7. Multi-Dialect Support

Different SQL dialects have different expectations:

```cpp
class DialectAwareParser {
    struct DialectRules {
        bool requires_semicolon;
        bool allows_keywords_as_identifiers;
        bool requires_quotes_for_keywords;
        map<string, ParseContext> dialect_specific_contexts;
    };
    
    DialectRules rules;
    
    void set_dialect(SQLDialect dialect) {
        switch (dialect) {
            case SCRATCHBIRD_NATIVE:
                rules = {
                    .requires_semicolon = false,
                    .allows_keywords_as_identifiers = true,
                    .requires_quotes_for_keywords = false
                };
                break;
                
            case POSTGRESQL:
                rules = {
                    .requires_semicolon = true,
                    .allows_keywords_as_identifiers = false,
                    .requires_quotes_for_keywords = true
                };
                break;
                
            case MYSQL:
                rules = {
                    .requires_semicolon = true,
                    .allows_keywords_as_identifiers = false,
                    .requires_quotes_for_keywords = false  // Uses backticks
                };
                break;
        }
    }
};
```

### 8. Statement Completion Detection

```cpp
class StatementCompleter {
    bool is_statement_complete(ParseContext ctx, TokenStream& stream) {
        switch (ctx) {
            case CREATE_TABLE_END:
                // Table creation is complete
                return true;
                
            case SELECT_COMPLETE:
                // SELECT is complete if no more clauses coming
                Token next = stream.peek();
                if (is_clause_keyword(next)) {
                    return false;  // More clauses coming
                }
                return true;  // Statement complete
                
            case INSERT_VALUES_END:
                // INSERT complete after values
                return true;
                
            case EXPRESSION:
                // Expression complete if balanced parens and no operators pending
                return is_expression_complete(stream);
        }
    }
    
    bool is_expression_complete(TokenStream& stream) {
        // Check for balanced parentheses
        if (paren_depth > 0) return false;
        
        // Check for pending operators
        Token last = stream.previous();
        if (is_binary_operator(last)) return false;
        
        // Check for incomplete function calls
        if (last.text == "(") return false;
        
        return true;
    }
};
```

### 9. Error Recovery and Suggestions

```cpp
class SmartErrorRecovery {
    void handle_parse_error(ParseContext ctx, Token unexpected) {
        switch (ctx) {
            case DATA_TYPE:
                suggest("Expected a data type. Did you mean: INTEGER, VARCHAR, TIMESTAMP?");
                
                // Check if user meant to use identifier
                if (could_be_identifier(unexpected)) {
                    suggest("If '" + unexpected.text + "' is a column name, you may have forgotten the data type.");
                }
                break;
                
            case TABLE_NAME:
                if (is_keyword(unexpected)) {
                    suggest("'" + unexpected.text + "' can be used as a table name in ScratchBird. " +
                           "Other databases may require quotes: \"" + unexpected.text + "\"");
                }
                break;
                
            case STATEMENT_START:
                // Intelligent command suggestions
                if (starts_with(unexpected.text, "SEL")) {
                    suggest("Did you mean: SELECT?");
                }
                if (starts_with(unexpected.text, "CRE")) {
                    suggest("Did you mean: CREATE?");
                }
                break;
        }
    }
};
```

### 10. Interactive Mode Enhancements

```sql
-- Interactive mode with smart completion
scratchbird> CREATE TABLE 
-- Parser knows: expecting table name

scratchbird> CREATE TABLE users
-- Parser knows: expecting ( or AS or LIKE

scratchbird> CREATE TABLE users (
-- Parser knows: expecting column definition

scratchbird> CREATE TABLE users (id
-- Parser knows: expecting data type

scratchbird> CREATE TABLE users (id integer
-- Parser knows: expecting column constraint or comma or )

scratchbird> CREATE TABLE users (id integer, name
-- Parser knows: expecting data type for 'name'

scratchbird> CREATE TABLE users (id integer, name varchar
-- Parser knows: expecting (size) for varchar

scratchbird> CREATE TABLE users (id integer, name varchar(100)
-- Parser knows: statement could end here or continue with more columns

scratchbird> CREATE TABLE users (id integer, name varchar(100))
-- Parser knows: statement complete! Auto-executes in interactive mode
Table 'users' created successfully.
```

## Implementation Phases

### Phase 1: Core Context Engine
- Basic state machine
- Context stack management
- Token classification

### Phase 2: Statement Completion
- Completion detection algorithms
- Automatic execution in interactive mode
- Multi-statement handling

### Phase 3: Keyword Liberation
- Allow keywords as identifiers
- Context-based disambiguation
- Backward compatibility mode

### Phase 4: Smart Error Recovery
- Contextual error messages
- Intelligent suggestions
- Typo correction

### Phase 5: Advanced Features
- Dialect-specific rules
- Custom context plugins
- IDE integration APIs

## Benefits

### 1. **Fewer Reserved Words**
- From ~200 reserved words to ~10
- No more "keyword conflicts"
- Future-proof (new features don't break old code)

### 2. **Natural SQL Writing**
```sql
-- Write SQL like English, parser figures it out
CREATE TABLE order (
    order integer,     -- 'order' as identifier!
    by varchar(100),   -- 'by' as identifier!
    group integer,     -- 'group' as identifier!
    select boolean     -- 'select' as identifier!
)
```

### 3. **Automatic Statement Termination**
- No more forgotten semicolons
- Natural interactive experience
- Still supports explicit terminators

### 4. **Better Error Messages**
```
Error at line 3: Expected data type after column name 'age'
  Suggestion: Common types are INTEGER, VARCHAR(n), DECIMAL(p,s)
  
Error at line 5: Incomplete statement
  Context: In CREATE TABLE, expecting column definitions
  Suggestion: Add columns or complete with ')'
```

### 5. **IDE Integration**
- Perfect autocomplete based on context
- Real-time syntax validation
- Intelligent suggestions

## Testing Strategy

```cpp
// Test context-aware parsing
TEST(ContextParser, KeywordsAsIdentifiers) {
    auto result = parse("CREATE TABLE select (from integer, where varchar(100))");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.table_name, "select");
    ASSERT_EQ(result.columns[0].name, "from");
    ASSERT_EQ(result.columns[1].name, "where");
}

// Test automatic completion
TEST(ContextParser, AutoCompletion) {
    auto result = parse("CREATE TABLE t1 (id integer)");
    ASSERT_TRUE(result.is_complete);
    ASSERT_TRUE(result.ready_to_execute);
}

// Test multi-statement
TEST(ContextParser, MultiStatement) {
    auto results = parse_multiple(
        "CREATE TABLE t1 (id integer)\n"
        "CREATE TABLE t2 (id integer)\n"
        "SELECT * FROM t1"
    );
    ASSERT_EQ(results.size(), 3);
    ASSERT_TRUE(all_complete(results));
}

// Test error recovery
TEST(ContextParser, ErrorRecovery) {
    auto result = parse("CREATE TABEL users");  // Typo
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.suggestions.contains("TABLE"));
}
```

## Comparison with Traditional Parsers

| Feature | Traditional | Context-Aware |
|---------|------------|---------------|
| Reserved Words | 200+ | ~10 |
| Keywords as Identifiers | ❌ | ✅ |
| Auto Statement Completion | ❌ | ✅ |
| Context-Based Errors | ❌ | ✅ |
| Natural Language Feel | ❌ | ✅ |
| Backward Compatible | N/A | ✅ |

## Challenges and Solutions

### Challenge 1: Ambiguity
**Solution**: Multi-token lookahead with weighted context scoring

### Challenge 2: Performance
**Solution**: JIT-compiled state machines for hot paths

### Challenge 3: Dialect Compatibility
**Solution**: Pluggable dialect rules with compatibility modes

### Challenge 4: Tool Integration
**Solution**: Provide traditional parser mode for legacy tools

## Conclusion

The context-aware parser represents a fundamental advancement in SQL parsing technology. By understanding context rather than relying on reserved words, ScratchBird provides a more natural, flexible, and user-friendly SQL experience while maintaining full compatibility with existing SQL standards.