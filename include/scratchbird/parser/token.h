#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <unordered_map>

namespace scratchbird
{
    namespace parser
    {

        // Token types for SQL lexer
        enum class TokenType : uint16_t
        {
            // Special tokens
            END_OF_FILE = 0,
            ERROR,

            // Literals
            INTEGER_LITERAL,
            FLOAT_LITERAL,
            STRING_LITERAL,

            // Identifiers and keywords
            IDENTIFIER,
            KEYWORD,

            // Operators and punctuation
            PLUS,    // +
            MINUS,   // -
            STAR,    // *
            SLASH,   // /
            PERCENT, // %

            EQUAL,         // =
            NOT_EQUAL,     // <>
            LESS_THAN,     // <
            GREATER_THAN,  // >
            LESS_EQUAL,    // <=
            GREATER_EQUAL, // >=

            LEFT_PAREN,  // (
            RIGHT_PAREN, // )
            COMMA,       // ,
            SEMICOLON,   // ;
            DOT,         // .
            COLON,       // : (used in :: for type casting)
            COLON_EQUALS,  // := (PL/SQL assignment operator) WP-6 PARSE-2

            // JSON operators (Phase 1 Task 7)
            ARROW,              // -> (JSON field as JSON)
            DOUBLE_ARROW,       // ->> (JSON field as text)
            HASH_ARROW,         // #> (JSON path as JSON)
            HASH_DOUBLE_ARROW,  // #>> (JSON path as text)

            // Array operators (Phase 2 Task 12)
            AMPERSAND_AMPERSAND,  // && (array overlap, also range overlap in Task 15)
            AT_GREATER,           // @> (array contains, also range contains in Task 15)
            LESS_AT,              // <@ (array contained by, also range contained by in Task 15)
            LEFT_BRACKET,         // [ (array literal, also range bound in Task 15)
            RIGHT_BRACKET,        // ] (array literal, also range bound in Task 15)

            // Range operators (Task 15 Phase 4)
            SHIFT_LEFT,           // << (strictly left of)
            SHIFT_RIGHT,          // >> (strictly right of)
            MINUS_PIPE_MINUS,     // -|- (adjacent)

            // Regex operators (Phase 2 Task 13)
            TILDE,                // ~ (regex match)
            TILDE_STAR,           // ~* (regex match case-insensitive)
            EXCLAIM_TILDE,        // !~ (regex not match)
            EXCLAIM_TILDE_STAR,   // !~* (regex not match case-insensitive)

            // SQL Keywords (minimal set for Alpha)
            // These are detected post-lexing based on identifier content
            KW_CREATE,
            KW_TABLE,
            KW_INDEX,   // Phase 2 Task 2.3
            KW_UNIQUE,  // Phase 2 Task 2.3
            KW_INSERT,
            KW_INTO,
            KW_VALUES,
            KW_SELECT,
            KW_FROM,
            KW_WHERE,
            KW_UPDATE,   // Phase 1 Task 2.1: UPDATE statement
            KW_DELETE,   // Phase 1 Task 2.2: DELETE statement
            KW_MERGE,    // Alpha 1 - Advanced SQL: MERGE statement
            KW_MATCHED,  // Alpha 1 - Advanced SQL: MERGE WHEN MATCHED
            KW_SOURCE,   // Alpha 1 - Advanced SQL: MERGE WHEN NOT MATCHED BY SOURCE
            KW_TARGET,   // Alpha 1 - Advanced SQL: MERGE WHEN NOT MATCHED BY TARGET
            KW_RETURNING, // Alpha 1 - Advanced SQL: RETURNING clause
            KW_NULL,
            KW_NOT,
            KW_ANALYZE,  // Phase 1 Task 1.1.2: Statistics collection
            KW_EXPLAIN,  // Phase 1 Task 1.5: EXPLAIN command
            KW_COLUMN,   // Phase 1 Task 1.1.2: ANALYZE ... COLUMN ...
            KW_SAMPLE,   // Phase 1 Task 1.1.2: ANALYZE ... SAMPLE ...

            // JOIN keywords (Phase 1 Task 3.1)
            KW_JOIN,
            KW_INNER,
            KW_LEFT,
            KW_RIGHT,
            KW_FULL,
            KW_OUTER,
            KW_CROSS,
            KW_NATURAL,
            KW_USING,

