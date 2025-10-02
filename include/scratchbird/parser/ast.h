#pragma once

#include "scratchbird/parser/token.h"
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

            // Expressions
            LITERAL,
            IDENTIFIER,
            BINARY_OP,

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

        // Data types
        enum class DataType : uint8_t
        {
            INTEGER,
            BIGINT,
            DOUBLE,
            VARCHAR,
        };

        struct TypeName
        {
            DataType type;
            uint32_t precision; // For VARCHAR(n)

            TypeName(DataType t, uint32_t p = 0) : type(t), precision(p) {}
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
            OR
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
                      bool nullable)
                : ASTNode(ASTKind::COLUMN_DEF, span), name_(name), type_(type), nullable_(nullable)
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

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId name_;
            TypeName type_;
            bool nullable_;
        };

        // CREATE TABLE statement
        class CreateTableStmt : public Statement
        {
        public:
            CreateTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                            std::vector<ColumnDef *> columns)
                : Statement(ASTKind::CREATE_TABLE, span), table_name_(table_name),
                  columns_(std::move(columns))
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

            void accept(ASTVisitor *visitor) override;

        private:
            StringPool::StringId table_name_;
            std::vector<ColumnDef *> columns_;
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

        // ===== Visitor Pattern =====

        class ASTVisitor
        {
        public:
            virtual ~ASTVisitor() = default;

            // Statements
            virtual void visit(CreateTableStmt *node) = 0;
            virtual void visit(InsertStmt *node) = 0;
            virtual void visit(SelectStmt *node) = 0;

            // Expressions
            virtual void visit(LiteralExpr *node) = 0;
            virtual void visit(IdentifierExpr *node) = 0;
            virtual void visit(BinaryOpExpr *node) = 0;

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
            void visit(LiteralExpr *node) override;
            void visit(IdentifierExpr *node) override;
            void visit(BinaryOpExpr *node) override;
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