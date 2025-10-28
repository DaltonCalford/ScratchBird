#pragma once

#include "scratchbird/parser/token.h"
#include "scratchbird/core/types.h"
#include <memory>
#include <vector>
#include <string>
#include <variant>

namespace scratchbird
{
    namespace parser
    {

        // Forward declarations
        class ASTVisitor;

        // Source span for error reporting
        struct SourceSpan
        {
            SourceLocation start;
            SourceLocation end;

            SourceSpan() = default;
            SourceSpan(const SourceLocation &s, const SourceLocation &e) : start(s), end(e) {}
        };

        // AST Node kinds
        enum class ASTKind : uint8_t
        {
            // Statements
            CREATE_TABLE,
            CREATE_INDEX,              // Phase 2 Task 2.3
            CREATE_TRIGGER,            // Phase 2 Wave 2 - Agent C: Basic Triggers
            DROP_TRIGGER,              // Phase 2 Wave 2 - Agent C: Basic Triggers
            CREATE_TABLESPACE,         // Phase 2 Task 2.1
            ALTER_TABLESPACE,          // Phase 2 Task 2.2
            ALTER_TABLE_SET_TABLESPACE, // Phase 4 Task 4.1.1
            DROP_TABLESPACE,           // Phase 2 Task 2.1
            ATTACH_TABLESPACE,         // Phase 6 Task 6.1
            DETACH_TABLESPACE,         // Phase 6 Task 6.2
            INSERT,
            SELECT,
            UPDATE,            // Phase 1 Task 2.1: UPDATE statement
            DELETE_STMT,       // Phase 1 Task 2.2: DELETE statement (DELETE is a keyword)
            ANALYZE,           // Phase 1 Task 1.1.2: Statistics collection
            EXPLAIN,           // Phase 1 Task 1.5: EXPLAIN command
            START_TRANSACTION, // Phase 2 Task 2.6
            SET_TRANSACTION,   // Phase 3 Task 3.6
            COMMIT,            // Phase 2 Task 2.6
            ROLLBACK,          // Phase 2 Task 2.6
            SWEEP,             // Phase 3 Task 3.3

            // Expressions
            LITERAL,
            IDENTIFIER,
            BINARY_OP,
            CAST,
            FUNCTION_CALL,
            AGGREGATE_FUNC,  // Phase 1 Task 4.1: Aggregate functions
            WINDOW_FUNC,     // Phase 1 Task 6: Window functions
            WINDOW_SPEC,     // Phase 1 Task 6: Window specification (OVER clause)
            JSON_FUNC,       // Phase 1 Task 7: JSON functions
            COALESCE,        // Phase 1 Task 8: COALESCE expression
            NULLIF,          // Phase 1 Task 8: NULLIF expression
            CASE,            // Phase 1 Task 8: CASE expression
            SUBQUERY,        // Phase 2 Wave 2 - Agent B: Subquery expression

            // Types
            TYPE_NAME,

            // Misc
            COLUMN_DEF,
            TABLE_CONSTRAINT,
            SELECT_LIST,
            WHERE_CLAUSE,
        };

        // Base AST Node
        class ASTNode
        {
        public:
            ASTNode(ASTKind kind, const SourceSpan &span) : kind_(kind), span_(span) {}

            virtual ~ASTNode() = default;

            ASTKind kind() const
            {
                return kind_;
            }
            const SourceSpan &span() const
            {
                return span_;
            }

            // Visitor pattern
            virtual void accept(ASTVisitor *visitor) = 0;

        protected:
            ASTKind kind_;
            SourceSpan span_;
        };

        // Arena allocator for AST nodes
        class ASTArena
        {
        public:
            ASTArena();
            ~ASTArena();

            template <typename T, typename... Args> T *make(Args &&...args)
            {
                void *ptr = allocate(sizeof(T));
                T *obj = new (ptr) T(std::forward<Args>(args)...);

                // Track objects that need destruction if they have non-trivial destructors
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    registerDestructor(obj, [](void *p) { static_cast<T *>(p)->~T(); });
                }

                return obj;
            }

            void reset();

        private:
            struct Block
            {
                std::unique_ptr<uint8_t[]> data;
                size_t size;
                size_t used;
            };

            std::vector<Block> blocks_;
            static constexpr size_t BLOCK_SIZE = 64 * 1024; // 64KB blocks

            // Track objects that need destructors called
            std::vector<std::pair<void *, void (*)(void *)>> destructors_;

            void *allocate(size_t size);
            void registerDestructor(void *obj, void (*destructor)(void *));
        };

        // Data types - now using unified type system
        using DataType = core::DataType;
        using TypeInfo = core::TypeInfo;

        // For backward compatibility during migration
        struct TypeName
        {
            DataType type;
            uint32_t precision;     // For VARCHAR(n), CHAR(n)
            uint32_t scale;         // For DECIMAL(p,s)
            bool with_timezone;     // For TIMESTAMP WITH TIME ZONE
            uint16_t timezone_hint; // Timezone ID for display

            TypeName(DataType t, uint32_t p = 0, uint32_t s = 0, bool tz = false,
                     uint16_t tz_hint = 0)
                : type(t), precision(p), scale(s), with_timezone(tz), timezone_hint(tz_hint)
            {
            }

            // Convert to TypeInfo
            TypeInfo toTypeInfo() const
            {
                TypeInfo info(type, precision, scale);
                info.with_timezone = with_timezone;
                info.timezone_hint = timezone_hint;
                return info;
            }
        };

        // ===== Expression Nodes =====

        class Expression : public ASTNode
        {
        public:
            using ASTNode::ASTNode;
        };

        // Literal value
        class LiteralExpr : public Expression
        {
        public:
            enum LiteralType
            {
                INTEGER,
                FLOAT,
                STRING,
                NULL_LITERAL
            };

            LiteralExpr(const SourceSpan &span, LiteralType type)
                : Expression(ASTKind::LITERAL, span), literal_type_(type), int_value_(0)
            {
                // Initialize union to a safe default (int_value_ = 0)
                // Caller must use appropriate setter for the actual type
            }

            LiteralType literalType() const
            {
                return literal_type_;
            }

            // Value accessors
            int64_t intValue() const
            {
                return int_value_;
            }
            double floatValue() const
            {
                return float_value_;
            }
            StringPool::StringId stringValue() const
            {
                return string_value_;
            }

            // Value setters
            void setIntValue(int64_t v)
            {
                int_value_ = v;
            }
            void setFloatValue(double v)
            {
                float_value_ = v;
            }
            void setStringValue(StringPool::StringId v)
            {
                string_value_ = v;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            LiteralType literal_type_;
            union
            {
                int64_t int_value_;
                double float_value_;
                StringPool::StringId string_value_;
            };
        };