            // Aggregation keywords (Phase 1 Task 4.1)
            KW_GROUP,
            KW_BY,
            KW_HAVING,
            KW_ORDER,
            KW_ASC,
            KW_DESC,
            KW_LIMIT,
            KW_OFFSET,
            KW_DISTINCT,
            KW_ALL,

            // Advanced grouping (ROLLUP, CUBE, GROUPING SETS)
            KW_ROLLUP,
            KW_CUBE,
            KW_GROUPING,
            KW_SETS,

            // Set operations (UNION, INTERSECT, EXCEPT)
            KW_UNION,
            KW_INTERSECT,

            // Aggregate functions (Phase 1 Task 4.1)
            KW_COUNT,
            KW_SUM,
            KW_AVG,
            KW_MIN,
            KW_MAX,
            KW_ARRAY_AGG,  // Phase 2 Task 12: Array aggregate function

            // Window functions (Phase 1 Task 6)
            KW_OVER,
            KW_PARTITION,
            KW_ROWS,
            KW_RANGE,
            KW_GROUPS,   // P2-9: GROUPS frame mode
            KW_BETWEEN,
            KW_UNBOUNDED,
            KW_PRECEDING,
            KW_FOLLOWING,
            KW_CURRENT,
            KW_ROW,
            KW_ROW_NUMBER,
            KW_RANK,
            KW_DENSE_RANK,
            KW_LAG,
            KW_LEAD,
            KW_FIRST_VALUE,
            KW_LAST_VALUE,
            KW_NTH_VALUE,
            KW_CUME_DIST,
            KW_PERCENT_RANK,
            KW_NULLS,
            KW_FIRST,
            KW_LAST,
            KW_AND,
            KW_OR,        // Logical OR / OR keyword (for CREATE OR REPLACE)

            // Numeric types
            KW_INT,
            KW_INTEGER,
            KW_SMALLINT,
            KW_BIGINT,
            KW_TINYINT,
            KW_INT128,
            KW_UINT8,
            KW_UINT16,
            KW_UINT32,
            KW_UINT64,
            KW_REAL,
            KW_FLOAT,
            KW_DOUBLE,
            KW_DECIMAL,
            KW_NUMERIC,
            KW_MONEY,

            // String types
            KW_CHAR,
            KW_CHARACTER,
            KW_VARCHAR,
            KW_TEXT,

            // Binary types
            KW_BINARY,
            KW_VARBINARY,
            KW_BLOB,
            KW_BYTEA,

            // Date/Time types
            KW_DATE,
            KW_TIME,
            KW_TIMESTAMP,
            KW_INTERVAL,

            // Boolean
            KW_BOOLEAN,
            KW_BOOL,

            // Special types
            KW_UUID,
            KW_JSON,
            KW_JSONB,
            KW_XML,
            KW_VECTOR,
            KW_ARRAY,  // Phase 2 Task 12: Array type

            // Spatial types (Type Integration Phase 3)
            KW_POINT,
            KW_LINESTRING,
            KW_POLYGON,
            KW_MULTIPOINT,
            KW_MULTILINESTRING,
            KW_MULTIPOLYGON,
            KW_GEOMETRYCOLLECTION,

            // Range types (Task 15 Phase 4)
            KW_INT4RANGE,
            KW_INT8RANGE,
            KW_NUMRANGE,
            KW_DATERANGE,
            KW_TSRANGE,
            KW_TSTZRANGE,

            // JSON functions (Phase 1 Task 7)
            KW_JSON_EXTRACT,
            KW_JSON_OBJECT,
            KW_JSON_ARRAY,
            KW_JSON_SET,
            KW_JSON_INSERT,
            KW_JSON_REMOVE,
            KW_JSONB_EXTRACT_PATH,
            KW_JSONB_BUILD_OBJECT,
            KW_JSONB_BUILD_ARRAY,
            KW_JSONB_SET,

            // Conditional functions (Phase 1 Task 8)
            KW_COALESCE,
            KW_NULLIF,
            KW_CASE,
            KW_WHEN,
            KW_THEN,
            KW_ELSE,
            KW_END,

            // Array functions (Phase 2 Task 12)
            KW_ARRAY_TO_STRING,
            KW_STRING_TO_ARRAY,
            KW_ARRAY_APPEND,
            KW_ARRAY_PREPEND,
            KW_ARRAY_CAT,
            KW_ARRAY_REMOVE,
            KW_ARRAY_REPLACE,
            KW_ARRAY_LENGTH,
            KW_ARRAY_DIMS,
            KW_ARRAY_UPPER,
            KW_ARRAY_LOWER,
            KW_UNNEST,

