#pragma once

#include "scratchbird/parser/ast.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace scratchbird
{
    namespace parser
    {

        // Symbol kinds
        enum class SymbolKind : uint8_t
        {
            TABLE,
            COLUMN,
            // Future: FUNCTION, VIEW, etc.
        };

        // Information about a column
        struct ColumnSymbol
        {
            StringPool::StringId name;
            TypeName type;
            bool nullable;
            uint32_t column_index; // Position in table

            ColumnSymbol() : name(0), type(DataType::INTEGER), nullable(true), column_index(0) {}
            ColumnSymbol(StringPool::StringId n, const TypeName &t, bool null, uint32_t idx)
                : name(n), type(t), nullable(null), column_index(idx)
            {
            }
        };

        // Information about a table
        struct TableSymbol
        {
            StringPool::StringId name;
            std::vector<ColumnSymbol> columns;
            std::unordered_map<StringPool::StringId, size_t> column_map; // name -> index

            explicit TableSymbol(StringPool::StringId n) : name(n) {}

            void addColumn(const ColumnSymbol &col)
            {
                column_map[col.name] = columns.size();
                columns.push_back(col);
            }

            const ColumnSymbol *findColumn(StringPool::StringId name) const
            {
                auto it = column_map.find(name);
                if (it != column_map.end())
                {
                    return &columns[it->second];
                }
                return nullptr;
            }
        };

        // Symbol table scope
        class Scope
        {
        public:
            Scope(Scope *parent = nullptr) : parent_(parent) {}

            // Add symbols
            void addTable(StringPool::StringId name, std::unique_ptr<TableSymbol> table);
            void addColumn(StringPool::StringId name, const ColumnSymbol &column);

            // Lookup symbols
            TableSymbol *findTable(StringPool::StringId name) const;
            const ColumnSymbol *findColumn(StringPool::StringId name) const;

            // Parent scope
            Scope *parent() const
            {
                return parent_;
            }

        private:
            Scope *parent_;
            std::unordered_map<StringPool::StringId, std::unique_ptr<TableSymbol>> tables_;
            std::unordered_map<StringPool::StringId, ColumnSymbol> columns_;
        };

        // Symbol table manager
        class SymbolTable
        {
        public:
            SymbolTable();
            ~SymbolTable();

            // Scope management
            void pushScope();
            void popScope();
            Scope *currentScope() const
            {
                return current_scope_;
            }

            // Symbol operations
            void addTable(StringPool::StringId name, std::unique_ptr<TableSymbol> table)
            {
                current_scope_->addTable(name, std::move(table));
            }

            TableSymbol *findTable(StringPool::StringId name) const
            {
                return current_scope_->findTable(name);
            }

            const ColumnSymbol *findColumn(StringPool::StringId name) const
            {
                return current_scope_->findColumn(name);
            }

            // Add column to current scope (for SELECT list resolution)
            void addColumn(StringPool::StringId name, const ColumnSymbol &column)
            {
                current_scope_->addColumn(name, column);
            }

        private:
            std::vector<std::unique_ptr<Scope>> scopes_;
            Scope *current_scope_;
        };

        // Type compatibility checking
        class TypeChecker
        {
        public:
            // Check if two types are compatible
            static bool areCompatible(const TypeName &t1, const TypeName &t2);

            // Check if a value can be assigned to a type
            static bool canAssign(const TypeName &target, const TypeName &source);

            // Get result type of binary operation
            static TypeName getBinaryOpResultType(BinaryOp op, const TypeName &left,
                                                  const TypeName &right);

            // Check if type supports comparison
            static bool supportsComparison(const TypeName &type);

            // Check if type supports arithmetic
            static bool supportsArithmetic(const TypeName &type);
        };

    } // namespace parser
} // namespace scratchbird