        // Identifier
        class IdentifierExpr : public Expression
        {
        public:
            // Simple identifier (column name only)
            IdentifierExpr(const SourceSpan &span, StringPool::StringId name)
                : Expression(ASTKind::IDENTIFIER, span), name_(name), qualifier_(0)
            {
            }

            // Qualified identifier (table.column or alias.column) - Phase 1 Task 3.1
            IdentifierExpr(const SourceSpan &span, StringPool::StringId qualifier, StringPool::StringId name)
                : Expression(ASTKind::IDENTIFIER, span), name_(name), qualifier_(qualifier)
            {
            }

            StringPool::StringId name() const
            {
                return name_;
            }

            StringPool::StringId qualifier() const
            {
                return qualifier_;
            }

            bool isQualified() const
            {
                return qualifier_ != 0;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;       // Column name
            StringPool::StringId qualifier_;  // Optional table name or alias
        };

        // Binary operation
        enum class BinaryOp : uint8_t
        {
            ADD,
            SUBTRACT,
            MULTIPLY,
            DIVIDE,
            MODULO,
            EQ,
            NE,
            LT,
            GT,
            LE,
            GE,
            AND,
            OR,
            LIKE,
            ILIKE,
            IN,          // Phase 2 Wave 2 - Agent B: Subquery IN operator
            NOT_IN,      // Phase 2 Wave 2 - Agent B: Subquery NOT IN operator
            // Array operators (Phase 2 Task 12)
            ARRAY_OVERLAP,      // && - array overlap
            ARRAY_CONTAINS,     // @> - array contains
            ARRAY_CONTAINED_BY, // <@ - array contained by
            // Regex operators (Phase 2 Task 13)
            REGEX_MATCH,        // ~ - regex match
            REGEX_MATCH_CI,     // ~* - regex match case-insensitive
            REGEX_NOT_MATCH,    // !~ - regex not match
            REGEX_NOT_MATCH_CI  // !~* - regex not match case-insensitive
        };

        class BinaryOpExpr : public Expression
        {
        public:
            BinaryOpExpr(const SourceSpan &span, BinaryOp op, Expression *left, Expression *right)
                : Expression(ASTKind::BINARY_OP, span), op_(op), left_(left), right_(right)
            {
            }

            BinaryOp op() const
            {
                return op_;
            }
            Expression *left() const
            {
                return left_;
            }
            Expression *right() const
            {
                return right_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            BinaryOp op_;
            Expression *left_;
            Expression *right_;
        };

        // CAST expression: CAST(expr AS type) or TRY_CAST(expr AS type)
        class CastExpr : public Expression
        {
        public:
            CastExpr(const SourceSpan &span, Expression *expr, const TypeName &target_type,
                     bool is_try_cast = false)
                : Expression(ASTKind::CAST, span), expr_(expr), target_type_(target_type),
                  is_try_cast_(is_try_cast)
            {
            }

            Expression *expr() const
            {
                return expr_;
            }
            const TypeName &targetType() const
            {
                return target_type_;
            }
            bool isTryCast() const
            {
                return is_try_cast_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            Expression *expr_;
            TypeName target_type_;
            bool is_try_cast_;
        };

        // Function call: func_name(arg1, arg2, ...)
        class FunctionCallExpr : public Expression
        {
        public:
            FunctionCallExpr(const SourceSpan &span, StringPool::StringId name,
                             std::vector<Expression *> args)
                : Expression(ASTKind::FUNCTION_CALL, span), name_(name), args_(std::move(args))
            {
            }

            StringPool::StringId name() const
            {
                return name_;
            }
            const std::vector<Expression *> &args() const
            {
                return args_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;
            std::vector<Expression *> args_;
        };

        // Aggregate function types (Phase 1 Task 4.1, Phase 2 Task 12)
        enum class AggregateFunc : uint8_t
        {
            COUNT,
            SUM,
            AVG,
            MIN,
            MAX,
            ARRAY_AGG  // Phase 2 Task 12
        };

        // Aggregate function expression (Phase 1 Task 4.1)
        class AggregateExpr : public Expression
        {
        public:
            AggregateExpr(const SourceSpan &span, AggregateFunc func, Expression *arg, bool distinct = false)
                : Expression(ASTKind::AGGREGATE_FUNC, span), func_(func), arg_(arg), distinct_(distinct)
            {
            }

            AggregateFunc func() const
            {
                return func_;
            }
            Expression *arg() const
            {
                return arg_;
            }
            bool distinct() const
            {
                return distinct_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            AggregateFunc func_;
            Expression *arg_;     // nullptr for COUNT(*)
            bool distinct_;       // true for COUNT(DISTINCT col)
        };

        // Window function types (Phase 1 Task 6)
        enum class WindowFunc : uint8_t
        {
            ROW_NUMBER,
            RANK,
            DENSE_RANK,
            LAG,
            LEAD,
            FIRST_VALUE,
            LAST_VALUE,
            NTH_VALUE
        };

        // Window frame boundary type (Phase 1 Task 6)
        enum class FrameBoundaryType : uint8_t
        {
            UNBOUNDED_PRECEDING,
            PRECEDING,
            CURRENT_ROW,
            FOLLOWING,
            UNBOUNDED_FOLLOWING
        };

        // Window frame mode (Phase 1 Task 6)
        enum class FrameMode : uint8_t
        {
            ROWS,
            RANGE
        };

        // Window frame boundary (Phase 1 Task 6)
        struct FrameBoundary
        {
            FrameBoundaryType type;
            Expression* offset;  // For PRECEDING/FOLLOWING with offset

            FrameBoundary() : type(FrameBoundaryType::CURRENT_ROW), offset(nullptr) {}
            FrameBoundary(FrameBoundaryType t, Expression* o = nullptr) : type(t), offset(o) {}
        };

        // Window specification (OVER clause) (Phase 1 Task 6)
        class WindowSpec : public ASTNode
        {
        public:
            WindowSpec(const SourceSpan &span)
                : ASTNode(ASTKind::WINDOW_SPEC, span),
                  has_frame_(false),
                  frame_mode_(FrameMode::RANGE)
            {
            }

            // PARTITION BY
            void addPartitionBy(Expression* expr)
            {
                partition_by_.push_back(expr);
            }
            const std::vector<Expression*>& partitionBy() const
            {
                return partition_by_;
            }

            // ORDER BY
            void addOrderBy(Expression* expr, bool ascending, bool nulls_first)
            {
                order_by_.push_back(expr);
                order_ascending_.push_back(ascending);
                order_nulls_first_.push_back(nulls_first);
            }
            const std::vector<Expression*>& orderBy() const
            {
                return order_by_;
            }
            const std::vector<bool>& orderAscending() const
            {
                return order_ascending_;
            }
            const std::vector<bool>& orderNullsFirst() const
            {
                return order_nulls_first_;
            }