            // Type conversion
            KW_CAST,
            KW_TRY_CAST,
            KW_AS,

            // Extraction and text functions with special syntax
            KW_EXTRACT,
            KW_POSITION,   // POSITION(substring IN string)
            KW_OVERLAY,    // OVERLAY(string PLACING replacement FROM start [FOR length])

            // Pattern matching
            KW_LIKE,
            KW_ILIKE,

            // Character set and collation
            KW_SET,
            KW_COLLATE,
            KW_COLLATION,
            KW_DEFAULT,

            // Timezone
            KW_ZONE,
            KW_WITH,
            KW_RECURSIVE, // WITH RECURSIVE for recursive CTEs
            KW_WITHOUT,
            KW_AT,

            // Transaction control (Phase 2 Task 2.6, Phase 3 Task 3.6)
            KW_START,
            KW_TRANSACTION,
            KW_COMMIT,
            KW_ROLLBACK,
            KW_READ,
            KW_WRITE,
            KW_ONLY,
            KW_WAIT,
            KW_ISOLATION,
            KW_LEVEL,
            KW_COMMITTED,
            KW_SNAPSHOT,
            KW_STABILITY,
            KW_RESERVING,
            KW_SHARED,
            KW_PROTECTED,
            KW_FOR,
            KW_PLACING,    // OVERLAY...PLACING syntax
            KW_OUTSTANDING,
            KW_LOCK,
            KW_TIMEOUT,

            // Database maintenance (Phase 3 Task 3.3)
            KW_SWEEP,
            KW_DATABASE,

            // Tablespace management (Phase 2 Task 2.1, 2.2)
            KW_TABLESPACE,
            KW_LOCATION,
            KW_AUTOEXTEND,
            KW_AUTOEXTEND_SIZE,
            KW_MAXSIZE,
            KW_UNLIMITED,
            KW_PREALLOC,
            KW_ENABLE,       // Security Phase 3.4: ENABLE ROW LEVEL SECURITY
            KW_DISABLE,      // Security Phase 3.4: DISABLE ROW LEVEL SECURITY
            KW_FORCE,
            KW_CASCADE,      // ALPHA Phase 1 - DDL Modifications
            KW_RESTRICT,     // ALPHA Phase 1 - DDL Modifications
            KW_DROP,
            KW_TRUNCATE,  // ALPHA Phase 1 - TRUNCATE TABLE
            KW_ASYNC,     // ALPHA Phase 1 - TRUNCATE TABLE ASYNC
            KW_SYNC,      // ALPHA Phase 1 - TRUNCATE TABLE SYNC
            KW_SEQUENCE,  // ALPHA Phase 1 - Sequences
            KW_INCREMENT, // ALPHA Phase 1 - Sequences
            KW_MINVALUE,  // ALPHA Phase 1 - Sequences
            KW_MAXVALUE,  // ALPHA Phase 1 - Sequences
            KW_NO,        // ALPHA Phase 1 - Sequences (NO MINVALUE, NO MAXVALUE, NO CYCLE)
            KW_CACHE,     // ALPHA Phase 1 - Sequences
            KW_CYCLE,     // ALPHA Phase 1 - Sequences
            KW_RESTART,   // ALPHA Phase 1 - Sequences
            KW_NEXTVAL,      // ALPHA Phase 1 - Sequences (function)
            KW_CURRVAL,      // ALPHA Phase 1 - Sequences (function)
            KW_SETVAL,       // ALPHA Phase 1 - Sequences (function)
            KW_VIEW,         // ALPHA Phase 1 - Views
            KW_REPLACE,      // ALPHA Phase 1 - Views (CREATE OR REPLACE)
            KW_MATERIALIZED, // ALPHA Phase 1 - Materialized Views
            KW_REFRESH,      // ALPHA Phase 1 - Materialized Views (REFRESH MATERIALIZED VIEW)
            KW_CONCURRENTLY, // ALPHA Phase 1 - Materialized Views (REFRESH CONCURRENTLY)
            KW_CHECK,        // ALPHA Phase 1 - Views (WITH CHECK OPTION)
            KW_OPTION,       // ALPHA Phase 1 - Views (WITH CHECK OPTION)
            KW_ADMIN,        // WP-6 PARSE-L1: WITH ADMIN OPTION for GRANT ROLE
            KW_ON,
            KW_OFF,
            KW_ALTER,  // Phase 2 Task 2.2
            KW_RENAME, // Phase 2 Task 2.2
            KW_TO,     // Phase 2 Task 2.2
            KW_ADD,    // ALPHA Phase 1 - ALTER TABLE ADD COLUMN
            KW_TYPE,   // ALPHA Phase 1 - ALTER TABLE ALTER COLUMN TYPE
            KW_CONSTRAINT,  // ALPHA Phase C - Table-level constraints
            KW_CONSTRAINTS, // P2-7: SET CONSTRAINTS statement (plural)
            KW_IDENTITY,   // ALPHA Phase 1 - IDENTITY columns (auto-increment)
            KW_GENERATED,  // ALPHA Phase 1 - GENERATED columns
            KW_ALWAYS,     // ALPHA Phase 1 - GENERATED ALWAYS AS IDENTITY
            KW_STORED,     // ALPHA Phase 1 - GENERATED ALWAYS AS ... STORED
            KW_VIRTUAL,    // ALPHA Phase 1 - GENERATED ALWAYS AS ... VIRTUAL
            KW_DEFERRABLE, // ALPHA Phase 1 - Deferred constraint checking
            KW_INITIALLY,  // ALPHA Phase 1 - INITIALLY DEFERRED/IMMEDIATE
            KW_DEFERRED,   // ALPHA Phase 1 - INITIALLY DEFERRED
            KW_IMMEDIATE,  // ALPHA Phase 1 - INITIALLY IMMEDIATE
            // KW_BY already defined above for GROUP BY/ORDER BY
            KW_FOREIGN,    // ALPHA Phase C - Foreign key constraints
            KW_KEY,        // ALPHA Phase C - Foreign/primary key
            KW_PRIMARY,    // ALPHA Phase C - Primary key constraints
            KW_ONLINE, // Phase 4 Task 4.1.1 - ALTER TABLE ... SET TABLESPACE ... ONLINE
            KW_ATTACH, // Phase 6 Task 6.1 - ATTACH TABLESPACE
            KW_DETACH, // Phase 6 Task 6.2 - DETACH TABLESPACE

