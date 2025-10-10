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
            INSERT,
            SELECT,
            START_TRANSACTION,  // Phase 2 Task 2.6
            COMMIT,             // Phase 2 Task 2.6
            ROLLBACK,           // Phase 2 Task 2.6
            SWEEP,              // Phase 3 Task 3.3

            // Expressions
            LITERAL,
            IDENTIFIER,
            BINARY_OP,
            CAST,
            FUNCTION_CALL,

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
            uint32_t precision; // For VARCHAR(n), CHAR(n)
            uint32_t scale;     // For DECIMAL(p,s)
            bool with_timezone; // For TIMESTAMP WITH TIME ZONE
            uint16_t timezone_hint; // Timezone ID for display

            TypeName(DataType t, uint32_t p = 0, uint32_t s = 0, bool tz = false, uint16_t tz_hint = 0)
                : type(t), precision(p), scale(s), with_timezone(tz), timezone_hint(tz_hint) {}

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
            IdentifierExpr(const SourceSpan &span, StringPool::StringId name)
                : Expression(ASTKind::IDENTIFIER, span), name_(name)
            {
            }

            StringPool::StringId name() const
            {
                return name_;
            }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;
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
            ILIKE
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
            CastExpr(const SourceSpan &span, Expression *expr, const TypeName &target_type, bool is_try_cast = false)
                : Expression(ASTKind::CAST, span), expr_(expr), target_type_(target_type), is_try_cast_(is_try_cast)
            {
            }

            Expression *expr() const { return expr_; }
            const TypeName &targetType() const { return target_type_; }
            bool isTryCast() const { return is_try_cast_; }

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
            FunctionCallExpr(const SourceSpan &span, StringPool::StringId name, std::vector<Expression *> args)
                : Expression(ASTKind::FUNCTION_CALL, span), name_(name), args_(std::move(args))
            {
            }

            StringPool::StringId name() const { return name_; }
            const std::vector<Expression *> &args() const { return args_; }

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;
            std::vector<Expression *> args_;
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
            StringPool::StringId charset_;    // CHARACTER SET clause
            StringPool::StringId collation_;  // COLLATE clause
        };

        // CREATE TABLE statement
        class CreateTableStmt : public Statement
        {
        public:
            CreateTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                            std::vector<ColumnDef *> columns,
                            StringPool::StringId charset = 0,
                            StringPool::StringId collation = 0)
                : Statement(ASTKind::CREATE_TABLE, span), table_name_(table_name),
                  columns_(std::move(columns)), charset_(charset), collation_(collation)
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

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<ColumnDef *> columns_;
            StringPool::StringId charset_;    // DEFAULT CHARACTER SET clause
            StringPool::StringId collation_;  // DEFAULT COLLATE clause
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

        // SELECT statement
        class SelectStmt : public Statement
        {
        public:
            SelectStmt(const SourceSpan &span, std::vector<SelectItem> select_list,
                       StringPool::StringId table_name, Expression *where_clause = nullptr)
                : Statement(ASTKind::SELECT, span), select_list_(std::move(select_list)),
                  table_name_(table_name), where_clause_(where_clause)
            {
            }

            const std::vector<SelectItem> &selectList() const
            {
                return select_list_;
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
            std::vector<SelectItem> select_list_;
            StringPool::StringId table_name_;
            Expression *where_clause_;
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

        // START TRANSACTION statement
        class StartTransactionStmt : public Statement
        {
        public:
            StartTransactionStmt(const SourceSpan &span,
                                 TransactionMode mode = TransactionMode::READ_WRITE,
                                 IsolationLevel isolation = IsolationLevel::READ_COMMITTED,
                                 bool wait = true,
                                 bool commit_outstanding = false)
                : Statement(ASTKind::START_TRANSACTION, span),
                  mode_(mode), isolation_(isolation), wait_(wait),
                  commit_outstanding_(commit_outstanding)
            {
            }

            TransactionMode mode() const { return mode_; }
            IsolationLevel isolation() const { return isolation_; }
            bool wait() const { return wait_; }
            bool commitOutstanding() const { return commit_outstanding_; }

            void accept(ASTVisitor *visitor) override;

        private:
            TransactionMode mode_;
            IsolationLevel isolation_;
            bool wait_;
            bool commit_outstanding_;
        };

        // COMMIT statement
        class CommitStmt : public Statement
        {
        public:
            explicit CommitStmt(const SourceSpan &span)
                : Statement(ASTKind::COMMIT, span)
            {
            }

            void accept(ASTVisitor *visitor) override;
        };

        // ROLLBACK statement
        class RollbackStmt : public Statement
        {
        public:
            explicit RollbackStmt(const SourceSpan &span)
                : Statement(ASTKind::ROLLBACK, span)
            {
            }

            void accept(ASTVisitor *visitor) override;
        };

        // SWEEP DATABASE statement (Phase 3 Task 3.3)
        class SweepStmt : public Statement
        {
        public:
            explicit SweepStmt(const SourceSpan &span)
                : Statement(ASTKind::SWEEP, span)
            {
            }

            void accept(ASTVisitor *visitor) override;
        };

        // ===== Visitor Pattern =====

        class ASTVisitor
        {
        public:
            virtual ~ASTVisitor() = default;

            // Statements
            virtual void visit(CreateTableStmt *node) = 0;
            virtual void visit(InsertStmt *node) = 0;
            virtual void visit(SelectStmt *node) = 0;
            virtual void visit(StartTransactionStmt *node) = 0;  // Phase 2 Task 2.6
            virtual void visit(CommitStmt *node) = 0;            // Phase 2 Task 2.6
            virtual void visit(RollbackStmt *node) = 0;          // Phase 2 Task 2.6
            virtual void visit(SweepStmt *node) = 0;             // Phase 3 Task 3.3

            // Expressions
            virtual void visit(LiteralExpr *node) = 0;
            virtual void visit(IdentifierExpr *node) = 0;
            virtual void visit(BinaryOpExpr *node) = 0;
            virtual void visit(CastExpr *node) = 0;
            virtual void visit(FunctionCallExpr *node) = 0;

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
            void visit(InsertStmt *node) override;
            void visit(SelectStmt *node) override;
            void visit(StartTransactionStmt *node) override;  // Phase 2 Task 2.6
            void visit(CommitStmt *node) override;            // Phase 2 Task 2.6
            void visit(RollbackStmt *node) override;          // Phase 2 Task 2.6
            void visit(SweepStmt *node) override;             // Phase 3 Task 3.3
            void visit(LiteralExpr *node) override;
            void visit(IdentifierExpr *node) override;
            void visit(BinaryOpExpr *node) override;
            void visit(CastExpr *node) override;
            void visit(FunctionCallExpr *node) override;
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