            // Frame clause
            void setFrame(FrameMode mode, const FrameBoundary& start, const FrameBoundary& end)
            {
                has_frame_ = true;
                frame_mode_ = mode;
                frame_start_ = start;
                frame_end_ = end;
            }
            bool hasFrame() const
            {
                return has_frame_;
            }
            FrameMode frameMode() const
            {
                return frame_mode_;
            }
            const FrameBoundary& frameStart() const
            {
                return frame_start_;
            }
            const FrameBoundary& frameEnd() const
            {
                return frame_end_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            std::vector<Expression*> partition_by_;
            std::vector<Expression*> order_by_;
            std::vector<bool> order_ascending_;
            std::vector<bool> order_nulls_first_;
            bool has_frame_;
            FrameMode frame_mode_;
            FrameBoundary frame_start_;
            FrameBoundary frame_end_;
        };

        // Window function expression (Phase 1 Task 6)
        class WindowFuncExpr : public Expression
        {
        public:
            WindowFuncExpr(const SourceSpan &span, WindowFunc func,
                          const std::vector<Expression*>& args, WindowSpec* window_spec)
                : Expression(ASTKind::WINDOW_FUNC, span),
                  func_(func),
                  args_(args),
                  window_spec_(window_spec)
            {
            }

            WindowFunc func() const
            {
                return func_;
            }
            const std::vector<Expression*>& args() const
            {
                return args_;
            }
            WindowSpec* windowSpec() const
            {
                return window_spec_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            WindowFunc func_;
            std::vector<Expression*> args_;
            WindowSpec* window_spec_;
        };

        // JSON function types (Phase 1 Task 7)
        enum class JSONFunc : uint8_t
        {
            // Extraction functions (Task 7.1)
            JSON_EXTRACT,       // JSON_EXTRACT(json, path)
            JSONB_EXTRACT_PATH, // jsonb_extract_path(jsonb, path_elem...)
            ARROW,              // json_col -> 'field' (returns JSON)
            DOUBLE_ARROW,       // json_col ->> 'field' (returns text)
            HASH_ARROW,         // json_col #> ARRAY['path', 'to', 'field'] (returns JSON)
            HASH_DOUBLE_ARROW,  // json_col #>> ARRAY['path', 'to', 'field'] (returns text)

            // Construction functions (Task 7.2)
            JSON_OBJECT,        // JSON_OBJECT('key1', val1, 'key2', val2, ...)
            JSON_ARRAY,         // JSON_ARRAY(val1, val2, ...)
            JSONB_BUILD_OBJECT, // jsonb_build_object('key1', val1, ...)
            JSONB_BUILD_ARRAY,  // jsonb_build_array(val1, val2, ...)

            // Modification functions (Task 7.3)
            JSON_SET,           // JSON_SET(json, path, value)
            JSON_INSERT,        // JSON_INSERT(json, path, value)
            JSON_REMOVE,        // JSON_REMOVE(json, path)
            JSONB_SET,          // jsonb_set(jsonb, path_array, value)
        };

        // JSON function expression (Phase 1 Task 7)
        class JSONFuncExpr : public Expression
        {
        public:
            JSONFuncExpr(const SourceSpan &span, JSONFunc func,
                        const std::vector<Expression*>& args)
                : Expression(ASTKind::JSON_FUNC, span),
                  func_(func),
                  args_(args)
            {
            }

            JSONFunc func() const
            {
                return func_;
            }
            const std::vector<Expression*>& args() const
            {
                return args_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            JSONFunc func_;
            std::vector<Expression*> args_;
        };

        // COALESCE expression (Phase 1 Task 8)
        class CoalesceExpr : public Expression
        {
        public:
            CoalesceExpr(const SourceSpan &span, const std::vector<Expression*>& args)
                : Expression(ASTKind::COALESCE, span),
                  args_(args)
            {
            }

            const std::vector<Expression*>& args() const
            {
                return args_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            std::vector<Expression*> args_;
        };

        // NULLIF expression (Phase 1 Task 8)
        class NullIfExpr : public Expression
        {
        public:
            NullIfExpr(const SourceSpan &span, Expression* expr1, Expression* expr2)
                : Expression(ASTKind::NULLIF, span),
                  expr1_(expr1),
                  expr2_(expr2)
            {
            }

            Expression* expr1() const
            {
                return expr1_;
            }
            Expression* expr2() const
            {
                return expr2_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            Expression* expr1_;
            Expression* expr2_;
        };

        // CASE expression (Phase 1 Task 8)
        class CaseExpr : public Expression
        {
        public:
            struct WhenClause
            {
                Expression* condition;  // WHEN condition
                Expression* result;     // THEN result
            };

            // Searched CASE: CASE WHEN cond THEN result ... END
            CaseExpr(const SourceSpan &span,
                     const std::vector<WhenClause>& when_clauses,
                     Expression* else_result = nullptr)
                : Expression(ASTKind::CASE, span),
                  case_operand_(nullptr),
                  when_clauses_(when_clauses),
                  else_result_(else_result)
            {
            }

            // Simple CASE: CASE expr WHEN value THEN result ... END
            CaseExpr(const SourceSpan &span,
                     Expression* case_operand,
                     const std::vector<WhenClause>& when_clauses,
                     Expression* else_result = nullptr)
                : Expression(ASTKind::CASE, span),
                  case_operand_(case_operand),
                  when_clauses_(when_clauses),
                  else_result_(else_result)
            {
            }

            bool isSimpleCase() const
            {
                return case_operand_ != nullptr;
            }
            Expression* caseOperand() const
            {
                return case_operand_;
            }
            const std::vector<WhenClause>& whenClauses() const
            {
                return when_clauses_;
            }
            Expression* elseResult() const
            {
                return else_result_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            Expression* case_operand_;  // NULL for searched CASE, non-NULL for simple CASE
            std::vector<WhenClause> when_clauses_;
            Expression* else_result_;  // Can be NULL
        };

        // Array literal expression: ARRAY[elem1, elem2, ...] (Phase 2 Task 12)
        class ArrayLiteral : public Expression
        {
        public:
            ArrayLiteral(const SourceSpan &span, const std::vector<Expression*>& elements)
                : Expression(ASTKind::LITERAL, span), elements_(elements)
            {
            }

            const std::vector<Expression*>& elements() const
            {
                return elements_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            std::vector<Expression*> elements_;
        };

        // Subquery expression types (Phase 2 Wave 2 - Agent B)
        enum class SubqueryType : uint8_t
        {
            SCALAR,      // Returns single value: (SELECT col FROM ...)
            EXISTS,      // Returns boolean: EXISTS (SELECT ...)
            IN,          // Membership test: col IN (SELECT ...)
            NOT_IN,      // Membership test: col NOT IN (SELECT ...)
            ARRAY        // Returns array: ARRAY(SELECT ...)
        };

        // Forward declaration
        class SelectStmt;