            // Subquery keywords (Phase 2 Wave 2 - Agent B)
            KW_IN,
            KW_EXISTS,

            // Trigger keywords (Phase 2 Wave 2 - Agent C)
            KW_TRIGGER,
            KW_BEFORE,
            KW_AFTER,
            KW_EXECUTE,
            KW_PROCEDURE,
            KW_OLD,
            KW_NEW,

            // Database trigger keywords (Firebird-style)
            KW_DISCONNECT,    // ON DISCONNECT database trigger
            KW_ACTIVE,        // CREATE TRIGGER name ACTIVE
            KW_INACTIVE,      // CREATE TRIGGER name INACTIVE

            // Stored procedure keywords (Phase 2 Task 10.2)
            KW_FUNCTION,
            KW_RETURNS,
            KW_LANGUAGE,
            KW_BEGIN,
            // KW_END, - already defined for CASE
            KW_DECLARE,
            KW_RETURN,
            KW_IF,
            // KW_THEN, - already defined for CASE
            // KW_ELSE, - already defined for CASE
            KW_ELSIF,
            KW_ENDIF,
            KW_LOOP,
            KW_WHILE,
            KW_ENDLOOP,
            KW_EXIT,
            // KW_WHEN, - already defined for CASE
            KW_RAISE,
            KW_EXCEPTION,
            KW_TRY,
            KW_EXCEPT,

            // Security keywords (ALPHA Phase 1 - Security System Phase 2)
            KW_USER,          // CREATE USER, ALTER USER, DROP USER
            KW_ROLE,          // CREATE ROLE, DROP ROLE, SET ROLE, GRANT ROLE
            // KW_GROUP already defined (line 108)
            KW_GRANT,         // GRANT privilege/role
            KW_REVOKE,        // REVOKE privilege/role
            KW_PRIVILEGES,    // GRANT SELECT, INSERT, ... PRIVILEGES ON
            KW_PASSWORD,      // CREATE USER ... WITH PASSWORD
            KW_SUPERUSER,     // CREATE USER ... SUPERUSER
            KW_NOSUPERUSER,   // CREATE USER ... NOSUPERUSER
            KW_SESSION,       // SET SESSION AUTHORIZATION, RESET SESSION AUTHORIZATION
            KW_AUTHORIZATION, // SET SESSION AUTHORIZATION
            KW_RESET,         // RESET ROLE, RESET SESSION AUTHORIZATION
            KW_PUBLIC,        // GRANT ... TO PUBLIC
            KW_USAGE,         // GRANT USAGE ON SCHEMA
            KW_CONNECT,       // GRANT CONNECT ON DATABASE
            KW_REFERENCES,    // GRANT REFERENCES ON TABLE
            KW_POLICY,        // Security Phase 3.4: CREATE/DROP POLICY
            // KW_ENABLE already defined (line 298)
            // KW_DISABLE already defined (line 299)
            // KW_FORCE already defined (line 300)
            KW_SQL,           // Security Phase 3.1: SQL SECURITY DEFINER/INVOKER
            KW_SECURITY,      // Security Phase 3.4: ROW LEVEL SECURITY
            KW_DEFINER,       // Security Phase 3.1: SQL SECURITY DEFINER
            KW_INVOKER,       // Security Phase 3.1: SQL SECURITY INVOKER
            // KW_LEVEL already defined (line 274)
            // KW_ROW already defined (line 137)
            // KW_RESTRICT already defined (line 300)

