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
            DROP_TABLE,                // ALPHA Phase 1 - DDL Modifications
            DROP_INDEX,                // ALPHA Phase 1 - DDL Modifications
            TRUNCATE_TABLE,            // ALPHA Phase 1 - DDL Modifications (TRUNCATE TABLE)
            ALTER_TABLE,               // ALPHA Phase 1 - DDL Modifications (ADD/DROP/ALTER COLUMN)
            CREATE_SEQUENCE,           // ALPHA Phase 1 - Sequences
            ALTER_SEQUENCE,            // ALPHA Phase 1 - Sequences
            DROP_SEQUENCE,             // ALPHA Phase 1 - Sequences
            CREATE_VIEW,               // ALPHA Phase 1 - Views
            DROP_VIEW,                 // ALPHA Phase 1 - Views
            REFRESH_MATERIALIZED_VIEW, // ALPHA Phase 1 - Materialized Views
            CREATE_TRIGGER,            // Phase 2 Wave 2 - Agent C: Basic Triggers
            DROP_TRIGGER,              // Phase 2 Wave 2 - Agent C: Basic Triggers
            CREATE_FUNCTION,           // Phase 2 Task 10.2 - Stored Procedures
            CREATE_PROCEDURE,          // Phase 2 Task 10.2 - Stored Procedures
            CREATE_TABLESPACE,         // Phase 2 Task 2.1
            ALTER_TABLESPACE,          // Phase 2 Task 2.2
            ALTER_TABLE_SET_TABLESPACE, // Phase 4 Task 4.1.1
            DROP_TABLESPACE,           // Phase 2 Task 2.1
            ATTACH_TABLESPACE,         // Phase 6 Task 6.1
            DETACH_TABLESPACE,         // Phase 6 Task 6.2
            INSERT,
            SELECT,
            SET_OPERATION,     // UNION/INTERSECT/EXCEPT set operations
            MERGE,             // Alpha 1 - Advanced SQL: MERGE statement
            UPDATE,            // Phase 1 Task 2.1: UPDATE statement
            DELETE_STMT,       // Phase 1 Task 2.2: DELETE statement (DELETE is a keyword)
            ANALYZE,           // Phase 1 Task 1.1.2: Statistics collection
            EXPLAIN,           // Phase 1 Task 1.5: EXPLAIN command
            START_TRANSACTION, // Phase 2 Task 2.6
            SET_TRANSACTION,   // Phase 3 Task 3.6
            COMMIT,            // Phase 2 Task 2.6
            ROLLBACK,          // Phase 2 Task 2.6
            SWEEP,             // Phase 3 Task 3.3
            SHOW,              // ALPHA Phase 1 - Developer Experience: SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE
            DESCRIBE,          // ALPHA Phase 1 - Developer Experience: DESCRIBE table (alias for SHOW COLUMNS)

            // Security statements (ALPHA Phase 1 - Security System Phase 2)
            CREATE_USER,       // CREATE USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
            ALTER_USER,        // ALTER USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
            DROP_USER,         // DROP USER username [IF EXISTS] [CASCADE | RESTRICT]
            CREATE_ROLE,       // CREATE ROLE rolename
            DROP_ROLE,         // DROP ROLE rolename [IF EXISTS] [CASCADE | RESTRICT]
            CREATE_GROUP,      // CREATE GROUP groupname
            DROP_GROUP,        // DROP GROUP groupname [IF EXISTS] [CASCADE | RESTRICT]
            GRANT_PRIVILEGE,   // GRANT privilege ON object TO grantee [WITH GRANT OPTION]
            REVOKE_PRIVILEGE,  // REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]
            GRANT_ROLE,        // GRANT role TO user/role
            REVOKE_ROLE,       // REVOKE role FROM user/role [CASCADE | RESTRICT]
            SET_ROLE,          // SET ROLE rolename / RESET ROLE
            SET_SESSION_AUTH,  // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION
            SET_CONSTRAINTS,   // P2-7: SET CONSTRAINTS {ALL | constraint_name} {DEFERRED | IMMEDIATE}
            CREATE_POLICY,     // Security Phase 3.4: CREATE POLICY policy_name ON table_name
            DROP_POLICY,       // Security Phase 3.4: DROP POLICY policy_name ON table_name
            ALTER_TABLE_RLS,   // Security Phase 3.4: ALTER TABLE ... ENABLE/DISABLE ROW LEVEL SECURITY

            // Procedural language statements (Phase 2 Task 10.2)
            BLOCK,             // BEGIN...END block
            VAR_DECLARATION,   // Variable declaration
            ASSIGNMENT,        // Variable assignment
            IF_STMT,           // IF statement
            LOOP_STMT,         // LOOP statement
            WHILE_STMT,        // WHILE loop
            EXIT_STMT,         // EXIT statement
            RETURN_STMT,       // RETURN statement
            RAISE_STMT,        // RAISE exception
            TRY_EXCEPT,        // TRY/EXCEPT block

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
            GROUPING,        // Phase 3: Advanced Grouping - GROUPING() function
            SUBQUERY,        // Phase 2 Wave 2 - Agent B: Subquery expression
            SEQUENCE_FUNCTION, // ALPHA Phase 1 - Sequences (NEXTVAL, CURRVAL, SETVAL)
            EXTRACT,         // EXTRACT(field FROM value) - extract sub-information

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
                NULL_LITERAL,
                RANGE  // Task 15 Phase 4: Range literals like '[1,10)'
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
            void setRangeValue(StringPool::StringId v)
            {
                range_value_ = v;  // Store range literal string like '[1,10)'
            }

            // Range literal accessor (returns string representation)
            StringPool::StringId rangeValue() const
            {
                return range_value_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            LiteralType literal_type_;
            union
            {
                int64_t int_value_;
                double float_value_;
                StringPool::StringId string_value_;
                StringPool::StringId range_value_;  // For RANGE literals
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
            REGEX_NOT_MATCH_CI, // !~* - regex not match case-insensitive
            // Range operators (Task 15 Phase 4) - reuse array overlap/contains
            RANGE_STRICTLY_LEFT,  // << - strictly left of
            RANGE_STRICTLY_RIGHT, // >> - strictly right of
            RANGE_ADJACENT        // -|- - adjacent
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

        // Sequence function types (ALPHA Phase 1 - Sequences)
        enum class SequenceFunctionType : uint8_t
        {
            NEXTVAL,
            CURRVAL,
            SETVAL
        };

        // Sequence function expression (ALPHA Phase 1 - Sequences)
        class SequenceFunctionExpr : public Expression
        {
        public:
            SequenceFunctionExpr(const SourceSpan& span, SequenceFunctionType func_type,
                                 Expression* sequence_name, Expression* value = nullptr,
                                 Expression* is_called = nullptr)
                : Expression(ASTKind::SEQUENCE_FUNCTION, span),
                  func_type_(func_type), sequence_name_(sequence_name),
                  value_(value), is_called_(is_called)
            {
            }

            SequenceFunctionType functionType() const { return func_type_; }
            Expression* sequenceName() const { return sequence_name_; }
            Expression* value() const { return value_; }  // For SETVAL
            Expression* isCalled() const { return is_called_; }  // For SETVAL

            void accept(ASTVisitor* visitor) override;

        private:
            SequenceFunctionType func_type_;
            Expression* sequence_name_;
            Expression* value_;       // For SETVAL(seq, value)
            Expression* is_called_;   // For SETVAL(seq, value, is_called)
        };

        /**
         * ExtractExpr - EXTRACT(field FROM value) expression
         *
         * Extracts sub-information from complex data types (DATE, TIME, TIMESTAMP,
         * INTERVAL, UUID, INET, POINT, ARRAY, RANGE, etc.)
         *
         * Examples:
         *   EXTRACT(year FROM date_col)
         *   EXTRACT(x FROM point_col)
         *   EXTRACT(version FROM uuid_col)
         */
        class ExtractExpr : public Expression
        {
        public:
            ExtractExpr(const SourceSpan& span, uint8_t field_id, const std::string& field_name,
                        Expression* source)
                : Expression(ASTKind::EXTRACT, span),
                  field_id_(field_id), field_name_(field_name), source_(source)
            {
            }

            uint8_t fieldId() const { return field_id_; }
            const std::string& fieldName() const { return field_name_; }
            Expression* source() const { return source_; }

            void accept(ASTVisitor* visitor) override;

        private:
            uint8_t field_id_;           // ExtractField enum value
            std::string field_name_;     // Field name as string (for error messages)
            Expression* source_;         // Source expression to extract from
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
            NTH_VALUE,
            CUME_DIST,        // Alpha 1 - Missing Functions Phase 4
            PERCENT_RANK      // Alpha 1 - Missing Functions Phase 4
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

        // GROUPING() function for ROLLUP/CUBE/GROUPING SETS (Phase 3: Advanced Grouping)
        class GroupingExpr : public Expression
        {
        public:
            GroupingExpr(const SourceSpan &span, Expression* arg)
                : Expression(ASTKind::GROUPING, span),
                  arg_(arg)
            {
            }

            Expression* arg() const
            {
                return arg_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            Expression* arg_;  // The grouping column expression to check
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

        // Generated column storage type (ALPHA Phase 1 - Constraint Features)
        enum class GeneratedColumnStorage : uint8_t
        {
            NOT_GENERATED = 0,  // Regular column
            STORED = 1,         // GENERATED ALWAYS AS ... STORED
            VIRTUAL = 2         // GENERATED ALWAYS AS ... VIRTUAL
        };

        // Column definition for CREATE TABLE
        class ColumnDef : public ASTNode
        {
        public:
            ColumnDef(const SourceSpan &span, StringPool::StringId name, const TypeName &type,
                      bool nullable, StringPool::StringId charset = 0,
                      StringPool::StringId collation = 0,
                      Expression *default_value = nullptr,
                      Expression *check_expr = nullptr,
                      bool is_unique = false,
                      bool is_primary_key = false,
                      StringPool::StringId fk_table = 0,
                      std::vector<StringPool::StringId> fk_columns = {},
                      StringPool::StringId fk_on_delete = 0,
                      StringPool::StringId fk_on_update = 0,
                      bool is_identity = false,
                      bool identity_always = true,
                      GeneratedColumnStorage generated_storage = GeneratedColumnStorage::NOT_GENERATED,
                      Expression *generation_expr = nullptr)
                : ASTNode(ASTKind::COLUMN_DEF, span), name_(name), type_(type), nullable_(nullable),
                  charset_(charset), collation_(collation),
                  default_value_(default_value), check_expr_(check_expr), is_unique_(is_unique),
                  is_primary_key_(is_primary_key),
                  fk_table_(fk_table), fk_columns_(std::move(fk_columns)),
                  fk_on_delete_(fk_on_delete), fk_on_update_(fk_on_update),
                  is_identity_(is_identity), identity_always_(identity_always),
                  generated_storage_(generated_storage), generation_expr_(generation_expr)
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
            Expression *default_value() const
            {
                return default_value_;
            }
            Expression *check_expr() const
            {
                return check_expr_;
            }
            bool isUnique() const
            {
                return is_unique_;
            }
            bool isPrimaryKey() const
            {
                return is_primary_key_;
            }
            StringPool::StringId fk_table() const
            {
                return fk_table_;
            }
            const std::vector<StringPool::StringId> &fk_columns() const
            {
                return fk_columns_;
            }
            StringPool::StringId fk_on_delete() const
            {
                return fk_on_delete_;
            }
            StringPool::StringId fk_on_update() const
            {
                return fk_on_update_;
            }
            bool isIdentity() const
            {
                return is_identity_;
            }
            bool identityAlways() const
            {
                return identity_always_;
            }
            GeneratedColumnStorage generatedStorage() const
            {
                return generated_storage_;
            }
            Expression *generationExpr() const
            {
                return generation_expr_;
            }
            bool isGenerated() const
            {
                return generated_storage_ != GeneratedColumnStorage::NOT_GENERATED;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;
            TypeName type_;
            bool nullable_;
            StringPool::StringId charset_;   // CHARACTER SET clause
            StringPool::StringId collation_; // COLLATE clause
            Expression *default_value_;      // DEFAULT clause expression
            Expression *check_expr_;         // CHECK constraint expression
            bool is_unique_;                 // UNIQUE constraint flag
            bool is_primary_key_;            // PRIMARY KEY constraint flag
            StringPool::StringId fk_table_;  // REFERENCES table name
            std::vector<StringPool::StringId> fk_columns_;  // REFERENCES column list
            StringPool::StringId fk_on_delete_;  // ON DELETE action
            StringPool::StringId fk_on_update_;  // ON UPDATE action
            bool is_identity_;                   // IDENTITY column flag (ALPHA Phase 1)
            bool identity_always_;               // true=ALWAYS, false=BY DEFAULT (ALPHA Phase 1)
            GeneratedColumnStorage generated_storage_;  // GENERATED column storage type (ALPHA Phase 1)
            Expression *generation_expr_;        // GENERATED ALWAYS AS expression (ALPHA Phase 1)
        };

        // Table-level constraint (ALPHA Phase C - Composite FK)
        class TableConstraint : public ASTNode
        {
        public:
            enum class ConstraintType
            {
                FOREIGN_KEY,
                PRIMARY_KEY,  // Reserved for future
                UNIQUE,       // Reserved for future
                CHECK         // Reserved for future
            };

            TableConstraint(const SourceSpan &span, ConstraintType type,
                           StringPool::StringId name = 0)
                : ASTNode(ASTKind::COLUMN_DEF, span), type_(type), name_(name)  // Reuse COLUMN_DEF kind for now
            {
            }

            ConstraintType constraintType() const { return type_; }
            StringPool::StringId name() const { return name_; }

            virtual void accept(ASTVisitor *visitor) override {}

        protected:
            ConstraintType type_;
            StringPool::StringId name_;  // Optional constraint name
        };

        // Foreign Key table constraint
        class ForeignKeyConstraint : public TableConstraint
        {
        public:
            ForeignKeyConstraint(const SourceSpan &span,
                                std::vector<StringPool::StringId> child_columns,
                                StringPool::StringId parent_table,
                                std::vector<StringPool::StringId> parent_columns,
                                StringPool::StringId on_delete = 0,
                                StringPool::StringId on_update = 0,
                                StringPool::StringId name = 0,
                                bool is_deferrable = false,
                                bool initially_deferred = false)
                : TableConstraint(span, ConstraintType::FOREIGN_KEY, name),
                  child_columns_(std::move(child_columns)),
                  parent_table_(parent_table),
                  parent_columns_(std::move(parent_columns)),
                  on_delete_(on_delete),
                  on_update_(on_update),
                  is_deferrable_(is_deferrable),
                  initially_deferred_(initially_deferred)
            {
            }

            const std::vector<StringPool::StringId> &childColumns() const { return child_columns_; }
            StringPool::StringId parentTable() const { return parent_table_; }
            const std::vector<StringPool::StringId> &parentColumns() const { return parent_columns_; }
            StringPool::StringId onDelete() const { return on_delete_; }
            StringPool::StringId onUpdate() const { return on_update_; }
            bool isDeferrable() const { return is_deferrable_; }
            bool initiallyDeferred() const { return initially_deferred_; }

        private:
            std::vector<StringPool::StringId> child_columns_;
            StringPool::StringId parent_table_;
            std::vector<StringPool::StringId> parent_columns_;
            StringPool::StringId on_delete_;
            StringPool::StringId on_update_;
            bool is_deferrable_;        // ALPHA Phase 1 - Deferred constraint checking
            bool initially_deferred_;   // ALPHA Phase 1 - Deferred constraint checking
        };

        // UNIQUE table constraint
        class UniqueConstraint : public TableConstraint
        {
        public:
            UniqueConstraint(const SourceSpan &span,
                            std::vector<StringPool::StringId> columns,
                            StringPool::StringId name = 0)
                : TableConstraint(span, ConstraintType::UNIQUE, name),
                  columns_(std::move(columns))
            {
            }

            const std::vector<StringPool::StringId> &columns() const { return columns_; }

        private:
            std::vector<StringPool::StringId> columns_;
        };

        // PRIMARY KEY table constraint
        class PrimaryKeyConstraint : public TableConstraint
        {
        public:
            PrimaryKeyConstraint(const SourceSpan &span,
                                std::vector<StringPool::StringId> columns,
                                StringPool::StringId name = 0)
                : TableConstraint(span, ConstraintType::PRIMARY_KEY, name),
                  columns_(std::move(columns))
            {
            }

            const std::vector<StringPool::StringId> &columns() const { return columns_; }

        private:
            std::vector<StringPool::StringId> columns_;
        };

        // CREATE TABLE statement
        class CreateTableStmt : public Statement
        {
        public:
            CreateTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                            std::vector<ColumnDef *> columns, StringPool::StringId charset = 0,
                            StringPool::StringId collation = 0, StringPool::StringId tablespace = 0,
                            std::vector<TableConstraint *> table_constraints = {})
                : Statement(ASTKind::CREATE_TABLE, span), table_name_(table_name),
                  columns_(std::move(columns)), charset_(charset), collation_(collation),
                  tablespace_(tablespace), table_constraints_(std::move(table_constraints))
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
            const std::vector<TableConstraint *> &tableConstraints() const  // Phase C - Composite FK
            {
                return table_constraints_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<ColumnDef *> columns_;
            StringPool::StringId charset_;    // DEFAULT CHARACTER SET clause
            StringPool::StringId collation_;  // DEFAULT COLLATE clause
            StringPool::StringId tablespace_; // TABLESPACE clause (Phase 2 Task 2.3)
            std::vector<TableConstraint *> table_constraints_;  // Table-level constraints (Phase C)
        };

        // CREATE INDEX statement (Phase 2 Task 2.3 + Task 17)
        class CreateIndexStmt : public Statement
        {
        public:
            // Task 17: Index column can be either a simple column or an expression
            struct IndexColumn
            {
                StringPool::StringId column_name;  // For simple column reference
                Expression *expression;            // For expression index (nullptr if simple column)
                bool is_expression;                // True if expression, false if column

                // Constructor for simple column
                IndexColumn(StringPool::StringId col)
                    : column_name(col), expression(nullptr), is_expression(false)
                {
                }

                // Constructor for expression
                IndexColumn(Expression *expr)
                    : column_name(0), expression(expr), is_expression(true)
                {
                }
            };

            // Constructor for backward compatibility (simple columns only)
            CreateIndexStmt(const SourceSpan &span, StringPool::StringId index_name,
                            StringPool::StringId table_name, std::vector<StringPool::StringId> columns,
                            bool is_unique = false, StringPool::StringId tablespace = 0,
                            StringPool::StringId index_type = 0)
                : Statement(ASTKind::CREATE_INDEX, span), index_name_(index_name),
                  table_name_(table_name), is_unique_(is_unique), tablespace_(tablespace),
                  where_clause_(nullptr), index_type_(index_type)
            {
                // Convert column names to IndexColumn structures
                for (auto col : columns)
                {
                    index_columns_.push_back(IndexColumn(col));
                }
            }

            // Task 17: New constructor supporting expressions and WHERE clause
            CreateIndexStmt(const SourceSpan &span, StringPool::StringId index_name,
                            StringPool::StringId table_name, std::vector<IndexColumn> index_columns,
                            Expression *where_clause = nullptr, bool is_unique = false,
                            StringPool::StringId tablespace = 0, StringPool::StringId index_type = 0)
                : Statement(ASTKind::CREATE_INDEX, span), index_name_(index_name),
                  table_name_(table_name), index_columns_(std::move(index_columns)),
                  where_clause_(where_clause), is_unique_(is_unique), tablespace_(tablespace),
                  index_type_(index_type)
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

            // Legacy column accessor (for backward compatibility)
            const std::vector<StringPool::StringId> columns() const
            {
                std::vector<StringPool::StringId> cols;
                for (const auto &ic : index_columns_)
                {
                    if (!ic.is_expression)
                    {
                        cols.push_back(ic.column_name);
                    }
                }
                return cols;
            }

            // Task 17: New accessors
            const std::vector<IndexColumn> &indexColumns() const
            {
                return index_columns_;
            }

            Expression *whereClause() const
            {
                return where_clause_;
            }

            bool hasWhereClause() const
            {
                return where_clause_ != nullptr;
            }

            bool hasExpressions() const
            {
                for (const auto &ic : index_columns_)
                {
                    if (ic.is_expression)
                        return true;
                }
                return false;
            }

            bool isUnique() const
            {
                return is_unique_;
            }
            StringPool::StringId tablespace() const
            {
                return tablespace_;
            }

            // LSM Integration: Index type accessor (Phase 2 Task 2.1)
            StringPool::StringId indexType() const
            {
                return index_type_;
            }

            bool hasIndexType() const
            {
                return index_type_ != 0;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId index_name_;
            StringPool::StringId table_name_;
            std::vector<IndexColumn> index_columns_;  // Task 17: Columns or expressions
            Expression *where_clause_;                // Task 17: WHERE clause for partial indexes
            bool is_unique_;
            StringPool::StringId tablespace_;
            StringPool::StringId index_type_;  // LSM Integration: Index type (e.g., "LSM", "BTREE")
        };

        // DROP TABLE statement (ALPHA Phase 1 - DDL Modifications)
        class DropTableStmt : public Statement
        {
        public:
            enum class DropBehavior : uint8_t
            {
                RESTRICT,  // Fail if dependencies exist (default)
                CASCADE    // Drop dependent objects recursively
            };

            DropTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                         bool if_exists = false, DropBehavior behavior = DropBehavior::RESTRICT)
                : Statement(ASTKind::DROP_TABLE, span), table_name_(table_name),
                  if_exists_(if_exists), drop_behavior_(behavior)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            bool ifExists() const
            {
                return if_exists_;
            }
            DropBehavior dropBehavior() const
            {
                return drop_behavior_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            bool if_exists_;           // IF EXISTS clause
            DropBehavior drop_behavior_; // CASCADE or RESTRICT
        };

        // DROP INDEX statement (ALPHA Phase 1 - DDL Modifications)
        class DropIndexStmt : public Statement
        {
        public:
            DropIndexStmt(const SourceSpan &span, StringPool::StringId index_name,
                         bool if_exists = false)
                : Statement(ASTKind::DROP_INDEX, span), index_name_(index_name),
                  if_exists_(if_exists)
            {
            }

            StringPool::StringId indexName() const
            {
                return index_name_;
            }
            bool ifExists() const
            {
                return if_exists_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId index_name_;
            bool if_exists_;  // IF EXISTS clause
        };

        // TRUNCATE TABLE statement (ALPHA Phase 1 - DDL Modifications - final operation)
        class TruncateTableStmt : public Statement
        {
        public:
            enum class TruncateMode : uint8_t
            {
                ASYNC = 0,  // Background job (default, non-blocking)
                SYNC = 1    // Block until complete
            };

            TruncateTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                             TruncateMode mode = TruncateMode::ASYNC)
                : Statement(ASTKind::TRUNCATE_TABLE, span), table_name_(table_name), mode_(mode)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }

            TruncateMode mode() const
            {
                return mode_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            TruncateMode mode_;
        };

        // ALTER TABLE statement (ALPHA Phase 1 - DDL Modifications)
        class AlterTableStmt : public Statement
        {
        public:
            enum class AlterAction : uint8_t
            {
                ADD_COLUMN,
                DROP_COLUMN,
                ALTER_COLUMN_TYPE,
                ALTER_COLUMN_SET_DEFAULT,
                ALTER_COLUMN_DROP_DEFAULT,
                RENAME_COLUMN,
                ADD_CONSTRAINT,
                DROP_CONSTRAINT
            };

            enum class DropBehavior : uint8_t
            {
                RESTRICT,  // Fail if dependencies exist (default)
                CASCADE    // Drop dependent objects recursively
            };

            AlterTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                          AlterAction action)
                : Statement(ASTKind::ALTER_TABLE, span), table_name_(table_name),
                  action_(action), column_def_(nullptr), new_type_(nullptr),
                  default_expr_(nullptr),
                  old_column_name_(0), new_column_name_(0), constraint_name_(0),
                  if_exists_(false), drop_behavior_(DropBehavior::RESTRICT)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }
            AlterAction action() const
            {
                return action_;
            }

            // ADD COLUMN accessors
            void setColumnDef(ColumnDef *col_def)
            {
                column_def_ = col_def;
            }
            ColumnDef *columnDef() const
            {
                return column_def_;
            }

            // DROP COLUMN accessors
            void setDropColumnName(StringPool::StringId col_name, bool if_exists,
                                  DropBehavior behavior)
            {
                old_column_name_ = col_name;
                if_exists_ = if_exists;
                drop_behavior_ = behavior;
            }
            StringPool::StringId dropColumnName() const
            {
                return old_column_name_;
            }

            // ALTER COLUMN TYPE accessors
            void setAlterColumnType(StringPool::StringId col_name, TypeName *new_type)
            {
                old_column_name_ = col_name;
                new_type_ = new_type;
            }
            TypeName *newType() const
            {
                return new_type_;
            }

            // ALTER COLUMN SET/DROP DEFAULT accessors
            void setAlterColumnDefault(StringPool::StringId col_name, Expression *default_expr)
            {
                old_column_name_ = col_name;
                default_expr_ = default_expr;
            }
            Expression *defaultExpr() const
            {
                return default_expr_;
            }

            // RENAME COLUMN accessors
            void setRenameColumn(StringPool::StringId old_name, StringPool::StringId new_name)
            {
                old_column_name_ = old_name;
                new_column_name_ = new_name;
            }
            StringPool::StringId oldColumnName() const
            {
                return old_column_name_;
            }
            StringPool::StringId newColumnName() const
            {
                return new_column_name_;
            }

            // ADD/DROP CONSTRAINT accessors (TODO: implement when TableConstraint is defined)
            void setConstraintName(StringPool::StringId constraint_name)
            {
                constraint_name_ = constraint_name;
            }
            StringPool::StringId constraintName() const
            {
                return constraint_name_;
            }

            bool ifExists() const
            {
                return if_exists_;
            }
            DropBehavior dropBehavior() const
            {
                return drop_behavior_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            AlterAction action_;

            // ADD COLUMN data
            ColumnDef *column_def_;

            // ALTER COLUMN TYPE data
            TypeName *new_type_;

            // ALTER COLUMN DEFAULT data
            Expression *default_expr_;

            // Column names
            StringPool::StringId old_column_name_;
            StringPool::StringId new_column_name_;
            StringPool::StringId constraint_name_;

            // DROP modifiers
            bool if_exists_;
            DropBehavior drop_behavior_;
        };

        // CREATE SEQUENCE statement (ALPHA Phase 1 - Sequences)
        class CreateSequenceStmt : public Statement
        {
        public:
            CreateSequenceStmt(const SourceSpan& span, StringPool::StringId name)
                : Statement(ASTKind::CREATE_SEQUENCE, span), name_(name),
                  increment_by_(nullptr), min_value_(nullptr), max_value_(nullptr),
                  start_with_(nullptr), cache_(nullptr),
                  cycle_(false), no_min_value_(false), no_max_value_(false)
            {
            }

            StringPool::StringId name() const { return name_; }

            // Optional parameters (nullptr if not specified)
            void setIncrementBy(Expression* expr) { increment_by_ = expr; }
            void setMinValue(Expression* expr) { min_value_ = expr; }
            void setMaxValue(Expression* expr) { max_value_ = expr; }
            void setStartWith(Expression* expr) { start_with_ = expr; }
            void setCache(Expression* expr) { cache_ = expr; }
            void setCycle(bool cycle) { cycle_ = cycle; }
            void setNoMinValue(bool no_min) { no_min_value_ = no_min; }
            void setNoMaxValue(bool no_max) { no_max_value_ = no_max; }

            Expression* incrementBy() const { return increment_by_; }
            Expression* minValue() const { return min_value_; }
            Expression* maxValue() const { return max_value_; }
            Expression* startWith() const { return start_with_; }
            Expression* cache() const { return cache_; }
            bool cycle() const { return cycle_; }
            bool noMinValue() const { return no_min_value_; }
            bool noMaxValue() const { return no_max_value_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            Expression* increment_by_;
            Expression* min_value_;
            Expression* max_value_;
            Expression* start_with_;
            Expression* cache_;
            bool cycle_;
            bool no_min_value_;
            bool no_max_value_;
        };

        // ALTER SEQUENCE statement (ALPHA Phase 1 - Sequences)
        class AlterSequenceStmt : public Statement
        {
        public:
            AlterSequenceStmt(const SourceSpan& span, StringPool::StringId name)
                : Statement(ASTKind::ALTER_SEQUENCE, span), name_(name),
                  increment_by_(nullptr), min_value_(nullptr), max_value_(nullptr),
                  restart_(nullptr), cache_(nullptr),
                  has_cycle_(false), cycle_(false),
                  no_min_value_(false), no_max_value_(false)
            {
            }

            StringPool::StringId name() const { return name_; }

            // Same setters as CreateSequenceStmt
            void setIncrementBy(Expression* expr) { increment_by_ = expr; }
            void setMinValue(Expression* expr) { min_value_ = expr; }
            void setMaxValue(Expression* expr) { max_value_ = expr; }
            void setRestart(Expression* expr) { restart_ = expr; }
            void setCache(Expression* expr) { cache_ = expr; }
            void setCycle(bool cycle) { cycle_ = cycle; has_cycle_ = true; }
            void setNoMinValue(bool no_min) { no_min_value_ = no_min; }
            void setNoMaxValue(bool no_max) { no_max_value_ = no_max; }

            Expression* incrementBy() const { return increment_by_; }
            Expression* minValue() const { return min_value_; }
            Expression* maxValue() const { return max_value_; }
            Expression* restart() const { return restart_; }
            Expression* cache() const { return cache_; }
            bool hasCycle() const { return has_cycle_; }
            bool cycle() const { return cycle_; }
            bool noMinValue() const { return no_min_value_; }
            bool noMaxValue() const { return no_max_value_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            Expression* increment_by_;
            Expression* min_value_;
            Expression* max_value_;
            Expression* restart_;
            Expression* cache_;
            bool has_cycle_;
            bool cycle_;
            bool no_min_value_;
            bool no_max_value_;
        };

        // DROP SEQUENCE statement (ALPHA Phase 1 - Sequences)
        class DropSequenceStmt : public Statement
        {
        public:
            DropSequenceStmt(const SourceSpan& span, StringPool::StringId name,
                             bool if_exists, bool cascade)
                : Statement(ASTKind::DROP_SEQUENCE, span),
                  name_(name), if_exists_(if_exists), cascade_(cascade)
            {
            }

            StringPool::StringId name() const { return name_; }
            bool ifExists() const { return if_exists_; }
            bool cascade() const { return cascade_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            bool if_exists_;
            bool cascade_;
        };

        // CREATE VIEW statement (ALPHA Phase 1 - Views)
        class CreateViewStmt : public Statement
        {
        public:
            CreateViewStmt(const SourceSpan& span, StringPool::StringId name,
                           SelectStmt* query, bool or_replace = false, bool materialized = false)
                : Statement(ASTKind::CREATE_VIEW, span),
                  name_(name), query_(query), or_replace_(or_replace),
                  check_option_(false), materialized_(materialized)
            {
            }

            StringPool::StringId name() const { return name_; }
            SelectStmt* query() const { return query_; }
            bool orReplace() const { return or_replace_; }
            bool checkOption() const { return check_option_; }
            bool materialized() const { return materialized_; }  // ALPHA Phase 1 - Materialized Views

            const std::vector<StringPool::StringId>& columnNames() const
            {
                return column_names_;
            }

            void setCheckOption(bool check) { check_option_ = check; }
            void setColumnNames(std::vector<StringPool::StringId> names)
            {
                column_names_ = std::move(names);
            }

            // ALPHA Phase 1 - Views: Store the actual SELECT query text
            const std::string& queryDefinitionText() const { return query_definition_text_; }
            void setQueryDefinitionText(std::string text)
            {
                query_definition_text_ = std::move(text);
            }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            SelectStmt* query_;
            bool or_replace_;
            bool check_option_;
            bool materialized_;  // ALPHA Phase 1 - Materialized Views
            std::vector<StringPool::StringId> column_names_;
            std::string query_definition_text_;  // ALPHA Phase 1 - Views: Actual SELECT text
        };

        // DROP VIEW statement (ALPHA Phase 1 - Views)
        class DropViewStmt : public Statement
        {
        public:
            DropViewStmt(const SourceSpan& span, StringPool::StringId name,
                         bool if_exists, bool cascade)
                : Statement(ASTKind::DROP_VIEW, span),
                  name_(name), if_exists_(if_exists), cascade_(cascade)
            {
            }

            StringPool::StringId name() const { return name_; }
            bool ifExists() const { return if_exists_; }
            bool cascade() const { return cascade_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            bool if_exists_;
            bool cascade_;
        };

        // REFRESH MATERIALIZED VIEW statement (ALPHA Phase 1 - Materialized Views)
        class RefreshMaterializedViewStmt : public Statement
        {
        public:
            RefreshMaterializedViewStmt(const SourceSpan& span, StringPool::StringId name,
                                       bool concurrently = false)
                : Statement(ASTKind::REFRESH_MATERIALIZED_VIEW, span),
                  name_(name), concurrently_(concurrently)
            {
            }

            StringPool::StringId name() const { return name_; }
            bool concurrently() const { return concurrently_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            bool concurrently_;  // CONCURRENTLY option for non-blocking refresh
        };

        // INSERT statement
        class InsertStmt : public Statement
        {
        public:
            InsertStmt(const SourceSpan &span, StringPool::StringId table_name,
                       std::vector<StringPool::StringId> columns, std::vector<Expression *> values,
                       bool has_returning = false, std::vector<StringPool::StringId> returning_columns = {})
                : Statement(ASTKind::INSERT, span), table_name_(table_name),
                  columns_(std::move(columns)), values_(std::move(values)),
                  has_returning_(has_returning), returning_columns_(std::move(returning_columns))
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
            bool hasReturning() const
            {
                return has_returning_;
            }
            const std::vector<StringPool::StringId> &returningColumns() const
            {
                return returning_columns_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<StringPool::StringId> columns_;
            std::vector<Expression *> values_;
            bool has_returning_;
            std::vector<StringPool::StringId> returning_columns_;
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
            bool recursive; // True if this is a recursive CTE

            CTEDefinition(StringPool::StringId n, SelectStmt *q,
                          std::vector<StringPool::StringId> aliases = {},
                          bool is_recursive = false)
                : name(n), query(q), column_aliases(std::move(aliases)), recursive(is_recursive)
            {
            }
        };

        // WITH clause (CTE support) - Phase 2 Wave 2
        class WithClause
        {
        public:
            WithClause(std::vector<CTEDefinition> ctes, bool is_recursive = false)
                : ctes_(std::move(ctes)), recursive_(is_recursive)
            {
            }

            const std::vector<CTEDefinition> &ctes() const
            {
                return ctes_;
            }

            bool isRecursive() const
            {
                return recursive_;
            }

        private:
            std::vector<CTEDefinition> ctes_;
            bool recursive_;
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

        // GROUP BY clause (Phase 1 Task 4.1, Phase 3: Advanced Grouping)

        // Grouping type for advanced GROUP BY features
        enum class GroupingType : uint8_t
        {
            STANDARD,        // Regular GROUP BY
            ROLLUP,          // GROUP BY ROLLUP(...)
            CUBE,            // GROUP BY CUBE(...)
            GROUPING_SETS    // GROUP BY GROUPING SETS(...)
        };

        struct GroupByClause
        {
            GroupingType type;                                  // Type of grouping
            std::vector<Expression *> grouping_exprs;          // Simple GROUP BY expressions
            std::vector<std::vector<Expression *>> grouping_sets;  // For GROUPING SETS - list of grouping sets
            Expression *having_clause;                          // Optional HAVING condition

            GroupByClause() : type(GroupingType::STANDARD), having_clause(nullptr) {}

            explicit GroupByClause(std::vector<Expression *> exprs, Expression *having = nullptr)
                : type(GroupingType::STANDARD), grouping_exprs(std::move(exprs)), having_clause(having) {}

            // Constructor for advanced grouping (ROLLUP, CUBE, GROUPING SETS)
            GroupByClause(GroupingType grp_type, std::vector<Expression *> exprs, Expression *having = nullptr)
                : type(grp_type), grouping_exprs(std::move(exprs)), having_clause(having) {}

            // Constructor for explicit GROUPING SETS with multiple sets
            GroupByClause(std::vector<std::vector<Expression *>> sets, Expression *having = nullptr)
                : type(GroupingType::GROUPING_SETS), grouping_sets(std::move(sets)), having_clause(having) {}
        };

        // Set operation type (UNION, INTERSECT, EXCEPT)
        enum class SetOperationType : uint8_t
        {
            UNION,          // UNION (removes duplicates)
            UNION_ALL,      // UNION ALL (keeps duplicates)
            INTERSECT,      // INTERSECT (removes duplicates)
            INTERSECT_ALL,  // INTERSECT ALL (keeps duplicates)
            EXCEPT,         // EXCEPT (removes duplicates)
            EXCEPT_ALL      // EXCEPT ALL (keeps duplicates)
        };

        // Forward declare SelectStmt for SetOperationStmt
        class SelectStmt;

        // Set operation statement (UNION/INTERSECT/EXCEPT)
        class SetOperationStmt : public Statement
        {
        public:
            SetOperationStmt(const SourceSpan &span, SetOperationType op_type,
                           Statement *left, Statement *right)
                : Statement(ASTKind::SET_OPERATION, span),
                  op_type_(op_type), left_(left), right_(right),
                  limit_count_(-1), offset_count_(-1)
            {
            }

            SetOperationType opType() const { return op_type_; }
            Statement *left() const { return left_; }
            Statement *right() const { return right_; }

            // ORDER BY accessors (applies to final result)
            const std::vector<OrderByItem> &orderByClause() const { return order_by_clause_; }
            void setOrderByClause(std::vector<OrderByItem> clause) { order_by_clause_ = std::move(clause); }

            // LIMIT/OFFSET accessors (applies to final result)
            bool hasLimit() const { return limit_count_ >= 0; }
            int64_t limitCount() const { return limit_count_; }
            void setLimitCount(int64_t count) { limit_count_ = count; }

            bool hasOffset() const { return offset_count_ >= 0; }
            int64_t offsetCount() const { return offset_count_; }
            void setOffsetCount(int64_t count) { offset_count_ = count; }

            void accept(ASTVisitor *visitor) override;

        private:
            SetOperationType op_type_;
            Statement *left_;   // Left SELECT or SetOperationStmt
            Statement *right_;  // Right SELECT or SetOperationStmt

            // ORDER BY/LIMIT/OFFSET apply to final result
            std::vector<OrderByItem> order_by_clause_;
            int64_t limit_count_;
            int64_t offset_count_;
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

            // Phase 3.4.7: RLS predicate injection
            void setWhereClause(Expression *where_clause)
            {
                where_clause_ = where_clause;
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
                       std::vector<Assignment> assignments, Expression *where_clause = nullptr,
                       bool has_returning = false, std::vector<StringPool::StringId> returning_columns = {})
                : Statement(ASTKind::UPDATE, span), table_name_(table_name),
                  assignments_(std::move(assignments)), where_clause_(where_clause),
                  has_returning_(has_returning), returning_columns_(std::move(returning_columns))
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
            bool hasReturning() const
            {
                return has_returning_;
            }
            const std::vector<StringPool::StringId> &returningColumns() const
            {
                return returning_columns_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<Assignment> assignments_;
            Expression *where_clause_;
            bool has_returning_;
            std::vector<StringPool::StringId> returning_columns_;
        };

        // DELETE statement (Phase 1 Task 2.2)
        class DeleteStmt : public Statement
        {
        public:
            DeleteStmt(const SourceSpan &span, StringPool::StringId table_name,
                       Expression *where_clause = nullptr,
                       bool has_returning = false, std::vector<StringPool::StringId> returning_columns = {})
                : Statement(ASTKind::DELETE_STMT, span), table_name_(table_name),
                  where_clause_(where_clause),
                  has_returning_(has_returning), returning_columns_(std::move(returning_columns))
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
            bool hasReturning() const
            {
                return has_returning_;
            }
            const std::vector<StringPool::StringId> &returningColumns() const
            {
                return returning_columns_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            Expression *where_clause_;
            bool has_returning_;
            std::vector<StringPool::StringId> returning_columns_;
        };

        // MERGE statement (Alpha 1 - Advanced SQL)
        class MergeStmt : public Statement
        {
        public:
            struct WhenClause
            {
                enum Type {
                    MATCHED,              // WHEN MATCHED THEN UPDATE
                    NOT_MATCHED,          // WHEN NOT MATCHED THEN INSERT
                    NOT_MATCHED_BY_SOURCE // WHEN NOT MATCHED BY SOURCE THEN DELETE
                };

                Type type;
                Expression* condition;  // Optional additional condition

                // For UPDATE
                std::vector<Assignment> assignments;

                // For INSERT
                std::vector<StringPool::StringId> insert_columns;
                std::vector<Expression*> insert_values;
            };

            MergeStmt(const SourceSpan& span,
                      StringPool::StringId target_table,
                      Expression* source,  // Can be table or subquery
                      Expression* on_condition,
                      const std::vector<WhenClause>& when_clauses)
                : Statement(ASTKind::MERGE, span),
                  target_table_(target_table),
                  source_(source),
                  on_condition_(on_condition),
                  when_clauses_(when_clauses)
            {
            }

            StringPool::StringId targetTable() const { return target_table_; }
            Expression* source() const { return source_; }
            Expression* onCondition() const { return on_condition_; }
            const std::vector<WhenClause>& whenClauses() const { return when_clauses_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId target_table_;
            Expression* source_;
            Expression* on_condition_;
            std::vector<WhenClause> when_clauses_;
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

        // SHOW statement (ALPHA Phase 1 - Developer Experience)
        enum class ShowObjectType : uint8_t
        {
            TABLES,         // SHOW TABLES
            DATABASES,      // SHOW DATABASES / SHOW SCHEMAS
            COLUMNS,        // SHOW COLUMNS FROM table
            INDEXES,        // SHOW INDEXES FROM table
            CREATE_TABLE    // SHOW CREATE TABLE table
        };

        class ShowStmt : public Statement
        {
        public:
            /**
             * Constructor
             *
             * @param span Source location
             * @param object_type Type of object to show (TABLES, DATABASES, etc.)
             * @param table_name Table name (for SHOW COLUMNS/INDEXES/CREATE TABLE)
             * @param database_name Database name (for SHOW TABLES FROM database)
             * @param like_pattern LIKE pattern for filtering results
             */
            ShowStmt(const SourceSpan &span,
                     ShowObjectType object_type,
                     StringPool::StringId table_name = 0,
                     StringPool::StringId database_name = 0,
                     StringPool::StringId like_pattern = 0)
                : Statement(ASTKind::SHOW, span),
                  object_type_(object_type),
                  table_name_(table_name),
                  database_name_(database_name),
                  like_pattern_(like_pattern)
            {
            }

            ShowObjectType objectType() const
            {
                return object_type_;
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }

            StringPool::StringId databaseName() const
            {
                return database_name_;
            }

            StringPool::StringId likePattern() const
            {
                return like_pattern_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            ShowObjectType object_type_;
            StringPool::StringId table_name_;
            StringPool::StringId database_name_;
            StringPool::StringId like_pattern_;
        };

        // DESCRIBE statement (ALPHA Phase 1 - Developer Experience)
        // Alias for SHOW COLUMNS FROM table
        class DescribeStmt : public Statement
        {
        public:
            /**
             * Constructor
             *
             * @param span Source location
             * @param table_name Table name to describe
             */
            DescribeStmt(const SourceSpan &span, StringPool::StringId table_name)
                : Statement(ASTKind::DESCRIBE, span), table_name_(table_name)
            {
            }

            StringPool::StringId tableName() const
            {
                return table_name_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
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

        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) =====

        // Parameter mode for functions/procedures
        enum class ParameterMode : uint8_t
        {
            IN,
            OUT,
            INOUT
        };

        // Parameter declaration
        struct Parameter
        {
            StringPool::StringId name;
            TypeName* type;
            ParameterMode mode;
            Expression* default_value;  // Optional

            Parameter(StringPool::StringId n, TypeName* t, ParameterMode m = ParameterMode::IN, Expression* def = nullptr)
                : name(n), type(t), mode(m), default_value(def) {}
        };

        // Variable declaration statement
        class VarDeclarationStmt : public Statement
        {
        public:
            VarDeclarationStmt(const SourceSpan& span,
                              StringPool::StringId name,
                              TypeName* type,
                              bool is_constant = false,
                              Expression* default_value = nullptr)
                : Statement(ASTKind::VAR_DECLARATION, span),
                  name_(name),
                  type_(type),
                  is_constant_(is_constant),
                  default_value_(default_value)
            {
            }

            StringPool::StringId name() const { return name_; }
            TypeName* type() const { return type_; }
            bool isConstant() const { return is_constant_; }
            Expression* defaultValue() const { return default_value_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            TypeName* type_;
            bool is_constant_;
            Expression* default_value_;
        };

        // Exception handler
        struct ExceptionHandler
        {
            StringPool::StringId exception_name;  // Can be 0 for OTHERS
            std::vector<Statement*> statements;

            ExceptionHandler(StringPool::StringId name, std::vector<Statement*> stmts)
                : exception_name(name), statements(std::move(stmts)) {}
        };

        // Block statement (BEGIN...END)
        class BlockStmt : public Statement
        {
        public:
            BlockStmt(const SourceSpan& span,
                     std::vector<VarDeclarationStmt*> declarations,
                     std::vector<Statement*> statements,
                     std::vector<ExceptionHandler*> exception_handlers = {})
                : Statement(ASTKind::BLOCK, span),
                  declarations_(std::move(declarations)),
                  statements_(std::move(statements)),
                  exception_handlers_(std::move(exception_handlers))
            {
            }

            const std::vector<VarDeclarationStmt*>& declarations() const { return declarations_; }
            const std::vector<Statement*>& statements() const { return statements_; }
            const std::vector<ExceptionHandler*>& exceptionHandlers() const { return exception_handlers_; }

            void accept(ASTVisitor* visitor) override;

        private:
            std::vector<VarDeclarationStmt*> declarations_;
            std::vector<Statement*> statements_;
            std::vector<ExceptionHandler*> exception_handlers_;
        };

        // Assignment statement
        class AssignmentStmt : public Statement
        {
        public:
            AssignmentStmt(const SourceSpan& span,
                          StringPool::StringId variable,
                          Expression* value)
                : Statement(ASTKind::ASSIGNMENT, span),
                  variable_(variable),
                  value_(value)
            {
            }

            StringPool::StringId variable() const { return variable_; }
            Expression* value() const { return value_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId variable_;
            Expression* value_;
        };

        // ELSIF clause for IF statement
        struct ElsIfClause
        {
            Expression* condition;
            std::vector<Statement*> statements;

            ElsIfClause(Expression* cond, std::vector<Statement*> stmts)
                : condition(cond), statements(std::move(stmts)) {}
        };

        // IF statement
        class IfStmt : public Statement
        {
        public:
            IfStmt(const SourceSpan& span,
                  Expression* condition,
                  std::vector<Statement*> then_stmts,
                  std::vector<ElsIfClause*> elsif_clauses = {},
                  std::vector<Statement*> else_stmts = {})
                : Statement(ASTKind::IF_STMT, span),
                  condition_(condition),
                  then_stmts_(std::move(then_stmts)),
                  elsif_clauses_(std::move(elsif_clauses)),
                  else_stmts_(std::move(else_stmts))
            {
            }

            Expression* condition() const { return condition_; }
            const std::vector<Statement*>& thenStatements() const { return then_stmts_; }
            const std::vector<ElsIfClause*>& elsifClauses() const { return elsif_clauses_; }
            const std::vector<Statement*>& elseStatements() const { return else_stmts_; }

            void accept(ASTVisitor* visitor) override;

        private:
            Expression* condition_;
            std::vector<Statement*> then_stmts_;
            std::vector<ElsIfClause*> elsif_clauses_;
            std::vector<Statement*> else_stmts_;
        };

        // LOOP statement
        class LoopStmt : public Statement
        {
        public:
            LoopStmt(const SourceSpan& span,
                    StringPool::StringId label,
                    std::vector<Statement*> statements)
                : Statement(ASTKind::LOOP_STMT, span),
                  label_(label),
                  statements_(std::move(statements))
            {
            }

            StringPool::StringId label() const { return label_; }
            const std::vector<Statement*>& statements() const { return statements_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId label_;
            std::vector<Statement*> statements_;
        };

        // WHILE statement
        class WhileStmt : public Statement
        {
        public:
            WhileStmt(const SourceSpan& span,
                     StringPool::StringId label,
                     Expression* condition,
                     std::vector<Statement*> statements)
                : Statement(ASTKind::WHILE_STMT, span),
                  label_(label),
                  condition_(condition),
                  statements_(std::move(statements))
            {
            }

            StringPool::StringId label() const { return label_; }
            Expression* condition() const { return condition_; }
            const std::vector<Statement*>& statements() const { return statements_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId label_;
            Expression* condition_;
            std::vector<Statement*> statements_;
        };

        // EXIT statement
        class ExitStmt : public Statement
        {
        public:
            ExitStmt(const SourceSpan& span,
                    StringPool::StringId label = 0,
                    Expression* when_condition = nullptr)
                : Statement(ASTKind::EXIT_STMT, span),
                  label_(label),
                  when_condition_(when_condition)
            {
            }

            StringPool::StringId label() const { return label_; }
            Expression* whenCondition() const { return when_condition_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId label_;
            Expression* when_condition_;
        };

        // RETURN statement
        class ReturnStmt : public Statement
        {
        public:
            ReturnStmt(const SourceSpan& span, Expression* return_value = nullptr)
                : Statement(ASTKind::RETURN_STMT, span),
                  return_value_(return_value)
            {
            }

            Expression* returnValue() const { return return_value_; }

            void accept(ASTVisitor* visitor) override;

        private:
            Expression* return_value_;
        };

        // RAISE statement
        class RaiseStmt : public Statement
        {
        public:
            enum class Level
            {
                EXCEPTION,
                NOTICE,
                WARNING,
                INFO,
                DEBUG
            };

            RaiseStmt(const SourceSpan& span,
                     Level level,
                     Expression* message,
                     std::vector<Expression*> args = {})
                : Statement(ASTKind::RAISE_STMT, span),
                  level_(level),
                  message_(message),
                  args_(std::move(args))
            {
            }

            Level level() const { return level_; }
            Expression* message() const { return message_; }
            const std::vector<Expression*>& args() const { return args_; }

            void accept(ASTVisitor* visitor) override;

        private:
            Level level_;
            Expression* message_;
            std::vector<Expression*> args_;
        };

        // CREATE FUNCTION statement
        class CreateFunctionStmt : public Statement
        {
        public:
            enum class SqlSecurity : uint8_t {
                DEFINER = 0,  // Execute with owner's privileges
                INVOKER = 1   // Execute with caller's privileges (default)
            };

            CreateFunctionStmt(const SourceSpan& span,
                              StringPool::StringId name,
                              std::vector<Parameter*> parameters,
                              TypeName* return_type,
                              bool or_replace,
                              BlockStmt* body,
                              SqlSecurity sql_security = SqlSecurity::INVOKER)
                : Statement(ASTKind::CREATE_FUNCTION, span),
                  name_(name),
                  parameters_(std::move(parameters)),
                  return_type_(return_type),
                  or_replace_(or_replace),
                  body_(body),
                  sql_security_(sql_security)
            {
            }

            StringPool::StringId name() const { return name_; }
            const std::vector<Parameter*>& parameters() const { return parameters_; }
            TypeName* returnType() const { return return_type_; }
            bool orReplace() const { return or_replace_; }
            BlockStmt* body() const { return body_; }
            SqlSecurity sqlSecurity() const { return sql_security_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            std::vector<Parameter*> parameters_;
            TypeName* return_type_;
            bool or_replace_;
            BlockStmt* body_;
            SqlSecurity sql_security_;  // Phase 3.1: SQL SECURITY DEFINER/INVOKER
        };

        // CREATE PROCEDURE statement
        class CreateProcedureStmt : public Statement
        {
        public:
            enum class SqlSecurity : uint8_t {
                DEFINER = 0,  // Execute with owner's privileges
                INVOKER = 1   // Execute with caller's privileges (default)
            };

            CreateProcedureStmt(const SourceSpan& span,
                               StringPool::StringId name,
                               std::vector<Parameter*> parameters,
                               bool or_replace,
                               BlockStmt* body,
                               SqlSecurity sql_security = SqlSecurity::INVOKER)
                : Statement(ASTKind::CREATE_PROCEDURE, span),
                  name_(name),
                  parameters_(std::move(parameters)),
                  or_replace_(or_replace),
                  body_(body),
                  sql_security_(sql_security)
            {
            }

            StringPool::StringId name() const { return name_; }
            const std::vector<Parameter*>& parameters() const { return parameters_; }
            bool orReplace() const { return or_replace_; }
            BlockStmt* body() const { return body_; }
            SqlSecurity sqlSecurity() const { return sql_security_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId name_;
            std::vector<Parameter*> parameters_;
            bool or_replace_;
            BlockStmt* body_;
            SqlSecurity sql_security_;  // Phase 3.1: SQL SECURITY DEFINER/INVOKER
        };

        // ===== Security Statements (ALPHA Phase 1 - Security System Phase 2) =====

        // CREATE USER username [WITH PASSWORD 'password'] [SUPERUSER | NOSUPERUSER]
        class CreateUserStmt : public Statement
        {
        public:
            CreateUserStmt(const SourceSpan& span,
                          StringPool::StringId username,
                          StringPool::StringId password,  // 0 if no password
                          bool has_password,
                          bool is_superuser)
                : Statement(ASTKind::CREATE_USER, span),
                  username_(username),
                  password_(password),
                  has_password_(has_password),
                  is_superuser_(is_superuser)
            {
            }

            StringPool::StringId username() const { return username_; }
            StringPool::StringId password() const { return password_; }
            bool hasPassword() const { return has_password_; }
            bool isSuperuser() const { return is_superuser_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId username_;
            StringPool::StringId password_;
            bool has_password_;
            bool is_superuser_;
        };

        // ALTER USER username [WITH PASSWORD 'password'] [SUPERUSER | NOSUPERUSER]
        class AlterUserStmt : public Statement
        {
        public:
            AlterUserStmt(const SourceSpan& span,
                         StringPool::StringId username,
                         StringPool::StringId password,  // 0 if not changing
                         bool change_password,
                         bool is_superuser,
                         bool change_superuser)
                : Statement(ASTKind::ALTER_USER, span),
                  username_(username),
                  password_(password),
                  change_password_(change_password),
                  is_superuser_(is_superuser),
                  change_superuser_(change_superuser)
            {
            }

            StringPool::StringId username() const { return username_; }
            StringPool::StringId password() const { return password_; }
            bool changePassword() const { return change_password_; }
            bool isSuperuser() const { return is_superuser_; }
            bool changeSuperuser() const { return change_superuser_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId username_;
            StringPool::StringId password_;
            bool change_password_;
            bool is_superuser_;
            bool change_superuser_;
        };

        // DROP USER username [IF EXISTS] [CASCADE | RESTRICT]
        class DropUserStmt : public Statement
        {
        public:
            enum class DropBehavior : uint8_t
            {
                RESTRICT,  // Fail if user owns objects
                CASCADE    // Drop user and transfer/drop owned objects
            };

            DropUserStmt(const SourceSpan& span,
                        StringPool::StringId username,
                        bool if_exists,
                        DropBehavior behavior)
                : Statement(ASTKind::DROP_USER, span),
                  username_(username),
                  if_exists_(if_exists),
                  drop_behavior_(behavior)
            {
            }

            StringPool::StringId username() const { return username_; }
            bool ifExists() const { return if_exists_; }
            DropBehavior dropBehavior() const { return drop_behavior_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId username_;
            bool if_exists_;
            DropBehavior drop_behavior_;
        };

        // CREATE ROLE rolename
        class CreateRoleStmt : public Statement
        {
        public:
            CreateRoleStmt(const SourceSpan& span,
                          StringPool::StringId rolename)
                : Statement(ASTKind::CREATE_ROLE, span),
                  rolename_(rolename)
            {
            }

            StringPool::StringId rolename() const { return rolename_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId rolename_;
        };

        // DROP ROLE rolename [IF EXISTS] [CASCADE | RESTRICT]
        class DropRoleStmt : public Statement
        {
        public:
            enum class DropBehavior : uint8_t
            {
                RESTRICT,
                CASCADE
            };

            DropRoleStmt(const SourceSpan& span,
                        StringPool::StringId rolename,
                        bool if_exists,
                        DropBehavior behavior)
                : Statement(ASTKind::DROP_ROLE, span),
                  rolename_(rolename),
                  if_exists_(if_exists),
                  drop_behavior_(behavior)
            {
            }

            StringPool::StringId rolename() const { return rolename_; }
            bool ifExists() const { return if_exists_; }
            DropBehavior dropBehavior() const { return drop_behavior_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId rolename_;
            bool if_exists_;
            DropBehavior drop_behavior_;
        };

        // CREATE GROUP groupname
        class CreateGroupStmt : public Statement
        {
        public:
            CreateGroupStmt(const SourceSpan& span,
                           StringPool::StringId groupname)
                : Statement(ASTKind::CREATE_GROUP, span),
                  groupname_(groupname)
            {
            }

            StringPool::StringId groupname() const { return groupname_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId groupname_;
        };

        // DROP GROUP groupname [IF EXISTS] [CASCADE | RESTRICT]
        class DropGroupStmt : public Statement
        {
        public:
            enum class DropBehavior : uint8_t
            {
                RESTRICT,
                CASCADE
            };

            DropGroupStmt(const SourceSpan& span,
                         StringPool::StringId groupname,
                         bool if_exists,
                         DropBehavior behavior)
                : Statement(ASTKind::DROP_GROUP, span),
                  groupname_(groupname),
                  if_exists_(if_exists),
                  drop_behavior_(behavior)
            {
            }

            StringPool::StringId groupname() const { return groupname_; }
            bool ifExists() const { return if_exists_; }
            DropBehavior dropBehavior() const { return drop_behavior_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId groupname_;
            bool if_exists_;
            DropBehavior drop_behavior_;
        };

        // GRANT privilege ON object TO grantee [WITH GRANT OPTION]
        class GrantPrivilegeStmt : public Statement
        {
        public:
            enum class PrivilegeType : uint32_t
            {
                SELECT    = 0x00000001,
                INSERT    = 0x00000002,
                UPDATE    = 0x00000004,
                DELETE    = 0x00000008,
                TRUNCATE  = 0x00000010,
                REFERENCES= 0x00000020,
                TRIGGER   = 0x00000040,
                CREATE    = 0x00000080,
                USAGE     = 0x00000100,
                EXECUTE   = 0x00000800,
                CONNECT   = 0x00001000,
                ALL       = 0xFFFFFFFF
            };

            enum class ObjectType : uint8_t
            {
                TABLE,
                VIEW,
                SEQUENCE,
                FUNCTION,
                PROCEDURE,
                SCHEMA,
                DATABASE,
                DOMAIN
            };

            enum class GranteeType : uint8_t
            {
                USER,
                ROLE,
                GROUP,
                PUBLIC
            };

            GrantPrivilegeStmt(const SourceSpan& span,
                             uint32_t privileges,
                             ObjectType object_type,
                             StringPool::StringId object_name,
                             GranteeType grantee_type,
                             StringPool::StringId grantee_name,  // 0 for PUBLIC
                             bool with_grant_option,
                             std::vector<StringPool::StringId> column_names = {})  // Security Phase 3.3.3
                : Statement(ASTKind::GRANT_PRIVILEGE, span),
                  privileges_(privileges),
                  object_type_(object_type),
                  object_name_(object_name),
                  grantee_type_(grantee_type),
                  grantee_name_(grantee_name),
                  with_grant_option_(with_grant_option),
                  column_names_(std::move(column_names))  // Security Phase 3.3.3
            {
            }

            uint32_t privileges() const { return privileges_; }
            ObjectType objectType() const { return object_type_; }
            StringPool::StringId objectName() const { return object_name_; }
            GranteeType granteeType() const { return grantee_type_; }
            StringPool::StringId granteeName() const { return grantee_name_; }
            bool withGrantOption() const { return with_grant_option_; }

            // Security Phase 3.3.3: Column-level permissions
            const std::vector<StringPool::StringId>& columnNames() const { return column_names_; }
            bool hasColumnList() const { return !column_names_.empty(); }

            void accept(ASTVisitor* visitor) override;

        private:
            uint32_t privileges_;
            ObjectType object_type_;
            StringPool::StringId object_name_;
            GranteeType grantee_type_;
            StringPool::StringId grantee_name_;
            bool with_grant_option_;
            std::vector<StringPool::StringId> column_names_;  // Security Phase 3.3.3: Column-level permissions
        };

        // REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]
        class RevokePrivilegeStmt : public Statement
        {
        public:
            using PrivilegeType = GrantPrivilegeStmt::PrivilegeType;
            using ObjectType = GrantPrivilegeStmt::ObjectType;
            using GranteeType = GrantPrivilegeStmt::GranteeType;

            enum class RevokeBehavior : uint8_t
            {
                RESTRICT,
                CASCADE
            };

            RevokePrivilegeStmt(const SourceSpan& span,
                              uint32_t privileges,
                              ObjectType object_type,
                              StringPool::StringId object_name,
                              GranteeType grantee_type,
                              StringPool::StringId grantee_name,
                              RevokeBehavior behavior,
                              std::vector<StringPool::StringId> column_names = {})  // Security Phase 3.3.3
                : Statement(ASTKind::REVOKE_PRIVILEGE, span),
                  privileges_(privileges),
                  object_type_(object_type),
                  object_name_(object_name),
                  grantee_type_(grantee_type),
                  grantee_name_(grantee_name),
                  revoke_behavior_(behavior),
                  column_names_(std::move(column_names))  // Security Phase 3.3.3
            {
            }

            uint32_t privileges() const { return privileges_; }
            ObjectType objectType() const { return object_type_; }
            StringPool::StringId objectName() const { return object_name_; }
            GranteeType granteeType() const { return grantee_type_; }
            StringPool::StringId granteeName() const { return grantee_name_; }
            RevokeBehavior revokeBehavior() const { return revoke_behavior_; }

            // Security Phase 3.3.3: Column-level permissions
            const std::vector<StringPool::StringId>& columnNames() const { return column_names_; }
            bool hasColumnList() const { return !column_names_.empty(); }

            void accept(ASTVisitor* visitor) override;

        private:
            uint32_t privileges_;
            ObjectType object_type_;
            StringPool::StringId object_name_;
            GranteeType grantee_type_;
            StringPool::StringId grantee_name_;
            RevokeBehavior revoke_behavior_;
            std::vector<StringPool::StringId> column_names_;  // Security Phase 3.3.3: Column-level permissions
        };

        // GRANT role TO user/role
        class GrantRoleStmt : public Statement
        {
        public:
            enum class GranteeType : uint8_t
            {
                USER,
                ROLE
            };

            GrantRoleStmt(const SourceSpan& span,
                         StringPool::StringId rolename,
                         GranteeType grantee_type,
                         StringPool::StringId grantee_name)
                : Statement(ASTKind::GRANT_ROLE, span),
                  rolename_(rolename),
                  grantee_type_(grantee_type),
                  grantee_name_(grantee_name)
            {
            }

            StringPool::StringId rolename() const { return rolename_; }
            GranteeType granteeType() const { return grantee_type_; }
            StringPool::StringId granteeName() const { return grantee_name_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId rolename_;
            GranteeType grantee_type_;
            StringPool::StringId grantee_name_;
        };

        // REVOKE role FROM user/role [CASCADE | RESTRICT]
        class RevokeRoleStmt : public Statement
        {
        public:
            using GranteeType = GrantRoleStmt::GranteeType;

            enum class RevokeBehavior : uint8_t
            {
                RESTRICT,
                CASCADE
            };

            RevokeRoleStmt(const SourceSpan& span,
                          StringPool::StringId rolename,
                          GranteeType grantee_type,
                          StringPool::StringId grantee_name,
                          RevokeBehavior behavior)
                : Statement(ASTKind::REVOKE_ROLE, span),
                  rolename_(rolename),
                  grantee_type_(grantee_type),
                  grantee_name_(grantee_name),
                  revoke_behavior_(behavior)
            {
            }

            StringPool::StringId rolename() const { return rolename_; }
            GranteeType granteeType() const { return grantee_type_; }
            StringPool::StringId granteeName() const { return grantee_name_; }
            RevokeBehavior revokeBehavior() const { return revoke_behavior_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId rolename_;
            GranteeType grantee_type_;
            StringPool::StringId grantee_name_;
            RevokeBehavior revoke_behavior_;
        };

        // SET ROLE rolename / RESET ROLE
        class SetRoleStmt : public Statement
        {
        public:
            SetRoleStmt(const SourceSpan& span,
                       StringPool::StringId rolename,  // 0 for RESET
                       bool is_reset)
                : Statement(ASTKind::SET_ROLE, span),
                  rolename_(rolename),
                  is_reset_(is_reset)
            {
            }

            StringPool::StringId rolename() const { return rolename_; }
            bool isReset() const { return is_reset_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId rolename_;
            bool is_reset_;
        };

        // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION
        class SetSessionAuthStmt : public Statement
        {
        public:
            SetSessionAuthStmt(const SourceSpan& span,
                              StringPool::StringId username,  // 0 for RESET
                              bool is_reset)
                : Statement(ASTKind::SET_SESSION_AUTH, span),
                  username_(username),
                  is_reset_(is_reset)
            {
            }

            StringPool::StringId username() const { return username_; }
            bool isReset() const { return is_reset_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId username_;
            bool is_reset_;
        };

        // P2-7: SET CONSTRAINTS statement
        // SET CONSTRAINTS { ALL | constraint_name [, ...] } { DEFERRED | IMMEDIATE }
        class SetConstraintsStmt : public Statement
        {
        public:
            SetConstraintsStmt(const SourceSpan& span,
                              bool all_constraints,
                              const std::vector<StringPool::StringId>& constraint_names,
                              bool deferred)
                : Statement(ASTKind::SET_CONSTRAINTS, span),
                  all_constraints_(all_constraints),
                  constraint_names_(constraint_names),
                  deferred_(deferred)
            {
            }

            bool allConstraints() const { return all_constraints_; }
            const std::vector<StringPool::StringId>& constraintNames() const { return constraint_names_; }
            bool isDeferred() const { return deferred_; }

            void accept(ASTVisitor* visitor) override;

        private:
            bool all_constraints_;  // true if ALL was specified
            std::vector<StringPool::StringId> constraint_names_;  // empty if all_constraints_ is true
            bool deferred_;  // true = DEFERRED, false = IMMEDIATE
        };

        // Security Phase 3.4: CREATE POLICY
        // CREATE POLICY policy_name ON table_name
        //   [FOR { ALL | SELECT | INSERT | UPDATE | DELETE }]
        //   [TO { role_name [, ...] | PUBLIC }]
        //   [USING ( expression )]
        //   [WITH CHECK ( expression )]
        class CreatePolicyStmt : public Statement
        {
        public:
            enum class PolicyCommand : uint8_t
            {
                ALL = 0,
                SELECT = 1,
                INSERT = 2,
                UPDATE = 3,
                DELETE_CMD = 4  // DELETE is a keyword
            };

            CreatePolicyStmt(const SourceSpan& span,
                           StringPool::StringId policy_name,
                           StringPool::StringId table_name,
                           PolicyCommand command,
                           std::vector<StringPool::StringId> roles,  // Empty = applies to all
                           Expression* using_expr,      // Can be nullptr
                           Expression* with_check_expr) // Can be nullptr
                : Statement(ASTKind::CREATE_POLICY, span),
                  policy_name_(policy_name),
                  table_name_(table_name),
                  command_(command),
                  roles_(std::move(roles)),
                  using_expr_(using_expr),
                  with_check_expr_(with_check_expr)
            {
            }

            StringPool::StringId policyName() const { return policy_name_; }
            StringPool::StringId tableName() const { return table_name_; }
            PolicyCommand command() const { return command_; }
            const std::vector<StringPool::StringId>& roles() const { return roles_; }
            Expression* usingExpr() const { return using_expr_; }
            Expression* withCheckExpr() const { return with_check_expr_; }

            bool appliesToAllRoles() const { return roles_.empty(); }
            bool hasUsingExpr() const { return using_expr_ != nullptr; }
            bool hasWithCheckExpr() const { return with_check_expr_ != nullptr; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId policy_name_;
            StringPool::StringId table_name_;
            PolicyCommand command_;
            std::vector<StringPool::StringId> roles_;
            Expression* using_expr_;
            Expression* with_check_expr_;
        };

        // Security Phase 3.4: DROP POLICY
        // DROP POLICY [IF EXISTS] policy_name ON table_name [CASCADE | RESTRICT]
        class DropPolicyStmt : public Statement
        {
        public:
            enum class DropBehavior : uint8_t
            {
                RESTRICT,
                CASCADE
            };

            DropPolicyStmt(const SourceSpan& span,
                          StringPool::StringId policy_name,
                          StringPool::StringId table_name,
                          bool if_exists,
                          DropBehavior drop_behavior)
                : Statement(ASTKind::DROP_POLICY, span),
                  policy_name_(policy_name),
                  table_name_(table_name),
                  if_exists_(if_exists),
                  drop_behavior_(drop_behavior)
            {
            }

            StringPool::StringId policyName() const { return policy_name_; }
            StringPool::StringId tableName() const { return table_name_; }
            bool ifExists() const { return if_exists_; }
            DropBehavior dropBehavior() const { return drop_behavior_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId policy_name_;
            StringPool::StringId table_name_;
            bool if_exists_;
            DropBehavior drop_behavior_;
        };

        // Security Phase 3.4: ALTER TABLE ... ROW LEVEL SECURITY
        // ALTER TABLE table_name ENABLE ROW LEVEL SECURITY
        // ALTER TABLE table_name DISABLE ROW LEVEL SECURITY
        // ALTER TABLE table_name FORCE ROW LEVEL SECURITY
        // ALTER TABLE table_name NO FORCE ROW LEVEL SECURITY
        class AlterTableRLSStmt : public Statement
        {
        public:
            enum class RLSAction : uint8_t
            {
                ENABLE,      // ENABLE ROW LEVEL SECURITY
                DISABLE,     // DISABLE ROW LEVEL SECURITY
                FORCE,       // FORCE ROW LEVEL SECURITY
                NO_FORCE     // NO FORCE ROW LEVEL SECURITY
            };

            AlterTableRLSStmt(const SourceSpan& span,
                             StringPool::StringId table_name,
                             RLSAction action)
                : Statement(ASTKind::ALTER_TABLE_RLS, span),
                  table_name_(table_name),
                  action_(action)
            {
            }

            StringPool::StringId tableName() const { return table_name_; }
            RLSAction action() const { return action_; }

            void accept(ASTVisitor* visitor) override;

        private:
            StringPool::StringId table_name_;
            RLSAction action_;
        };

        // ===== Visitor Pattern =====

        class ASTVisitor
        {
        public:
            virtual ~ASTVisitor() = default;

            // Statements
            virtual void visit(CreateTableStmt *node) = 0;
            virtual void visit(CreateIndexStmt *node) = 0;             // Phase 2 Task 2.3
            virtual void visit(DropTableStmt *node) = 0;               // ALPHA Phase 1 - DDL Modifications
            virtual void visit(DropIndexStmt *node) = 0;               // ALPHA Phase 1 - DDL Modifications
            virtual void visit(TruncateTableStmt *node) = 0;           // ALPHA Phase 1 - DDL Modifications (TRUNCATE TABLE ASYNC)
            virtual void visit(AlterTableStmt *node) = 0;              // ALPHA Phase 1 - DDL Modifications
            virtual void visit(CreateSequenceStmt *node) = 0;          // ALPHA Phase 1 - Sequences
            virtual void visit(AlterSequenceStmt *node) = 0;           // ALPHA Phase 1 - Sequences
            virtual void visit(DropSequenceStmt *node) = 0;            // ALPHA Phase 1 - Sequences
            virtual void visit(CreateViewStmt *node) = 0;              // ALPHA Phase 1 - Views
            virtual void visit(DropViewStmt *node) = 0;                // ALPHA Phase 1 - Views
            virtual void visit(RefreshMaterializedViewStmt *node) = 0; // ALPHA Phase 1 - Materialized Views
            virtual void visit(CreateTablespaceStmt *node) = 0;        // Phase 2 Task 2.1
            virtual void visit(AlterTablespaceStmt *node) = 0;         // Phase 2 Task 2.2
            virtual void visit(AlterTableSetTablespaceStmt *node) = 0; // Phase 4 Task 4.1.1
            virtual void visit(DropTablespaceStmt *node) = 0;          // Phase 2 Task 2.1
            virtual void visit(AttachTablespaceStmt *node) = 0;        // Phase 6 Task 6.1
            virtual void visit(DetachTablespaceStmt *node) = 0;        // Phase 6 Task 6.2
            virtual void visit(InsertStmt *node) = 0;
            virtual void visit(SelectStmt *node) = 0;
            virtual void visit(SetOperationStmt *node) = 0;     // UNION/INTERSECT/EXCEPT
            virtual void visit(UpdateStmt *node) = 0;           // Phase 1 Task 2.1
            virtual void visit(DeleteStmt *node) = 0;           // Phase 1 Task 2.2
            virtual void visit(MergeStmt *node) = 0;            // Alpha 1 - Advanced SQL
            virtual void visit(AnalyzeStmt *node) = 0;          // Phase 1 Task 1.1.2
            virtual void visit(ExplainStmt *node) = 0;          // Phase 1 Task 1.5
            virtual void visit(StartTransactionStmt *node) = 0; // Phase 2 Task 2.6
            virtual void visit(SetTransactionStmt *node) = 0;   // Phase 3 Task 3.6
            virtual void visit(CommitStmt *node) = 0;           // Phase 2 Task 2.6
            virtual void visit(RollbackStmt *node) = 0;         // Phase 2 Task 2.6
            virtual void visit(SweepStmt *node) = 0;            // Phase 3 Task 3.3
            virtual void visit(ShowStmt *node) = 0;             // ALPHA Phase 1 - Developer Experience
            virtual void visit(DescribeStmt *node) = 0;         // ALPHA Phase 1 - Developer Experience
            virtual void visit(CreateTriggerStmt *node) = 0;    // Phase 2 Wave 2 Agent C
            virtual void visit(DropTriggerStmt *node) = 0;      // Phase 2 Wave 2 Agent C

            // PSQL - Stored Procedures and Functions (Phase 2 Task 10.2)
            virtual void visit(CreateFunctionStmt *node) = 0;
            virtual void visit(CreateProcedureStmt *node) = 0;
            virtual void visit(BlockStmt *node) = 0;
            virtual void visit(VarDeclarationStmt *node) = 0;
            virtual void visit(AssignmentStmt *node) = 0;
            virtual void visit(IfStmt *node) = 0;
            virtual void visit(LoopStmt *node) = 0;
            virtual void visit(WhileStmt *node) = 0;
            virtual void visit(ExitStmt *node) = 0;
            virtual void visit(ReturnStmt *node) = 0;
            virtual void visit(RaiseStmt *node) = 0;

            // Security statements (ALPHA Phase 1 - Security System Phase 2)
            virtual void visit(CreateUserStmt *node) = 0;
            virtual void visit(AlterUserStmt *node) = 0;
            virtual void visit(DropUserStmt *node) = 0;
            virtual void visit(CreateRoleStmt *node) = 0;
            virtual void visit(DropRoleStmt *node) = 0;
            virtual void visit(CreateGroupStmt *node) = 0;
            virtual void visit(DropGroupStmt *node) = 0;
            virtual void visit(GrantPrivilegeStmt *node) = 0;
            virtual void visit(RevokePrivilegeStmt *node) = 0;
            virtual void visit(GrantRoleStmt *node) = 0;
            virtual void visit(RevokeRoleStmt *node) = 0;
            virtual void visit(SetRoleStmt *node) = 0;
            virtual void visit(SetSessionAuthStmt *node) = 0;
            virtual void visit(SetConstraintsStmt *node) = 0;   // P2-7: SET CONSTRAINTS
            virtual void visit(CreatePolicyStmt *node) = 0;     // Security Phase 3.4
            virtual void visit(DropPolicyStmt *node) = 0;       // Security Phase 3.4
            virtual void visit(AlterTableRLSStmt *node) = 0;    // Security Phase 3.4

            // Expressions
            virtual void visit(LiteralExpr *node) = 0;
            virtual void visit(IdentifierExpr *node) = 0;
            virtual void visit(BinaryOpExpr *node) = 0;
            virtual void visit(CastExpr *node) = 0;
            virtual void visit(FunctionCallExpr *node) = 0;
            virtual void visit(SequenceFunctionExpr *node) = 0;  // ALPHA Phase 1 - Sequences
            virtual void visit(ExtractExpr *node) = 0;           // EXTRACT(field FROM value)
            virtual void visit(AggregateExpr *node) = 0;  // Phase 1 Task 4.1
            virtual void visit(WindowFuncExpr *node) = 0; // Phase 1 Task 6
            virtual void visit(WindowSpec *node) = 0;     // Phase 1 Task 6
            virtual void visit(JSONFuncExpr *node) = 0;   // Phase 1 Task 7
            virtual void visit(CoalesceExpr *node) = 0;   // Phase 1 Task 8
            virtual void visit(NullIfExpr *node) = 0;     // Phase 1 Task 8
            virtual void visit(CaseExpr *node) = 0;       // Phase 1 Task 8
            virtual void visit(GroupingExpr *node) = 0;   // Phase 3: Advanced Grouping
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
            void visit(MergeStmt *node) override;                    // ALPHA Phase 1 - Advanced SQL
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
            void visit(GroupingExpr *node) override;   // Phase 3: Advanced Grouping
            void visit(ArrayLiteral *node) override;   // Phase 2 Task 12
            void visit(SubqueryExpr *node) override;   // Phase 2 Wave 2 - Agent B
            void visit(ColumnDef *node) override;

            // Procedural language statements (stub implementations)
            void visit(CreateTriggerStmt *node) override;
            void visit(DropTriggerStmt *node) override;
            void visit(CreateFunctionStmt *node) override;
            void visit(CreateProcedureStmt *node) override;
            void visit(BlockStmt *node) override;
            void visit(VarDeclarationStmt *node) override;
            void visit(AssignmentStmt *node) override;
            void visit(IfStmt *node) override;
            void visit(LoopStmt *node) override;
            void visit(WhileStmt *node) override;
            void visit(ExitStmt *node) override;
            void visit(ReturnStmt *node) override;
            void visit(RaiseStmt *node) override;

            // Security statements (ALPHA Phase 1 - Security System Phase 2)
            void visit(CreateUserStmt *node) override;
            void visit(AlterUserStmt *node) override;
            void visit(DropUserStmt *node) override;
            void visit(CreateRoleStmt *node) override;
            void visit(DropRoleStmt *node) override;
            void visit(CreateGroupStmt *node) override;
            void visit(DropGroupStmt *node) override;
            void visit(GrantPrivilegeStmt *node) override;
            void visit(RevokePrivilegeStmt *node) override;
            void visit(GrantRoleStmt *node) override;
            void visit(RevokeRoleStmt *node) override;
            void visit(SetRoleStmt *node) override;
            void visit(SetSessionAuthStmt *node) override;
            void visit(SetConstraintsStmt *node) override;

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