        // Subquery expression (Phase 2 Wave 2 - Agent B)
        class SubqueryExpr : public Expression
        {
        public:
            SubqueryExpr(const SourceSpan &span, SelectStmt* query, SubqueryType type)
                : Expression(ASTKind::SUBQUERY, span), query_(query), type_(type)
            {
            }

            SelectStmt* query() const
            {
                return query_;
            }
            SubqueryType type() const
            {
                return type_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            SelectStmt* query_;
            SubqueryType type_;
        };

        // ===== Statement Nodes =====

        class Statement : public ASTNode
        {
        public:
            using ASTNode::ASTNode;
        };

        // Column definition for CREATE TABLE
        class ColumnDef : public ASTNode
        {
        public:
            ColumnDef(const SourceSpan &span, StringPool::StringId name, const TypeName &type,
                      bool nullable, StringPool::StringId charset = 0,
                      StringPool::StringId collation = 0)
                : ASTNode(ASTKind::COLUMN_DEF, span), name_(name), type_(type), nullable_(nullable),
                  charset_(charset), collation_(collation)
            {
            }

            StringPool::StringId name() const
            {
                return name_;
            }
            const TypeName &type() const
            {
                return type_;
            }
            bool nullable() const
            {
                return nullable_;
            }
            StringPool::StringId charset() const
            {
                return charset_;
            }
            StringPool::StringId collation() const
            {
                return collation_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;
            TypeName type_;
            bool nullable_;
            StringPool::StringId charset_;   // CHARACTER SET clause
            StringPool::StringId collation_; // COLLATE clause
        };

        // CREATE TABLE statement
        class CreateTableStmt : public Statement
        {
        public:
            CreateTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                            std::vector<ColumnDef *> columns, StringPool::StringId charset = 0,
                            StringPool::StringId collation = 0, StringPool::StringId tablespace = 0)
                : Statement(ASTKind::CREATE_TABLE, span), table_name_(table_name),
                  columns_(std::move(columns)), charset_(charset), collation_(collation),
                  tablespace_(tablespace)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            const std::vector<ColumnDef *> &columns() const
            {
                return columns_;
            }
            StringPool::StringId charset() const
            {
                return charset_;
            }
            StringPool::StringId collation() const
            {
                return collation_;
            }
            StringPool::StringId tablespace() const // Phase 2 Task 2.3
            {
                return tablespace_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<ColumnDef *> columns_;
            StringPool::StringId charset_;    // DEFAULT CHARACTER SET clause
            StringPool::StringId collation_;  // DEFAULT COLLATE clause
            StringPool::StringId tablespace_; // TABLESPACE clause (Phase 2 Task 2.3)
        };

        // CREATE INDEX statement (Phase 2 Task 2.3)
        class CreateIndexStmt : public Statement
        {
        public:
            CreateIndexStmt(const SourceSpan &span, StringPool::StringId index_name,
                            StringPool::StringId table_name, std::vector<StringPool::StringId> columns,
                            bool is_unique = false, StringPool::StringId tablespace = 0)
                : Statement(ASTKind::CREATE_INDEX, span), index_name_(index_name),
                  table_name_(table_name), columns_(std::move(columns)),
                  is_unique_(is_unique), tablespace_(tablespace)
            {
            }

            StringPool::StringId indexName() const
            {
                return index_name_;
            }
            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            const std::vector<StringPool::StringId> &columns() const
            {
                return columns_;
            }
            bool isUnique() const
            {
                return is_unique_;
            }
            StringPool::StringId tablespace() const
            {
                return tablespace_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId index_name_;
            StringPool::StringId table_name_;
            std::vector<StringPool::StringId> columns_;
            bool is_unique_;
            StringPool::StringId tablespace_;
        };

        // INSERT statement
        class InsertStmt : public Statement
        {
        public:
            InsertStmt(const SourceSpan &span, StringPool::StringId table_name,
                       std::vector<StringPool::StringId> columns, std::vector<Expression *> values)
                : Statement(ASTKind::INSERT, span), table_name_(table_name),
                  columns_(std::move(columns)), values_(std::move(values))
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            const std::vector<StringPool::StringId> &columns() const
            {
                return columns_;
            }
            const std::vector<Expression *> &values() const
            {
                return values_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<StringPool::StringId> columns_;
            std::vector<Expression *> values_;
        };

        // SELECT list item
        struct SelectItem
        {
            bool is_star; // SELECT *
            Expression *expr;
            StringPool::StringId alias; // Optional AS alias

            SelectItem() : is_star(true), expr(nullptr), alias(0) {}
            SelectItem(Expression *e, StringPool::StringId a = 0)
                : is_star(false), expr(e), alias(a)
            {
            }
        };

        // JOIN types (Phase 1 Task 3.1)
        enum class JoinType : uint8_t
        {
            INNER,        // INNER JOIN
            LEFT,         // LEFT OUTER JOIN
            RIGHT,        // RIGHT OUTER JOIN
            FULL,         // FULL OUTER JOIN
            CROSS         // CROSS JOIN
        };

        // JOIN condition type
        enum class JoinConditionType : uint8_t
        {
            ON,           // JOIN ... ON condition
            USING,        // JOIN ... USING (columns)
            NATURAL,      // NATURAL JOIN (no explicit condition)
            CROSS         // CROSS JOIN (no condition)
        };

        // Table reference in FROM clause
        struct TableRef
        {
            StringPool::StringId table_name;
            StringPool::StringId alias;  // Optional table alias

            TableRef(StringPool::StringId name, StringPool::StringId a = 0)
                : table_name(name), alias(a) {}
        };

        // JOIN clause representation
        struct JoinClause
        {
            JoinType join_type;
            bool natural;  // NATURAL JOIN modifier
            TableRef right_table;
            JoinConditionType condition_type;
            Expression *on_condition;  // For ON condition
            std::vector<StringPool::StringId> using_columns;  // For USING clause

            JoinClause(JoinType type, bool is_natural, const TableRef &right,
                      JoinConditionType cond_type, Expression *on_cond = nullptr)
                : join_type(type), natural(is_natural), right_table(right),
                  condition_type(cond_type), on_condition(on_cond) {}
        };

        // FROM clause with table and optional joins
        struct FromClause
        {
            TableRef base_table;
            std::vector<JoinClause> joins;

            explicit FromClause(const TableRef &base) : base_table(base) {}
        };

        // CTE (Common Table Expression) definition - Phase 2 Wave 2
        struct CTEDefinition
        {
            StringPool::StringId name;
            SelectStmt *query;
            std::vector<StringPool::StringId> column_aliases; // Optional column aliases

            CTEDefinition(StringPool::StringId n, SelectStmt *q,
                          std::vector<StringPool::StringId> aliases = {})
                : name(n), query(q), column_aliases(std::move(aliases))
            {
            }
        };

        // WITH clause (CTE support) - Phase 2 Wave 2
        class WithClause
        {
        public:
            WithClause(std::vector<CTEDefinition> ctes)
                : ctes_(std::move(ctes))
            {
            }