            // SQL Engine Commands (ALPHA Phase 1 - Developer Experience)
            KW_SHOW,          // SHOW TABLES, SHOW DATABASES, etc.
            KW_DESCRIBE,      // DESCRIBE table (alias for SHOW COLUMNS FROM table)
            // KW_DESC already defined (line 117) - used for ORDER BY DESC, also works for DESCRIBE shorthand
            KW_TABLES,        // SHOW TABLES
            KW_DATABASES,     // SHOW DATABASES
            KW_SCHEMAS,       // SHOW SCHEMAS
            KW_COLUMNS,       // SHOW COLUMNS FROM table
            KW_INDEXES,       // SHOW INDEXES FROM table

            // UPSERT/ON CONFLICT (INSERT ... ON CONFLICT)
            KW_CONFLICT,      // ON CONFLICT clause
            KW_DO,            // DO UPDATE / DO NOTHING
            KW_NOTHING,       // DO NOTHING action
            KW_EXCLUDED,      // EXCLUDED pseudo-table in ON CONFLICT DO UPDATE

            // LATERAL JOIN
            KW_LATERAL,       // LATERAL keyword for correlated subqueries in FROM

            // STRING_AGG aggregate function
            KW_STRING_AGG,    // STRING_AGG(expression, delimiter)
            KW_GROUP_CONCAT,  // GROUP_CONCAT (MySQL alias for STRING_AGG)
            KW_WITHIN,        // WITHIN GROUP (ORDER BY ...) clause

            // FILTER clause for aggregates
            KW_FILTER,        // FILTER (WHERE condition) clause

            // NTILE window function
            KW_NTILE,         // NTILE(n) window function

            // SAVEPOINT transaction control
            KW_SAVEPOINT,     // SAVEPOINT name
            KW_RELEASE,       // RELEASE SAVEPOINT name

            // User Defined Types
            KW_COMPOSITE,     // CREATE TYPE ... AS (composite type)
            KW_ENUM,          // CREATE TYPE ... AS ENUM
            KW_DOMAIN,        // CREATE DOMAIN (domain type)
            // Note: KW_RANGE already exists for window frame mode (line 147)
            KW_SUBTYPE,       // SUBTYPE = typename in RANGE definition

            // Procedure invocation
            KW_CALL,          // CALL procedure_name(args...)

            // Extended SHOW/SET keywords (Firebird ISQL compatibility)
            KW_DIALECT,       // SET SQL DIALECT / SHOW SQL DIALECT
            KW_GENERATOR,     // SHOW GENERATOR (alias for SEQUENCE)
            KW_DEPENDENCIES,  // SHOW DEPENDENCIES
            KW_COLLATIONS,    // SHOW COLLATIONS
            KW_COMMENTS,      // SHOW COMMENTS
            KW_CHECKS,        // SHOW CHECKS
            KW_GRANTS,        // SHOW GRANTS
            KW_SYSTEM,        // SHOW SYSTEM
            KW_PACKAGE,       // SHOW PACKAGE
            KW_NAMES,         // SET NAMES charset
            KW_LOCAL_TIMEOUT, // SET LOCAL_TIMEOUT N
            KW_PARSER,        // SET PARSER VERSION (Phase 10)
            KW_SCHEMA,        // SHOW SCHEMA (singular - specific schema)
            KW_VERSION,       // SHOW VERSION

