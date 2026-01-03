#pragma once

/**
 * MySQL Parser
 *
 * Recursive-descent parser for MySQL 8.0 SQL dialect.
 * Unlike the ScratchBird V2 parser which produces AST nodes,
 * this parser generates SBLR bytecode directly for execution.
 *
 * Schema: Databases are emulated as schemas at:
 *   /remote/emulated/mysql/localhost/{database}/
 *
 * Supported statements:
 * - DDL: CREATE/ALTER/DROP TABLE, INDEX, VIEW, DATABASE
 * - DML: SELECT, INSERT, UPDATE, DELETE, REPLACE
 * - Transaction: BEGIN, COMMIT, ROLLBACK, SAVEPOINT
 * - Admin: SHOW, DESCRIBE, USE, SET
 * - Stored programs: CREATE PROCEDURE/FUNCTION (limited)
 */

#include "mysql_lexer.h"
#include <scratchbird/sblr/opcodes.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace scratchbird {
namespace core {
class Database;
}
}

namespace scratchbird::parser::mysql {

/**
 * Parse error with location information
 */
struct ParseError {
    std::string message;
    SourceLocation location;

    ParseError(std::string msg, SourceLocation loc)
        : message(std::move(msg)), location(loc) {}
};

/**
 * Compilation/parse result containing bytecode or errors
 */
class ParseResult {
public:
    ParseResult() = default;

    bool success() const { return errors_.empty(); }

    const std::vector<ParseError>& errors() const { return errors_; }

    void addError(const std::string& message, SourceLocation loc) {
        errors_.emplace_back(message, loc);
    }

    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    std::vector<uint8_t>& bytecode() { return bytecode_; }

    void setBytecode(std::vector<uint8_t> bc) {
        bytecode_ = std::move(bc);
    }

private:
    std::vector<ParseError> errors_;
    std::vector<uint8_t> bytecode_;
};

/**
 * MySQL data type representation
 */
struct MySQLDataType {
    enum class Kind {
        // Integer types
        TINYINT, SMALLINT, MEDIUMINT, INT, BIGINT,
        // Floating point
        FLOAT, DOUBLE, DECIMAL,
        // String types
        CHAR, VARCHAR, TEXT, TINYTEXT, MEDIUMTEXT, LONGTEXT,
        // Binary types
        BINARY, VARBINARY, BLOB, TINYBLOB, MEDIUMBLOB, LONGBLOB,
        // Date/Time types
        DATE, TIME, DATETIME, TIMESTAMP, YEAR,
        // Other types
        BIT, BOOL, ENUM, SET, JSON, GEOMETRY, POINT, LINESTRING, POLYGON
    };

    Kind kind;
    int length = 0;         // For CHAR, VARCHAR, etc.
    int precision = 0;      // For DECIMAL
    int scale = 0;          // For DECIMAL
    bool unsigned_ = false;
    bool zerofill = false;
    bool nullable = true;
    std::string charset;
    std::string collation;
    std::vector<std::string> enum_values;  // For ENUM/SET

    MySQLDataType() : kind(Kind::INT) {}
    explicit MySQLDataType(Kind k) : kind(k) {}
};

/**
 * Column definition for CREATE TABLE
 */
struct ColumnDef {
    std::string name;
    MySQLDataType type;
    bool primary_key = false;
    bool unique = false;
    bool auto_increment = false;
    bool has_default = false;
    std::string default_value;
    bool default_is_null = false;
    bool default_is_expr = false;
    std::string comment;
};

/**
 * Index definition for CREATE TABLE/INDEX
 */
struct IndexDef {
    enum class Type { NORMAL, UNIQUE, PRIMARY, FULLTEXT, SPATIAL };
    Type type = Type::NORMAL;
    std::string name;
    std::vector<std::string> columns;
    std::vector<int> column_lengths;  // Prefix lengths for text columns
    std::string algorithm;  // BTREE, HASH
    std::string comment;
};

/**
 * Foreign key definition
 */
struct ForeignKeyDef {
    std::string name;
    std::vector<std::string> columns;
    std::string ref_table;
    std::vector<std::string> ref_columns;
    std::string on_delete;  // CASCADE, SET NULL, etc.
    std::string on_update;
};

/**
 * Expression types for SELECT, WHERE, etc.
 */
enum class ExprType {
    LITERAL_INT,
    LITERAL_FLOAT,
    LITERAL_STRING,
    LITERAL_NULL,
    LITERAL_BOOL,
    COLUMN_REF,
    BINARY_OP,
    UNARY_OP,
    FUNCTION_CALL,
    CASE_EXPR,
    SUBQUERY,
    EXISTS_EXPR,
    IN_EXPR,
    BETWEEN_EXPR,
    LIKE_EXPR,
    IS_NULL_EXPR,
    CAST_EXPR,
    AGGREGATE_CALL,
    WINDOW_CALL,
    USER_VARIABLE,
    SYSTEM_VARIABLE,
    PLACEHOLDER
};

/**
 * Forward declarations for expression tree
 */
struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

/**
 * Expression node (simplified for SBLR generation)
 */
struct Expr {
    ExprType type;

    // For literals
    int64_t int_value = 0;
    double float_value = 0.0;
    std::string string_value;
    bool bool_value = false;

    // For column references: schema.table.column
    std::string schema;
    std::string table;
    std::string column;

    // For binary/unary ops
    std::string op;
    ExprPtr left;
    ExprPtr right;

    // For function calls
    std::string function_name;
    std::vector<ExprPtr> args;
    bool distinct = false;  // For aggregates

    // For CASE
    ExprPtr case_value;
    std::vector<std::pair<ExprPtr, ExprPtr>> when_clauses;
    ExprPtr else_clause;