            const std::vector<CTEDefinition> &ctes() const
            {
                return ctes_;
            }

        private:
            std::vector<CTEDefinition> ctes_;
        };

        // Sort direction for ORDER BY (Phase 1 Task 5.1)
        enum class SortOrder : uint8_t
        {
            ASC,
            DESC
        };

        // NULL ordering for ORDER BY (Phase 1 Task 5.1)
        enum class NullsOrder : uint8_t
        {
            DEFAULT,      // Database default
            NULLS_FIRST,
            NULLS_LAST
        };

        // ORDER BY item (Phase 1 Task 5.1)
        struct OrderByItem
        {
            Expression *expr;
            SortOrder order;
            NullsOrder nulls_order;

            OrderByItem(Expression *e, SortOrder o = SortOrder::ASC, NullsOrder n = NullsOrder::DEFAULT)
                : expr(e), order(o), nulls_order(n) {}
        };

        // GROUP BY clause (Phase 1 Task 4.1)
        struct GroupByClause
        {
            std::vector<Expression *> grouping_exprs;
            Expression *having_clause;  // Optional HAVING condition

            GroupByClause() : having_clause(nullptr) {}
            explicit GroupByClause(std::vector<Expression *> exprs, Expression *having = nullptr)
                : grouping_exprs(std::move(exprs)), having_clause(having) {}
        };

        // SELECT statement
        class SelectStmt : public Statement
        {
        public:
            // Legacy constructor for single table (backwards compatibility)
            SelectStmt(const SourceSpan &span, std::vector<SelectItem> select_list,
                       StringPool::StringId table_name, Expression *where_clause = nullptr)
                : Statement(ASTKind::SELECT, span), select_list_(std::move(select_list)),
                  from_clause_(TableRef(table_name)), where_clause_(where_clause),
                  has_joins_(false), with_clause_(nullptr)
            {
            }

            // New constructor for FROM clause with JOINs (Phase 1 Task 3.1)
            SelectStmt(const SourceSpan &span, std::vector<SelectItem> select_list,
                       FromClause from_clause, Expression *where_clause = nullptr)
                : Statement(ASTKind::SELECT, span), select_list_(std::move(select_list)),
                  from_clause_(std::move(from_clause)), where_clause_(where_clause),
                  has_joins_(!from_clause_.joins.empty()), with_clause_(nullptr)
            {
            }

            // Constructor with WITH clause (Phase 2 Wave 2)
            SelectStmt(const SourceSpan &span, WithClause *with_clause,
                       std::vector<SelectItem> select_list,
                       FromClause from_clause, Expression *where_clause = nullptr)
                : Statement(ASTKind::SELECT, span), select_list_(std::move(select_list)),
                  from_clause_(std::move(from_clause)), where_clause_(where_clause),
                  has_joins_(!from_clause_.joins.empty()), with_clause_(with_clause)
            {
            }

            const std::vector<SelectItem> &selectList() const
            {
                return select_list_;
            }

            // Legacy accessor for backwards compatibility
            StringPool::StringId tableName() const
            {
                return from_clause_.base_table.table_name;
            }

            // New accessor for FROM clause
            const FromClause &fromClause() const
            {
                return from_clause_;
            }

            bool hasJoins() const
            {
                return has_joins_;
            }

            Expression *whereClause() const
            {
                return where_clause_;
            }

            // WITH clause accessor (Phase 2 Wave 2)
            WithClause *withClause() const
            {
                return with_clause_;
            }
            void setWithClause(WithClause *with_clause)
            {
                with_clause_ = with_clause;
            }

            // Aggregation accessors (Phase 1 Task 4.1)
            const GroupByClause *groupByClause() const
            {
                return group_by_clause_.grouping_exprs.empty() ? nullptr : &group_by_clause_;
            }
            void setGroupByClause(GroupByClause clause)
            {
                group_by_clause_ = std::move(clause);
            }

            // Sorting accessors (Phase 1 Task 5.1)
            const std::vector<OrderByItem> &orderByClause() const
            {
                return order_by_clause_;
            }
            void setOrderByClause(std::vector<OrderByItem> clause)
            {
                order_by_clause_ = std::move(clause);
            }

            // Limit accessors (Phase 1 Task 5.2)
            bool hasLimit() const
            {
                return limit_count_ >= 0;
            }
            int64_t limitCount() const
            {
                return limit_count_;
            }
            void setLimitCount(int64_t count)
            {
                limit_count_ = count;
            }

            bool hasOffset() const
            {
                return offset_count_ >= 0;
            }
            int64_t offsetCount() const
            {
                return offset_count_;
            }
            void setOffsetCount(int64_t count)
            {
                offset_count_ = count;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            std::vector<SelectItem> select_list_;
            FromClause from_clause_;  // Replaces table_name_
            Expression *where_clause_;
            bool has_joins_;
            WithClause *with_clause_;  // Phase 2 Wave 2: CTE support

            // Aggregation (Phase 1 Task 4.1)
            GroupByClause group_by_clause_;

            // Sorting (Phase 1 Task 5.1)
            std::vector<OrderByItem> order_by_clause_;

            // Limit (Phase 1 Task 5.2)
            int64_t limit_count_ = -1;   // -1 = no limit
            int64_t offset_count_ = -1;  // -1 = no offset
        };

        // Assignment for UPDATE SET clause
        struct Assignment
        {
            StringPool::StringId column_name;
            Expression *value;

            Assignment(StringPool::StringId col, Expression *val)
                : column_name(col), value(val)
            {
            }
        };

        // UPDATE statement (Phase 1 Task 2.1)
        class UpdateStmt : public Statement
        {
        public:
            UpdateStmt(const SourceSpan &span, StringPool::StringId table_name,
                       std::vector<Assignment> assignments, Expression *where_clause = nullptr)
                : Statement(ASTKind::UPDATE, span), table_name_(table_name),
                  assignments_(std::move(assignments)), where_clause_(where_clause)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            const std::vector<Assignment> &assignments() const
            {
                return assignments_;
            }
            Expression *whereClause() const
            {
                return where_clause_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<Assignment> assignments_;
            Expression *where_clause_;
        };

        // DELETE statement (Phase 1 Task 2.2)
        class DeleteStmt : public Statement
        {
        public:
            DeleteStmt(const SourceSpan &span, StringPool::StringId table_name,
                       Expression *where_clause = nullptr)
                : Statement(ASTKind::DELETE_STMT, span), table_name_(table_name),
                  where_clause_(where_clause)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            Expression *whereClause() const
            {
                return where_clause_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            Expression *where_clause_;
        };

