#pragma once

#include "scratchbird/parser/ast.h"
#include "scratchbird/sblr/opcodes.h"
#include <vector>
#include <memory>
#include <cstring>

namespace scratchbird
{
    namespace sblr
    {

        // Bytecode generation result
        class BytecodeResult
        {
        public:
            bool success() const
            {
                // Success requires no errors AND bytecode was generated
                return errors_.empty() && !bytecode_.empty();
            }
            const std::vector<uint8_t> &bytecode() const
            {
                return bytecode_;
            }
            const std::vector<std::string> &errors() const
            {
                return errors_;
            }

            void addError(const std::string &error)
            {
                errors_.push_back(error);
            }

            // Bytecode writing helpers
            void writeByte(uint8_t byte)
            {
                bytecode_.push_back(byte);
            }
            void writeOpcode(Opcode op)
            {
                writeByte(static_cast<uint8_t>(op));
            }

            void writeInt32(uint32_t value)
            {
                bytecode_.resize(bytecode_.size() + 4);
                sblr::writeInt32(&bytecode_[bytecode_.size() - 4], value);
            }

            void writeInt64(uint64_t value)
            {
                bytecode_.resize(bytecode_.size() + 8);
                sblr::writeInt64(&bytecode_[bytecode_.size() - 8], value);
            }

            void writeDouble(double value)
            {
                // Write as 64-bit little-endian (IEEE 754 double)
                // Use memcpy to avoid aliasing issues, then serialize bytes
                uint64_t bits;
                std::memcpy(&bits, &value, sizeof(double));
                bytecode_.resize(bytecode_.size() + 8);
                sblr::writeInt64(&bytecode_[bytecode_.size() - 8], bits);
            }

            void writeString(const std::string &str)
            {
                // Check for overflow when converting size_t to uint32_t
                if (str.size() > UINT32_MAX)
                {
                    addError("String length exceeds maximum allowed size (4GB)");
                    writeInt32(0);
                    return;
                }
                // Write length as 32-bit value
                writeInt32(static_cast<uint32_t>(str.size()));
                // Write string data
                bytecode_.insert(bytecode_.end(), str.begin(), str.end());
            }

        private:
            std::vector<uint8_t> bytecode_;
            std::vector<std::string> errors_;
        };

        // Bytecode generator - converts AST to SBLR bytecode
        class BytecodeGenerator : public parser::ASTVisitor
        {
        public:
            BytecodeGenerator(const parser::StringPool &string_pool);
            ~BytecodeGenerator();

            // Generate bytecode for a statement
            BytecodeResult generate(parser::Statement *stmt);

            // ASTVisitor interface
            void visit(parser::CreateTableStmt *node) override;
            void visit(parser::InsertStmt *node) override;
            void visit(parser::SelectStmt *node) override;
            void visit(parser::LiteralExpr *node) override;
            void visit(parser::IdentifierExpr *node) override;
            void visit(parser::BinaryOpExpr *node) override;
            void visit(parser::ColumnDef *node) override;

        private:
            const parser::StringPool &string_pool_;
            BytecodeResult *current_result_;

            // Helper to write string from string pool
            void writeStringId(parser::StringPool::StringId id);

            // Helper to write data type
            void writeDataType(const parser::TypeName &type);

            // Helper to generate expression bytecode
            void generateExpression(parser::Expression *expr);
        };

        // Debug helper - disassemble bytecode to readable format
        class BytecodeDisassembler
        {
        public:
            static std::string disassemble(const std::vector<uint8_t> &bytecode);

        private:
            static std::string opcodeToString(Opcode op);
        };

    } // namespace sblr
} // namespace scratchbird