            // Schema navigation keywords
            KW_PATH,          // SHOW SCHEMA PATH, SET SEARCH PATH, IN PATH
            KW_TREE,          // SHOW SCHEMA TREE
            KW_DEPTH,         // SHOW SCHEMA TREE DEPTH n
            KW_SEARCH,        // SET SEARCH PATH, SHOW SEARCH PATH
            // KW_LOCATION already defined above for tablespace, reused for SHOW LOCATION OF
            KW_OF,            // SHOW LOCATION OF
            KW_RESOLVED,      // SHOW RESOLVED name
            KW_OBJECTS,       // SHOW OBJECTS
            KW_DETAIL,        // IN DETAIL
            KW_HOME,          // SET SCHEMA HOME
            KW_ROOT,          // SET SCHEMA ROOT
            KW_UP,            // SET SCHEMA UP
        };

        // Location in source file
        struct SourceLocation
        {
            uint32_t line;
            uint32_t column;
            uint32_t offset; // Byte offset in source

            SourceLocation() : line(1), column(1), offset(0) {}
            SourceLocation(uint32_t l, uint32_t c, uint32_t o) : line(l), column(c), offset(o) {}
        };

        // String pool for interning
        class StringPool
        {
        public:
            using StringId = uint32_t;

            StringId intern(std::string_view str);
            std::string_view get(StringId id) const;
            void clear();

        private:
            std::vector<std::string> strings_;
            std::unordered_map<std::string_view, StringId> lookup_;
        };

        // Token structure
        struct Token
        {
            TokenType type;
            SourceLocation location;
            uint32_t length; // Length of token text

            // Firebird SQL identifier handling:
            // - Unquoted identifiers: case-insensitive (stored as-is, compared UPPER)
            // - Quoted "Identifiers": case-sensitive (delimited)
            bool is_delimited = false;  // True if identifier was double-quoted

            union
            {
                int64_t int_value;              // For INTEGER_LITERAL
                double float_value;             // For FLOAT_LITERAL
                StringPool::StringId string_id; // For IDENTIFIER, STRING_LITERAL
                uint8_t keyword_code;           // For specific keywords
            } value;

            // Default constructor - initializes to safe state
            Token() : type(TokenType::ERROR), location(), length(0), is_delimited(false)
            {
                value.int_value = 0; // Initialize union to zero
            }

            // Constructor helpers
            static Token makeEOF(const SourceLocation &loc)
            {
                Token t;
                t.type = TokenType::END_OF_FILE;
                t.location = loc;
                t.length = 0;
                t.value.int_value = 0; // Initialize union
                return t;
            }

            static Token makeError(const SourceLocation &loc, uint32_t len)
            {
                Token t;
                t.type = TokenType::ERROR;
                t.location = loc;
                t.length = len;
                t.value.int_value = 0; // Initialize union
                return t;
            }

            static Token makeInteger(const SourceLocation &loc, uint32_t len, int64_t val)
            {
                Token t;
                t.type = TokenType::INTEGER_LITERAL;
                t.location = loc;
                t.length = len;
                t.value.int_value = val;
                return t;
            }

            static Token makeFloat(const SourceLocation &loc, uint32_t len, double val)
            {
                Token t;
                t.type = TokenType::FLOAT_LITERAL;
                t.location = loc;
                t.length = len;
                t.value.float_value = val;
                return t;
            }

            static Token makeString(const SourceLocation &loc, uint32_t len,
                                    StringPool::StringId id)
            {
                Token t;
                t.type = TokenType::STRING_LITERAL;
                t.location = loc;
                t.length = len;
                t.value.string_id = id;
                return t;
            }

            static Token makeIdentifier(const SourceLocation &loc, uint32_t len,
                                        StringPool::StringId id, bool delimited = false)
            {
                Token t;
                t.type = TokenType::IDENTIFIER;
                t.location = loc;
                t.length = len;
                t.is_delimited = delimited;
                t.value.string_id = id;
                return t;
            }

            static Token makeKeyword(const SourceLocation &loc, uint32_t len, TokenType kwType)
            {
                Token t;
                t.type = kwType;
                t.location = loc;
                t.length = len;
                t.value.int_value = 0; // Initialize union
                return t;
            }

            static Token makeOperator(const SourceLocation &loc, uint32_t len, TokenType opType)
            {
                Token t;
                t.type = opType;
                t.location = loc;
                t.length = len;
                t.value.int_value = 0; // Initialize union
                return t;
            }
        };

        // Convert token type to string for debugging
        const char *tokenTypeToString(TokenType type);

    } // namespace parser
} // namespace scratchbird