        // ANALYZE statement (Phase 1 Task 1.1.2: Statistics collection)
        class AnalyzeStmt : public Statement
        {
        public:
            // ANALYZE table_name [COLUMN column_name] [SAMPLE sample_rate]
            AnalyzeStmt(const SourceSpan &span, StringPool::StringId table_name,
                        StringPool::StringId column_name = 0,
                        float sample_rate = 0.0f)
                : Statement(ASTKind::ANALYZE, span), table_name_(table_name),
                  column_name_(column_name), sample_rate_(sample_rate)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            StringPool::StringId columnName() const
            {
                return column_name_;
            }
            float sampleRate() const
            {
                return sample_rate_;
            }
            bool analyzeAllColumns() const
            {
                return column_name_ == 0;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;  // Table to analyze
            StringPool::StringId column_name_; // Specific column (or INVALID for all columns)
            float sample_rate_;                // Sample rate (0.0 = auto, 0.0-1.0 = explicit)
        };

        /**
         * ExplainStmt - EXPLAIN command (Phase 1 Task 1.5)
         *
         * Shows the execution plan for a query without executing it.
         * Displays cost estimates, row estimates, and access methods.
         *
         * Example:
         *   EXPLAIN SELECT * FROM users WHERE id > 10;
         *
         * Output:
         *   SeqScan on users (cost=0.00..225.00 rows=1000)
         *     Filter: id > 10
         */
        class ExplainStmt : public Statement
        {
        public:
            /**
             * Constructor
             *
             * @param span Source location
             * @param query The statement to explain (SELECT, INSERT, etc.)
             */
            ExplainStmt(const SourceSpan &span, Statement *query)
                : Statement(ASTKind::EXPLAIN, span), query_(query)
            {
            }

            /**
             * Get the query to explain
             */
            Statement *query() const
            {
                return query_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            Statement *query_; // The statement to explain
        };

        // Transaction mode flags (Phase 2 Task 2.6)
        enum class TransactionMode : uint8_t
        {
            READ_WRITE = 0,
            READ_ONLY = 1
        };

        enum class IsolationLevel : uint8_t
        {
            READ_COMMITTED = 0,
            SNAPSHOT = 1,
            SNAPSHOT_TABLE_STABILITY = 2
        };

        // Table lock mode for RESERVING clause (Firebird-style)
        enum class TableLockMode : uint8_t
        {
            SHARED = 0,    // SHARED READ - allows concurrent reads
            PROTECTED = 1, // PROTECTED READ/WRITE - exclusive table access
        };

        // Table reservation for RESERVING clause
        struct TableReservation
        {
            StringPool::StringId table_name;
            TableLockMode lock_mode;
            bool for_write;

            TableReservation(StringPool::StringId name, TableLockMode mode, bool write)
                : table_name(name), lock_mode(mode), for_write(write)
            {
            }
        };

        // START TRANSACTION statement (Phase 2 Task 2.6, Phase 3 Task 3.6)
        class StartTransactionStmt : public Statement
        {
        public:
            StartTransactionStmt(const SourceSpan &span,
                                 TransactionMode mode = TransactionMode::READ_WRITE,
                                 IsolationLevel isolation = IsolationLevel::READ_COMMITTED,
                                 bool wait = true, bool commit_outstanding = false,
                                 uint32_t lock_timeout = 0,
                                 std::vector<TableReservation> reservations = {})
                : Statement(ASTKind::START_TRANSACTION, span), mode_(mode), isolation_(isolation),
                  wait_(wait), commit_outstanding_(commit_outstanding), lock_timeout_(lock_timeout),
                  table_reservations_(std::move(reservations))
            {
            }

            TransactionMode mode() const
            {
                return mode_;
            }
            IsolationLevel isolation() const
            {
                return isolation_;
            }
            bool wait() const
            {
                return wait_;
            }
            bool commitOutstanding() const
            {
                return commit_outstanding_;
            }
            uint32_t lockTimeout() const
            {
                return lock_timeout_;
            }
            const std::vector<TableReservation> &tableReservations() const
            {
                return table_reservations_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            TransactionMode mode_;
            IsolationLevel isolation_;
            bool wait_;
            bool commit_outstanding_;
            uint32_t
                lock_timeout_; // Lock timeout in seconds (0 = no wait, UINT32_MAX = wait forever)
            std::vector<TableReservation> table_reservations_; // RESERVING clause tables
        };

        // COMMIT statement
        class CommitStmt : public Statement
        {
        public:
            explicit CommitStmt(const SourceSpan &span) : Statement(ASTKind::COMMIT, span) {}

            void accept(ASTVisitor *visitor) override;
        };

        // ROLLBACK statement
        class RollbackStmt : public Statement
        {
        public:
            explicit RollbackStmt(const SourceSpan &span) : Statement(ASTKind::ROLLBACK, span) {}

            void accept(ASTVisitor *visitor) override;
        };

        // CREATE TABLESPACE statement (Phase 2 Task 2.1)
        class CreateTablespaceStmt : public Statement
        {
        public:
            CreateTablespaceStmt(const SourceSpan &span, StringPool::StringId tablespace_name,
                                 StringPool::StringId location, bool autoextend_enabled = true,
                                 uint32_t autoextend_size_mb = 100, uint32_t max_size_mb = 0,
                                 uint32_t prealloc_pages = 0)
                : Statement(ASTKind::CREATE_TABLESPACE, span), tablespace_name_(tablespace_name),
                  location_(location), autoextend_enabled_(autoextend_enabled),
                  autoextend_size_mb_(autoextend_size_mb), max_size_mb_(max_size_mb),
                  prealloc_pages_(prealloc_pages)
            {
            }

            StringPool::StringId tablespaceName() const
            {
                return tablespace_name_;
            }
            StringPool::StringId location() const
            {
                return location_;
            }
            bool autoextendEnabled() const
            {
                return autoextend_enabled_;
            }
            uint32_t autoextendSizeMB() const
            {
                return autoextend_size_mb_;
            }
            uint32_t maxSizeMB() const
            {
                return max_size_mb_;
            }
            uint32_t preallocPages() const
            {
                return prealloc_pages_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId tablespace_name_;
            StringPool::StringId location_;
            bool autoextend_enabled_;
            uint32_t autoextend_size_mb_; // AUTOEXTEND_SIZE parameter
            uint32_t max_size_mb_;        // MAXSIZE parameter (0 = UNLIMITED)
            uint32_t prealloc_pages_;     // PREALLOC parameter
        };

        // DROP TABLESPACE statement (Phase 2 Task 2.1)
        class DropTablespaceStmt : public Statement
        {
        public:
            DropTablespaceStmt(const SourceSpan &span, StringPool::StringId tablespace_name,
                               bool force = false)
                : Statement(ASTKind::DROP_TABLESPACE, span), tablespace_name_(tablespace_name),
                  force_(force)
            {
            }

            StringPool::StringId tablespaceName() const
            {
                return tablespace_name_;
            }
            bool force() const
            {
                return force_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId tablespace_name_;
            bool force_; // FORCE clause
        };