    // For window functions
    std::vector<ExprPtr> partition_by;
    std::vector<std::pair<ExprPtr, bool>> order_by;  // expr, is_desc

    Expr() : type(ExprType::LITERAL_NULL) {}
    explicit Expr(ExprType t) : type(t) {}
};

/**
 * Table reference for FROM clause
 */
struct TableRef {
    enum class Type { TABLE, SUBQUERY, JOIN };
    Type type;
    std::string schema;
    std::string table;
    std::string alias;

    // For joins
    std::string join_type;  // INNER, LEFT, RIGHT, CROSS
    std::unique_ptr<TableRef> left;
    std::unique_ptr<TableRef> right;
    ExprPtr join_condition;
    std::vector<std::string> using_columns;

    // For subqueries
    std::unique_ptr<struct SelectStmt> subquery;

    TableRef() : type(Type::TABLE) {}
};

/**
 * SELECT statement structure
 */
struct SelectStmt {
    bool distinct = false;
    std::vector<std::pair<ExprPtr, std::string>> select_list;  // expr, alias
    std::unique_ptr<TableRef> from;
    ExprPtr where;
    std::vector<ExprPtr> group_by;
    ExprPtr having;
    std::vector<std::pair<ExprPtr, bool>> order_by;  // expr, is_desc
    int64_t limit = -1;
    int64_t offset = 0;
    bool for_update = false;

    // UNION support
    std::unique_ptr<SelectStmt> union_stmt;
    bool union_all = false;
};

/**
 * MySQL Parser
 *
 * Parses MySQL SQL and generates SBLR bytecode for execution.
 */
class Parser {
public:
    /**
     * Create a parser for the given SQL input.
     * @param input The SQL statement(s) to parse
     * @param db The database context (for schema resolution)
     * @param default_schema Default schema path for unqualified tables
     */
    Parser(std::string_view input,
           core::Database* db = nullptr,
           std::string_view default_schema = "/remote/emulated/mysql/localhost/");

    ~Parser() = default;

    // Non-copyable
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    /**
     * Parse and compile a single statement.
     * Returns bytecode ready for execution.
     */
    ParseResult parseStatement();

    /**
     * Parse all statements in the input.
     * Useful for multi-statement queries.
     */
    std::vector<ParseResult> parseAll();

    /**
     * Get the lexer's string pool.
     */
    StringPool& stringPool() { return lexer_.stringPool(); }
    const StringPool& stringPool() const { return lexer_.stringPool(); }

private:
    Lexer lexer_;
    core::Database* db_;
    std::string default_schema_;
    Token current_token_;
    std::vector<uint8_t> bytecode_;
    std::vector<ParseError> errors_;
    uint32_t next_placeholder_index_ = 1;

    // Token management
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchKeyword(TokenType kw);
    bool matchIdentifierKeyword(const char* keyword);
    Token consume(TokenType type, const std::string& message);
    Token consumeKeyword(TokenType kw, const std::string& message);

    // Error handling
    void error(const std::string& message);
    void synchronize();

    // Bytecode emission helpers
    void emit(sblr::Opcode op);
    void emitByte(uint8_t byte);
    void emitU16(uint16_t val);
    void emitU32(uint32_t val);
    void emitU64(uint64_t val);
    void emitI64(int64_t val);
    void emitF64(double val);
    void emitString(std::string_view str);

    // Statement parsing
    void parseStatementInternal();
    void parseSelectStmt();
    void parseInsertStmt();
    void parseUpdateStmt();
    void parseDeleteStmt();
    void parseReplaceStmt();
    void parseCreateStmt();
    void parseRenameStmt();
    void parseAlterStmt();
    void parseDropStmt();
    void parseTruncateStmt();
    void parseSetStmt();
    void parseShowStmt();
    void parseDescribeStmt();
    void parseUseStmt();
    void parseBeginStmt();
    void parseCommitStmt();
    void parseRollbackStmt();
    void parseSavepointStmt();
    void parseReleaseStmt();
    void parseLockStmt();
    void parseUnlockStmt();

    // DDL parsing
    void parseCreateTable();
    void parseCreateIndex();
    void parseCreateView();
    void parseCreateDatabase();
    void parseCreateProcedure();
    void parseCreateFunction();
    void parseCreateTrigger();
    ColumnDef parseColumnDef();
    MySQLDataType parseDataType();
    IndexDef parseIndexDef();
    ForeignKeyDef parseForeignKeyDef();

    // DML clause parsing
    void parseSelectList();
    void parseFromClause();
    void parseWhereClause();
    void parseGroupByClause();
    void parseHavingClause();
    void parseOrderByClause();
    void parseLimitClause();

    // Expression parsing (generates bytecode)
    void parseExpression();
    void parseOrExpr();
    void parseXorExpr();
    void parseAndExpr();
    void parseNotExpr();
    void parseComparisonExpr();
    void parseBitwiseOrExpr();
    void parseBitwiseXorExpr();
    void parseBitwiseAndExpr();
    void parseShiftExpr();
    void parseAdditiveExpr();
    void parseMultiplicativeExpr();
    void parseUnaryExpr();
    void parsePrimaryExpr();
    void parseFunctionCall(const std::string& name);
    void parseCaseExpr();
    void parseCastExpr();
    void parseSubquery();

    // Table reference parsing
    std::unique_ptr<TableRef> parseTableRef();
    std::unique_ptr<TableRef> parseJoinClause(std::unique_ptr<TableRef> left);

    // Type conversion helpers
    sblr::Opcode typeToOpcode(MySQLDataType::Kind kind);

    // Identifier helpers
    std::string parseIdentifier();
    std::string parseQualifiedName();
    void resolveTableName(std::string& schema, std::string& table);
};

} // namespace scratchbird::parser::mysql