        // ATTACH TABLESPACE statement (Phase 6 Task 6.1)
        class AttachTablespaceStmt : public Statement
        {
        public:
            AttachTablespaceStmt(const SourceSpan &span, StringPool::StringId file_path,
                                 StringPool::StringId tablespace_name = 0)
                : Statement(ASTKind::ATTACH_TABLESPACE, span), file_path_(file_path),
                  tablespace_name_(tablespace_name)
            {
            }

            StringPool::StringId filePath() const
            {
                return file_path_;
            }
            StringPool::StringId tablespaceName() const
            {
                return tablespace_name_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId file_path_;       // Path to .sbts file
            StringPool::StringId tablespace_name_; // Optional AS name
        };

        // DETACH TABLESPACE statement (Phase 6 Task 6.2)
        class DetachTablespaceStmt : public Statement
        {
        public:
            DetachTablespaceStmt(const SourceSpan &span, StringPool::StringId tablespace_name,
                                 bool force = false)
                : Statement(ASTKind::DETACH_TABLESPACE, span), tablespace_name_(tablespace_name),
                  force_(force)
            {
            }

            StringPool::StringId tablespaceName() const
            {
                return tablespace_name_;
            }
            bool force() const
            {
                return force_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId tablespace_name_;
            bool force_; // FORCE clause
        };

        // ALTER TABLESPACE statement (Phase 2 Task 2.2)
        enum class TablespaceAlterationType : uint8_t
        {
            SET_AUTOEXTEND,      // AUTOEXTEND ON|OFF
            SET_AUTOEXTEND_SIZE, // AUTOEXTEND_SIZE N
            SET_MAXSIZE,         // MAXSIZE N | UNLIMITED
            RENAME_TO            // RENAME TO new_name
        };

        struct TablespaceAlteration
        {
            TablespaceAlterationType type;
            // For SET_AUTOEXTEND
            bool autoextend_enabled;
            // For SET_AUTOEXTEND_SIZE and SET_MAXSIZE
            uint32_t size_value;
            // For RENAME_TO
            StringPool::StringId new_name;

            TablespaceAlteration(TablespaceAlterationType t) : type(t), autoextend_enabled(false), size_value(0), new_name(0) {}
        };

        class AlterTablespaceStmt : public Statement
        {
        public:
            AlterTablespaceStmt(const SourceSpan &span, StringPool::StringId tablespace_name)
                : Statement(ASTKind::ALTER_TABLESPACE, span), tablespace_name_(tablespace_name)
            {
            }

            StringPool::StringId tablespaceName() const
            {
                return tablespace_name_;
            }

            void addAlteration(const TablespaceAlteration &alteration)
            {
                alterations_.push_back(alteration);
            }

            const std::vector<TablespaceAlteration> &alterations() const
            {
                return alterations_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId tablespace_name_;
            std::vector<TablespaceAlteration> alterations_;
        };

        // ALTER TABLE ... SET TABLESPACE statement (Phase 4 Task 4.1.1)
        // Moves a table to a different tablespace (offline or online mode)
        class AlterTableSetTablespaceStmt : public Statement
        {
        public:
            AlterTableSetTablespaceStmt(const SourceSpan &span, StringPool::StringId table_name,
                                        StringPool::StringId tablespace_name, bool online)
                : Statement(ASTKind::ALTER_TABLE_SET_TABLESPACE, span), table_name_(table_name),
                  tablespace_name_(tablespace_name), online_(online)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            StringPool::StringId tablespaceName() const
            {
                return tablespace_name_;
            }
            bool online() const
            {
                return online_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            StringPool::StringId tablespace_name_;
            bool online_; // true if ONLINE clause present
        };

        // SWEEP DATABASE statement (Phase 3 Task 3.3)
        class SweepStmt : public Statement
        {
        public:
            explicit SweepStmt(const SourceSpan &span) : Statement(ASTKind::SWEEP, span) {}

            void accept(ASTVisitor *visitor) override;
        };

        // SET TRANSACTION statement (Phase 3 Task 3.6)
        // Sets transaction parameters for the NEXT transaction (doesn't start one immediately)
        class SetTransactionStmt : public Statement
        {
        public:
            SetTransactionStmt(const SourceSpan &span,
                               TransactionMode mode = TransactionMode::READ_WRITE,
                               IsolationLevel isolation = IsolationLevel::READ_COMMITTED,
                               bool wait = true, uint32_t lock_timeout = 0,
                               std::vector<TableReservation> reservations = {})
                : Statement(ASTKind::SET_TRANSACTION, span), mode_(mode), isolation_(isolation),
                  wait_(wait), lock_timeout_(lock_timeout),
                  table_reservations_(std::move(reservations))
            {
            }

            TransactionMode mode() const
            {
                return mode_;
            }
            IsolationLevel isolation() const
            {
                return isolation_;
            }
            bool wait() const
            {
                return wait_;
            }
            uint32_t lockTimeout() const
            {
                return lock_timeout_;
            }
            const std::vector<TableReservation> &tableReservations() const
            {
                return table_reservations_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            TransactionMode mode_;
            IsolationLevel isolation_;
            bool wait_;
            uint32_t
                lock_timeout_; // Lock timeout in seconds (0 = no wait, UINT32_MAX = wait forever)
            std::vector<TableReservation> table_reservations_; // RESERVING clause tables
        };

        // Trigger timing (Phase 2 Wave 2 - Agent C)
        enum class TriggerTiming : uint8_t
        {
            BEFORE,
            AFTER
        };

        // Trigger event (Phase 2 Wave 2 - Agent C)
        enum class TriggerEvent : uint8_t
        {
            INSERT,
            UPDATE,
            DELETE
        };

        // Trigger granularity (Phase 2 Wave 2 - Agent C)
        enum class TriggerGranularity : uint8_t
        {
            FOR_EACH_ROW,
            FOR_EACH_STATEMENT  // For future support
        };

        // CREATE TRIGGER statement (Phase 2 Wave 2 - Agent C)
        class CreateTriggerStmt : public Statement
        {
        public:
            CreateTriggerStmt(const SourceSpan &span,
                              StringPool::StringId trigger_name,
                              StringPool::StringId table_name,
                              TriggerTiming timing,
                              TriggerEvent event,
                              TriggerGranularity granularity,
                              StringPool::StringId procedure_name)
                : Statement(ASTKind::CREATE_TRIGGER, span),
                  trigger_name_(trigger_name),
                  table_name_(table_name),
                  timing_(timing),
                  event_(event),
                  granularity_(granularity),
                  procedure_name_(procedure_name)
            {
            }

            StringPool::StringId triggerName() const { return trigger_name_; }
            StringPool::StringId tableName() const { return table_name_; }
            TriggerTiming timing() const { return timing_; }
            TriggerEvent event() const { return event_; }
            TriggerGranularity granularity() const { return granularity_; }
            StringPool::StringId procedureName() const { return procedure_name_; }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId trigger_name_;
            StringPool::StringId table_name_;
            TriggerTiming timing_;
            TriggerEvent event_;
            TriggerGranularity granularity_;
            StringPool::StringId procedure_name_;
        };

        // DROP TRIGGER statement (Phase 2 Wave 2 - Agent C)
        class DropTriggerStmt : public Statement
        {
        public:
            DropTriggerStmt(const SourceSpan &span,
                            StringPool::StringId trigger_name,
                            bool if_exists = false)
                : Statement(ASTKind::DROP_TRIGGER, span),
                  trigger_name_(trigger_name),
                  if_exists_(if_exists)
            {
            }

            StringPool::StringId triggerName() const { return trigger_name_; }
            bool ifExists() const { return if_exists_; }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId trigger_name_;
            bool if_exists_;
        };

        // ===== Visitor Pattern =====

        class ASTVisitor
        {
        public:
            virtual ~ASTVisitor() = default;

            // Statements
            virtual void visit(CreateTableStmt *node) = 0;
            virtual void visit(CreateIndexStmt *node) = 0;             // Phase 2 Task 2.3
            virtual void visit(CreateTablespaceStmt *node) = 0;        // Phase 2 Task 2.1
            virtual void visit(AlterTablespaceStmt *node) = 0;         // Phase 2 Task 2.2
            virtual void visit(AlterTableSetTablespaceStmt *node) = 0; // Phase 4 Task 4.1.1
            virtual void visit(DropTablespaceStmt *node) = 0;          // Phase 2 Task 2.1
            virtual void visit(AttachTablespaceStmt *node) = 0;        // Phase 6 Task 6.1
            virtual void visit(DetachTablespaceStmt *node) = 0;        // Phase 6 Task 6.2
            virtual void visit(InsertStmt *node) = 0;
            virtual void visit(SelectStmt *node) = 0;
            virtual void visit(UpdateStmt *node) = 0;           // Phase 1 Task 2.1
            virtual void visit(DeleteStmt *node) = 0;           // Phase 1 Task 2.2
            virtual void visit(AnalyzeStmt *node) = 0;          // Phase 1 Task 1.1.2
            virtual void visit(ExplainStmt *node) = 0;          // Phase 1 Task 1.5
            virtual void visit(StartTransactionStmt *node) = 0; // Phase 2 Task 2.6
            virtual void visit(SetTransactionStmt *node) = 0;   // Phase 3 Task 3.6
            virtual void visit(CommitStmt *node) = 0;           // Phase 2 Task 2.6
            virtual void visit(RollbackStmt *node) = 0;         // Phase 2 Task 2.6
            virtual void visit(SweepStmt *node) = 0;            // Phase 3 Task 3.3

            // Expressions
            virtual void visit(LiteralExpr *node) = 0;
            virtual void visit(IdentifierExpr *node) = 0;
            virtual void visit(BinaryOpExpr *node) = 0;
            virtual void visit(CastExpr *node) = 0;
            virtual void visit(FunctionCallExpr *node) = 0;
            virtual void visit(AggregateExpr *node) = 0;  // Phase 1 Task 4.1
            virtual void visit(WindowFuncExpr *node) = 0; // Phase 1 Task 6
            virtual void visit(WindowSpec *node) = 0;     // Phase 1 Task 6
            virtual void visit(JSONFuncExpr *node) = 0;   // Phase 1 Task 7
            virtual void visit(CoalesceExpr *node) = 0;   // Phase 1 Task 8
            virtual void visit(NullIfExpr *node) = 0;     // Phase 1 Task 8
            virtual void visit(CaseExpr *node) = 0;       // Phase 1 Task 8
            virtual void visit(ArrayLiteral *node) = 0;   // Phase 2 Task 12
            virtual void visit(SubqueryExpr *node) = 0;   // Phase 2 Wave 2 - Agent B

            // Other nodes
            virtual void visit(ColumnDef *node) = 0;
        };

        // AST Printer for debugging
        class ASTPrinter : public ASTVisitor
        {
        public:
            ASTPrinter(std::ostream &out, const StringPool &pool)
                : out_(out), pool_(pool), indent_(0)
            {
            }

            void visit(CreateTableStmt *node) override;
            void visit(CreateIndexStmt *node) override;              // Phase 2 Task 2.3
            void visit(CreateTablespaceStmt *node) override;         // Phase 2 Task 2.1
            void visit(AlterTablespaceStmt *node) override;          // Phase 2 Task 2.2
            void visit(AlterTableSetTablespaceStmt *node) override;  // Phase 4 Task 4.1.1
            void visit(DropTablespaceStmt *node) override;           // Phase 2 Task 2.1
            void visit(AttachTablespaceStmt *node) override;         // Phase 6 Task 6.1
            void visit(DetachTablespaceStmt *node) override;         // Phase 6 Task 6.2
            void visit(InsertStmt *node) override;
            void visit(SelectStmt *node) override;
            void visit(UpdateStmt *node) override;                   // Phase 1 Task 2.1
            void visit(DeleteStmt *node) override;                   // Phase 1 Task 2.2
            void visit(AnalyzeStmt *node) override;                  // Phase 1 Task 1.1.2
            void visit(ExplainStmt *node) override;                  // Phase 1 Task 1.5
            void visit(StartTransactionStmt *node) override; // Phase 2 Task 2.6
            void visit(SetTransactionStmt *node) override;   // Phase 3 Task 3.6
            void visit(CommitStmt *node) override;           // Phase 2 Task 2.6
            void visit(RollbackStmt *node) override;         // Phase 2 Task 2.6
            void visit(SweepStmt *node) override;            // Phase 3 Task 3.3
            void visit(LiteralExpr *node) override;
            void visit(IdentifierExpr *node) override;
            void visit(BinaryOpExpr *node) override;
            void visit(CastExpr *node) override;
            void visit(FunctionCallExpr *node) override;
            void visit(AggregateExpr *node) override;  // Phase 1 Task 4.1
            void visit(WindowFuncExpr *node) override; // Phase 1 Task 6
            void visit(WindowSpec *node) override;     // Phase 1 Task 6
            void visit(JSONFuncExpr *node) override;   // Phase 1 Task 7
            void visit(CoalesceExpr *node) override;   // Phase 1 Task 8
            void visit(NullIfExpr *node) override;     // Phase 1 Task 8
            void visit(CaseExpr *node) override;       // Phase 1 Task 8
            void visit(ArrayLiteral *node) override;   // Phase 2 Task 12
            void visit(SubqueryExpr *node) override;   // Phase 2 Wave 2 - Agent B
            void visit(ColumnDef *node) override;

        private:
            std::ostream &out_;
            const StringPool &pool_;
            int indent_;

            void printIndent();
            void increaseIndent()
            {
                indent_ += 2;
            }
            void decreaseIndent()
            {
                indent_ -= 2;
            }
        };

    } // namespace parser
} // namespace scratchbird