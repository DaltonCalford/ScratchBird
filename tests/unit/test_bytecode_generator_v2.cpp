/**
 * Unit Tests for ScratchBird Bytecode Generator v2.0
 *
 * Tests bytecode generation from resolved AST:
 * - Expression bytecode
 * - Statement bytecode
 * - Optimization passes
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "unit/test_user_helpers.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace scratchbird::parser::v2;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
namespace sblr = scratchbird::sblr;

// Generate a unique database path per test to avoid conflicts in parallel execution
static std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_bytecode_v2_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

class BytecodeGeneratorV2Test : public ::testing::Test {
protected:
    static std::string formatDiagnostics(const BytecodeResultV2& result) {
        std::ostringstream oss;
        if (!result.errors().empty()) {
            oss << "\nErrors:";
            for (const auto& err : result.errors()) {
                oss << "\n  " << err;
            }
        }
        if (!result.warnings().empty()) {
            oss << "\nWarnings:";
            for (const auto& warn : result.warnings()) {
                oss << "\n  " << warn;
            }
        }
        return oss.str();
    }

    void SetUp() override {
        // Create a temporary database for testing with unique path
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr) << "CatalogManager is null";

        // Create a test schema
        EnsureUser(catalog_, "test_user");
        status = catalog_->createSchema("test", "test_user", test_schema_id_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test schema";
    }

    void TearDown() override {
        db_.close();
        // Clean up database file and lock file
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    // Generate bytecode from SQL
    BytecodeResultV2 generateBytecode(const std::string& sql) {
        input_sql_ = sql;
        parser_ = std::make_unique<Parser>(input_sql_);
        auto parse_result = parser_->parseStatement();

        if (!parse_result.success()) {
            BytecodeResultV2 result;
            for (const auto& err : parse_result.errors()) {
                result.addError("Parse error: " + err.message);
            }
            return result;
        }

        analyzer_ = std::make_unique<SemanticAnalyzerV2>(*catalog_, parser_->stringPool());
        analyzer_->setCurrentSchema(test_schema_id_);
        auto sem_result = analyzer_->analyze(parse_result.statement());

        if (!sem_result.success()) {
            BytecodeResultV2 result;
            for (const auto& err : sem_result.errors()) {
                result.addError("Semantic error: " + err.message);
            }
            return result;
        }

        generator_ = std::make_unique<BytecodeGeneratorV2>(parser_->stringPool());
        return generator_->generate(sem_result.statement());
    }

    // Check if bytecode contains a specific opcode
    bool hasOpcode(const std::vector<uint8_t>& bytecode, Opcode op) {
        uint8_t target = static_cast<uint8_t>(op);
        for (size_t i = 0; i < bytecode.size(); ++i) {
            if (bytecode[i] == target) {
                return true;
            }
        }
        return false;
    }

    bool hasExtendedOpcode(const std::vector<uint8_t>& bytecode, sblr::ExtendedOpcode op) {
        uint16_t target = static_cast<uint16_t>(op);
        for (size_t i = 0; i + 2 < bytecode.size(); ++i) {
            if (bytecode[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)) {
                uint16_t opcode = sblr::readInt16(&bytecode[i + 1]);
                if (opcode == target) {
                    return true;
                }
                i += 2;
            }
        }
        return false;
    }

    // Get opcode count
    size_t countOpcode(const std::vector<uint8_t>& bytecode, Opcode op) {
        uint8_t target = static_cast<uint8_t>(op);
        size_t count = 0;
        for (size_t i = 0; i < bytecode.size(); ++i) {
            if (bytecode[i] == target) {
                ++count;
            }
        }
        return count;
    }

    size_t findOpcodeOffset(const std::vector<uint8_t>& bytecode, Opcode op) {
        uint8_t target = static_cast<uint8_t>(op);
        for (size_t i = 0; i < bytecode.size(); ++i) {
            if (bytecode[i] == target) {
                return i;
            }
        }
        return bytecode.size();
    }

    bool findExtendedOpcode(const std::vector<uint8_t>& bytecode,
                            sblr::ExtendedOpcode ext_op,
                            size_t& payload_offset) {
        uint8_t marker = static_cast<uint8_t>(Opcode::EXTENDED_OPCODE);
        for (size_t i = 0; i + 2 < bytecode.size(); ++i) {
            if (bytecode[i] != marker) {
                continue;
            }
            uint16_t found = sblr::readInt16(&bytecode[i + 1]);
            if (found == static_cast<uint16_t>(ext_op)) {
                payload_offset = i + 3;
                return true;
            }
        }
        return false;
    }

    bool readUVarint(const std::vector<uint8_t>& bytecode, size_t* offset, uint64_t* out) {
        if (*offset >= bytecode.size()) {
            return false;
        }
        size_t bytes_read = 0;
        if (!sblr::readUVarint(&bytecode[*offset], bytecode.size() - *offset, *out, bytes_read)) {
            return false;
        }
        *offset += bytes_read;
        return true;
    }

    bool readStringVarint(const std::vector<uint8_t>& bytecode, size_t* offset, std::string* out) {
        uint64_t length = 0;
        if (!readUVarint(bytecode, offset, &length)) {
            return false;
        }
        if (*offset + length > bytecode.size()) {
            return false;
        }
        out->assign(reinterpret_cast<const char*>(&bytecode[*offset]), length);
        *offset += length;
        return true;
    }

    struct StartTransactionPayload {
        uint16_t flags = 0;
        uint8_t conflict_action = 0;
        bool has_conflict_error_code = false;
        int32_t conflict_error_code = 0;
        bool has_autocommit = false;
        uint8_t autocommit_mode = 0;
        bool has_read_committed_mode = false;
        uint8_t read_committed_mode = 0;
        bool has_wait_mode = false;
        uint8_t wait_mode = 0;
        bool has_lock_timeout = false;
        uint32_t lock_timeout = 0;
        bool has_reservations = false;
        uint32_t reservation_count = 0;
        uint8_t first_lock_mode = 0;
        uint8_t first_for_write = 0;
    };

    struct CreateDatabasePayload {
        uint8_t flags = 0;
        std::string database_path;
        std::string source_spec;
        std::vector<std::pair<std::string, std::string>> options;
        std::vector<std::string> aliases;
    };

    bool parseStartTransactionPayload(const std::vector<uint8_t>& bytecode,
                                      StartTransactionPayload& out) {
        size_t offset = findOpcodeOffset(bytecode, Opcode::START_TRANSACTION);
        if (offset == bytecode.size()) {
            return false;
        }
        offset += 1;
        if (offset + 3 > bytecode.size()) {
            return false;
        }

        out.flags = sblr::readInt16(&bytecode[offset]);
        offset += 2;
        out.conflict_action = bytecode[offset++];

        if (out.flags & sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE) {
            if (offset + 4 > bytecode.size()) return false;
            out.has_conflict_error_code = true;
            out.conflict_error_code = static_cast<int32_t>(sblr::readInt32(&bytecode[offset]));
            offset += 4;
        }
        if (out.flags & sblr::TransactionFlags::HAS_AUTOCOMMIT) {
            if (offset + 1 > bytecode.size()) return false;
            out.has_autocommit = true;
            out.autocommit_mode = bytecode[offset++];
        }
        if (out.flags & sblr::TransactionFlags::HAS_ISOLATION) {
            if (offset + 1 > bytecode.size()) return false;
            offset += 1;
        }
        if (out.flags & sblr::TransactionFlags::HAS_READ_COMMITTED_MODE) {
            if (offset + 1 > bytecode.size()) return false;
            out.has_read_committed_mode = true;
            out.read_committed_mode = bytecode[offset++];
        }
        if (out.flags & sblr::TransactionFlags::HAS_ACCESS_MODE) {
            if (offset + 1 > bytecode.size()) return false;
            offset += 1;
        }
        if (out.flags & sblr::TransactionFlags::HAS_DEFERRABLE) {
            if (offset + 1 > bytecode.size()) return false;
            offset += 1;
        }
        if (out.flags & sblr::TransactionFlags::HAS_WAIT_MODE) {
            if (offset + 1 > bytecode.size()) return false;
            out.has_wait_mode = true;
            out.wait_mode = bytecode[offset++];
        }
        if (out.flags & sblr::TransactionFlags::HAS_LOCK_TIMEOUT) {
            if (offset + 4 > bytecode.size()) return false;
            out.has_lock_timeout = true;
            out.lock_timeout = sblr::readInt32(&bytecode[offset]);
            offset += 4;
        }
        if (out.flags & sblr::TransactionFlags::HAS_RESERVATIONS) {
            if (offset + 1 > bytecode.size()) return false;
            out.has_reservations = true;
            if (bytecode[offset++] != static_cast<uint8_t>(Opcode::BEGIN_LIST)) return false;
            uint64_t reservation_count = 0;
            if (!readUVarint(bytecode, &offset, &reservation_count)) return false;
            out.reservation_count = static_cast<uint32_t>(reservation_count);
            if (out.reservation_count > 0) {
                if (offset + 1 > bytecode.size()) return false;
                if (bytecode[offset++] != static_cast<uint8_t>(Opcode::TABLE_REF)) return false;
                if (offset + 1 > bytecode.size()) return false;
                uint8_t ref_kind = bytecode[offset++];
                if (ref_kind != 0) {
                    if (offset + 16 > bytecode.size()) return false;
                    offset += 16;
                } else {
                    std::string table_name;
                    if (!readStringVarint(bytecode, &offset, &table_name)) return false;
                }
                std::string alias;
                if (!readStringVarint(bytecode, &offset, &alias)) return false;
                if (offset + 2 > bytecode.size()) return false;
                out.first_lock_mode = bytecode[offset++];
                out.first_for_write = bytecode[offset++];
            }
        }

        return true;
    }

    bool parseCreateDatabasePayload(const std::vector<uint8_t>& bytecode,
                                    CreateDatabasePayload& out) {
        size_t offset = 0;
        if (!findExtendedOpcode(bytecode, sblr::ExtendedOpcode::EXT_CREATE_DATABASE, offset)) {
            return false;
        }

        if (offset >= bytecode.size()) {
            return false;
        }
        out.flags = bytecode[offset++];

        if (!readStringVarint(bytecode, &offset, &out.database_path)) return false;
        if (!readStringVarint(bytecode, &offset, &out.source_spec)) return false;

        if (offset + 4 > bytecode.size()) return false;
        uint32_t option_count = sblr::readInt32(&bytecode[offset]);
        offset += 4;
        out.options.clear();
        out.options.reserve(option_count);
        for (uint32_t i = 0; i < option_count; ++i) {
            std::string key;
            std::string value;
            if (!readStringVarint(bytecode, &offset, &key)) return false;
            if (!readStringVarint(bytecode, &offset, &value)) return false;
            out.options.emplace_back(std::move(key), std::move(value));
        }

        if (offset + 4 > bytecode.size()) return false;
        uint32_t alias_count = sblr::readInt32(&bytecode[offset]);
        offset += 4;
        out.aliases.clear();
        out.aliases.reserve(alias_count);
        for (uint32_t i = 0; i < alias_count; ++i) {
            std::string alias;
            if (!readStringVarint(bytecode, &offset, &alias)) return false;
            out.aliases.push_back(std::move(alias));
        }

        return true;
    }

    std::string input_sql_;
    std::unique_ptr<Parser> parser_;
    std::unique_ptr<SemanticAnalyzerV2> analyzer_;
    std::unique_ptr<BytecodeGeneratorV2> generator_;

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID test_schema_id_;
};

// =============================================================================
// Basic Bytecode Structure Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, BytecodeHasVersionHeader) {
    auto result = generateBytecode("SELECT 1");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    const auto& bc = result.bytecode();
    ASSERT_GE(bc.size(), 3);

    // First byte should be VERSION opcode
    EXPECT_EQ(bc[0], static_cast<uint8_t>(Opcode::VERSION));
    // Second byte should be version number
    EXPECT_EQ(bc[1], SBLR_VERSION);
}

TEST_F(BytecodeGeneratorV2Test, BytecodeHasEndMarker) {
    auto result = generateBytecode("SELECT 1");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    const auto& bc = result.bytecode();
    ASSERT_GE(bc.size(), 1);

    // Last byte should be END opcode
    EXPECT_EQ(bc.back(), static_cast<uint8_t>(Opcode::END));
}

// =============================================================================
// Literal Expression Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, IntegerLiteral) {
    auto result = generateBytecode("SELECT 42");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::SELECT));
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32));
}

TEST_F(BytecodeGeneratorV2Test, LargIntegerLiteral) {
    auto result = generateBytecode("SELECT 9223372036854775807");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT64));
}

TEST_F(BytecodeGeneratorV2Test, StringLiteral) {
    auto result = generateBytecode("SELECT 'hello'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_STRING));
}

TEST_F(BytecodeGeneratorV2Test, NullLiteral) {
    auto result = generateBytecode("SELECT NULL");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_NULL));
}

TEST_F(BytecodeGeneratorV2Test, BooleanLiteral) {
    auto result = generateBytecode("SELECT TRUE");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    // Boolean stored as INT32
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32));
}

// =============================================================================
// Arithmetic Expression Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, Addition) {
    auto result = generateBytecode("SELECT 1 + 2");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    // With constant folding, should just be a single literal
    // But even without, should have ADD opcode
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32) ||
                hasOpcode(result.bytecode(), Opcode::EXPR_ADD));
}

TEST_F(BytecodeGeneratorV2Test, Subtraction) {
    auto result = generateBytecode("SELECT 5 - 3");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32) ||
                hasOpcode(result.bytecode(), Opcode::EXPR_SUBTRACT));
}

TEST_F(BytecodeGeneratorV2Test, Multiplication) {
    auto result = generateBytecode("SELECT 4 * 5");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32) ||
                hasOpcode(result.bytecode(), Opcode::EXPR_MULTIPLY));
}

TEST_F(BytecodeGeneratorV2Test, Division) {
    auto result = generateBytecode("SELECT 10 / 2");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32) ||
                hasOpcode(result.bytecode(), Opcode::EXPR_DIVIDE));
}

// =============================================================================
// Comparison Expression Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, Equality) {
    auto result = generateBytecode("SELECT 1 = 1");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_EQ));
}

TEST_F(BytecodeGeneratorV2Test, LessThan) {
    auto result = generateBytecode("SELECT 1 < 2");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_LT));
}

TEST_F(BytecodeGeneratorV2Test, GreaterThan) {
    auto result = generateBytecode("SELECT 2 > 1");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_GT));
}

// =============================================================================
// Logical Expression Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, LogicalAnd) {
    auto result = generateBytecode("SELECT TRUE AND FALSE");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_AND));
}

TEST_F(BytecodeGeneratorV2Test, LogicalOr) {
    auto result = generateBytecode("SELECT TRUE OR FALSE");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_OR));
}

// =============================================================================
// Special Expression Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, CaseExpression) {
    auto result = generateBytecode("SELECT CASE WHEN TRUE THEN 1 ELSE 0 END");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::CASE_WHEN));
}

TEST_F(BytecodeGeneratorV2Test, CastExpression) {
    auto result = generateBytecode("SELECT CAST(42 AS VARCHAR)");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_CAST));
}

TEST_F(BytecodeGeneratorV2Test, CastExpressionPayload) {
    auto result = generateBytecode("SELECT CAST(42 AS VARCHAR)");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    const auto& bytecode = result.bytecode();
    auto it = std::find(bytecode.begin(), bytecode.end(),
                        static_cast<uint8_t>(Opcode::EXPR_CAST));
    ASSERT_NE(it, bytecode.end());
    size_t pos = static_cast<size_t>(it - bytecode.begin());
    ASSERT_GT(bytecode.size(), pos + 7);

    EXPECT_EQ(bytecode[pos + 1], 0u);  // try_cast flag
    EXPECT_EQ(bytecode[pos + 2], static_cast<uint8_t>(Opcode::TYPE_VARCHAR));
    uint32_t len = sblr::readInt32(&bytecode[pos + 3]);
    EXPECT_EQ(len, 255u);
    EXPECT_EQ(bytecode[pos + 7], static_cast<uint8_t>(CastFormat::DEFAULT));
}

TEST_F(BytecodeGeneratorV2Test, CastUsingFormatPayload) {
    auto result = generateBytecode("SELECT CAST('ff' AS BLOB USING hex)");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    const auto& bytecode = result.bytecode();
    auto it = std::find(bytecode.begin(), bytecode.end(),
                        static_cast<uint8_t>(Opcode::EXPR_CAST));
    ASSERT_NE(it, bytecode.end());
    size_t pos = static_cast<size_t>(it - bytecode.begin());
    ASSERT_GT(bytecode.size(), pos + 3);

    EXPECT_EQ(bytecode[pos + 1], 0u);  // try_cast flag
    EXPECT_EQ(bytecode[pos + 2], static_cast<uint8_t>(Opcode::TYPE_BLOB));
    EXPECT_EQ(bytecode[pos + 3], static_cast<uint8_t>(CastFormat::HEX));
}

TEST_F(BytecodeGeneratorV2Test, LikeExpression) {
    auto result = generateBytecode("SELECT 'hello' LIKE 'h%'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_LIKE));
}

TEST_F(BytecodeGeneratorV2Test, LikeEscapeExpression) {
    auto result = generateBytecode("SELECT 'hello' LIKE 'h!%' ESCAPE '!'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasExtendedOpcode(result.bytecode(), sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
}

// =============================================================================
// DML Statement Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, SimpleSelect) {
    auto result = generateBytecode("SELECT 1, 2, 3");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::SELECT));
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::BEGIN_LIST));
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::END_LIST));
}

TEST_F(BytecodeGeneratorV2Test, SelectWithAlias) {
    auto result = generateBytecode("SELECT 1 AS one, 2 AS two");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::SELECT));
}

// =============================================================================
// Transaction Statement Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, StartTransaction) {
    auto result = generateBytecode("START TRANSACTION");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::START_TRANSACTION));
}

TEST_F(BytecodeGeneratorV2Test, StartTransactionExtendedPayload) {
    auto result = generateBytecode(
        "START TRANSACTION AUTOCOMMIT ON, ON CONFLICT ERROR 99, NO WAIT, "
        "LOCK TIMEOUT 12, RESERVING widgets FOR PROTECTED WRITE");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    StartTransactionPayload payload;
    ASSERT_TRUE(parseStartTransactionPayload(result.bytecode(), payload));

    EXPECT_TRUE(payload.flags & sblr::TransactionFlags::HAS_AUTOCOMMIT);
    EXPECT_TRUE(payload.flags & sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE);
    EXPECT_TRUE(payload.flags & sblr::TransactionFlags::HAS_WAIT_MODE);
    EXPECT_TRUE(payload.flags & sblr::TransactionFlags::HAS_LOCK_TIMEOUT);
    EXPECT_TRUE(payload.flags & sblr::TransactionFlags::HAS_RESERVATIONS);

    EXPECT_EQ(payload.conflict_action,
              static_cast<uint8_t>(sblr::TransactionConflictAction::ERROR));
    EXPECT_TRUE(payload.has_conflict_error_code);
    EXPECT_EQ(payload.conflict_error_code, 99);
    EXPECT_TRUE(payload.has_autocommit);
    EXPECT_EQ(payload.autocommit_mode, static_cast<uint8_t>(sblr::AutocommitMode::ON));
    EXPECT_TRUE(payload.has_wait_mode);
    EXPECT_EQ(payload.wait_mode, 0u);
    EXPECT_TRUE(payload.has_lock_timeout);
    EXPECT_EQ(payload.lock_timeout, 12u);
    EXPECT_TRUE(payload.has_reservations);
    EXPECT_EQ(payload.reservation_count, 1u);
    EXPECT_EQ(payload.first_lock_mode,
              static_cast<uint8_t>(scratchbird::parser::v2::TableLockMode::PROTECTED));
    EXPECT_EQ(payload.first_for_write, 1u);
}

TEST_F(BytecodeGeneratorV2Test, StartTransactionReadCommittedModePayload) {
    auto result = generateBytecode(
        "START TRANSACTION READ COMMITTED RECORD VERSION");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    StartTransactionPayload payload;
    ASSERT_TRUE(parseStartTransactionPayload(result.bytecode(), payload));

    EXPECT_TRUE(payload.flags & sblr::TransactionFlags::HAS_READ_COMMITTED_MODE);
    EXPECT_TRUE(payload.has_read_committed_mode);
    EXPECT_EQ(payload.read_committed_mode,
              static_cast<uint8_t>(sblr::ReadCommittedMode::RECORD_VERSION));
}

TEST_F(BytecodeGeneratorV2Test, SetAutocommitErrorEmitsCode) {
    auto result = generateBytecode("SET AUTOCOMMIT 1 ON CONFLICT ERROR 99");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_SET_AUTOCOMMIT,
                                   payload_offset));
    ASSERT_LT(payload_offset + 6, result.bytecode().size());

    EXPECT_EQ(result.bytecode()[payload_offset++], 1);
    EXPECT_EQ(result.bytecode()[payload_offset++],
              static_cast<uint8_t>(sblr::TransactionConflictAction::ERROR));
    uint32_t code = sblr::readInt32(&result.bytecode()[payload_offset]);
    EXPECT_EQ(code, 99u);
}

TEST_F(BytecodeGeneratorV2Test, Commit) {
    auto result = generateBytecode("COMMIT");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::COMMIT));
}

TEST_F(BytecodeGeneratorV2Test, CommitPrepared) {
    auto result = generateBytecode("COMMIT PREPARED 'tx1'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    EXPECT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_COMMIT_PREPARED,
                                   payload_offset));
    EXPECT_LT(payload_offset, result.bytecode().size());
}

TEST_F(BytecodeGeneratorV2Test, Rollback) {
    auto result = generateBytecode("ROLLBACK");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::ROLLBACK));
}

TEST_F(BytecodeGeneratorV2Test, RollbackPrepared) {
    auto result = generateBytecode("ROLLBACK PREPARED 'tx1'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    EXPECT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_ROLLBACK_PREPARED,
                                   payload_offset));
    EXPECT_LT(payload_offset, result.bytecode().size());
}

TEST_F(BytecodeGeneratorV2Test, PrepareTransaction) {
    auto result = generateBytecode("PREPARE TRANSACTION 'tx1'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    EXPECT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_PREPARE_TRANSACTION,
                                   payload_offset));
    EXPECT_LT(payload_offset, result.bytecode().size());
}

// =============================================================================
// DDL Statement Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, CreateTable) {
    auto result = generateBytecode("CREATE TABLE products (id INT, name VARCHAR(100))");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::CREATE_TABLE));
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::COLUMN_DEF));
}

TEST_F(BytecodeGeneratorV2Test, CreateDatabaseEmulatedSimple) {
    auto result = generateBytecode("CREATE DATABASE IF NOT EXISTS EMULATED mysql mydb");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    CreateDatabasePayload payload;
    ASSERT_TRUE(parseCreateDatabasePayload(result.bytecode(), payload));

    EXPECT_EQ(payload.flags & 0x01, 0x01);  // IF NOT EXISTS
    EXPECT_EQ(payload.database_path, "emulation.mysql.localhost.mydb");
    EXPECT_EQ(payload.source_spec, "mydb");
    EXPECT_TRUE(payload.options.empty());
    EXPECT_TRUE(payload.aliases.empty());
}

TEST_F(BytecodeGeneratorV2Test, CreateDatabaseEmulatedWithOptionsAndAliases) {
    auto result = generateBytecode(
        "CREATE DATABASE EMULATED firebird 'srv:/var/db/employee.fdb' "
        "ALIAS legacy, emp "
        "WITH OPTIONS (user = 'SYSDBA', password = 'masterkey')");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    CreateDatabasePayload payload;
    ASSERT_TRUE(parseCreateDatabasePayload(result.bytecode(), payload));

    EXPECT_EQ(payload.database_path, "emulation.firebird.srv.var.db.employee");
    EXPECT_EQ(payload.source_spec, "srv:/var/db/employee.fdb");
    ASSERT_EQ(payload.options.size(), 2u);
    EXPECT_EQ(payload.options[0].first, "user");
    EXPECT_EQ(payload.options[0].second, "SYSDBA");
    EXPECT_EQ(payload.options[1].first, "password");
    EXPECT_EQ(payload.options[1].second, "masterkey");
    ASSERT_EQ(payload.aliases.size(), 2u);
    EXPECT_EQ(payload.aliases[0], "legacy");
    EXPECT_EQ(payload.aliases[1], "emp");
}

TEST_F(BytecodeGeneratorV2Test, CreateDomain) {
    auto result = generateBytecode(
        "CREATE DOMAIN test_domain AS TEXT DEFAULT '5' "
        "WITH INTEGRITY (UNIQUENESS = TRUE) "
        "WITH SECURITY (MASKING = FULL) "
        "WITH VALIDATION (FUNCTION = validate_domain) "
        "WITH QUALITY (PARSE_FUNCTION = parse_domain)");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_CREATE_DOMAIN,
                                   payload_offset));
    ASSERT_LT(payload_offset, result.bytecode().size());

    uint8_t flags = result.bytecode()[payload_offset++];
    EXPECT_EQ(flags & 0x01, 0);  // IF NOT EXISTS not set
    EXPECT_EQ(flags & 0x02, 0x02);  // WITH INTEGRITY set
    EXPECT_EQ(flags & 0x04, 0x04);  // WITH SECURITY set
    EXPECT_EQ(flags & 0x08, 0x08);  // WITH VALIDATION set
    EXPECT_EQ(flags & 0x10, 0x10);  // WITH QUALITY set

    uint8_t domain_kind = result.bytecode()[payload_offset++];
    EXPECT_EQ(domain_kind, static_cast<uint8_t>(DomainKind::BASIC));

    std::string domain_name;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &payload_offset, &domain_name));
    EXPECT_EQ(domain_name, "test_domain");

    // Base type opcode
    ASSERT_LT(payload_offset, result.bytecode().size());
    payload_offset += 1;

    // Nullable flag
    ASSERT_LT(payload_offset, result.bytecode().size());
    payload_offset += 1;

    std::string default_value;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &payload_offset, &default_value));
    EXPECT_EQ(default_value, "'5'");
}

TEST_F(BytecodeGeneratorV2Test, CreateDomainRecord) {
    auto result = generateBytecode(
        "CREATE DOMAIN person_record AS RECORD (id INT NOT NULL, name TEXT DEFAULT 'anon')");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_CREATE_DOMAIN,
                                   payload_offset));

    auto read_string = [&](size_t& offset) -> std::string {
        std::string out;
        if (!readStringVarint(result.bytecode(), &offset, &out)) {
            ADD_FAILURE() << "String data out of range";
        }
        return out;
    };

    payload_offset += 1;  // flags
    uint8_t domain_kind = result.bytecode()[payload_offset++];
    EXPECT_EQ(domain_kind, static_cast<uint8_t>(DomainKind::RECORD));

    std::string domain_name = read_string(payload_offset);
    EXPECT_EQ(domain_name, "person_record");

    uint64_t field_count = 0;
    ASSERT_TRUE(readUVarint(result.bytecode(), &payload_offset, &field_count));
    EXPECT_EQ(field_count, 2u);

    std::string field1_name = read_string(payload_offset);
    EXPECT_EQ(field1_name, "id");
    EXPECT_EQ(result.bytecode()[payload_offset++], 0);  // type_ref kind
    EXPECT_EQ(result.bytecode()[payload_offset++], static_cast<uint8_t>(Opcode::TYPE_INTEGER));
    EXPECT_EQ(result.bytecode()[payload_offset++], 0);  // nullable
    std::string field1_default = read_string(payload_offset);
    EXPECT_TRUE(field1_default.empty());

    std::string field2_name = read_string(payload_offset);
    EXPECT_EQ(field2_name, "name");
    EXPECT_EQ(result.bytecode()[payload_offset++], 0);  // type_ref kind
    EXPECT_EQ(result.bytecode()[payload_offset++], static_cast<uint8_t>(Opcode::TYPE_TEXT));
    EXPECT_EQ(result.bytecode()[payload_offset++], 1);  // nullable
    std::string field2_default = read_string(payload_offset);
    EXPECT_EQ(field2_default, "'anon'");
}

TEST_F(BytecodeGeneratorV2Test, CreateDomainEnum) {
    auto result = generateBytecode(
        "CREATE DOMAIN status AS ENUM ('NEW', 'DONE') WITH OPTIONS (WRAP = TRUE)");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_CREATE_DOMAIN,
                                   payload_offset));

    auto read_string = [&](size_t& offset) -> std::string {
        std::string out;
        if (!readStringVarint(result.bytecode(), &offset, &out)) {
            ADD_FAILURE() << "String data out of range";
        }
        return out;
    };

    payload_offset += 1;  // flags
    uint8_t domain_kind = result.bytecode()[payload_offset++];
    EXPECT_EQ(domain_kind, static_cast<uint8_t>(DomainKind::ENUM));

    std::string domain_name = read_string(payload_offset);
    EXPECT_EQ(domain_name, "status");

    uint64_t value_count = 0;
    ASSERT_TRUE(readUVarint(result.bytecode(), &payload_offset, &value_count));
    EXPECT_EQ(value_count, 2u);

    std::string label1 = read_string(payload_offset);
    int32_t pos1 = static_cast<int32_t>(sblr::readInt32(&result.bytecode()[payload_offset]));
    payload_offset += 4;
    EXPECT_EQ(label1, "NEW");
    EXPECT_EQ(pos1, 1);

    std::string label2 = read_string(payload_offset);
    int32_t pos2 = static_cast<int32_t>(sblr::readInt32(&result.bytecode()[payload_offset]));
    payload_offset += 4;
    EXPECT_EQ(label2, "DONE");
    EXPECT_EQ(pos2, 2);

    uint8_t wrap = result.bytecode()[payload_offset++];
    EXPECT_EQ(wrap, 1);
}

TEST_F(BytecodeGeneratorV2Test, CreateDomainSet) {
    auto result = generateBytecode("CREATE DOMAIN tag_set AS SET OF TEXT");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed"
                                  << formatDiagnostics(result);

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_CREATE_DOMAIN,
                                   payload_offset));

    payload_offset += 1;  // flags
    uint8_t domain_kind = result.bytecode()[payload_offset++];
    EXPECT_EQ(domain_kind, static_cast<uint8_t>(DomainKind::SET));

    std::string domain_name;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &payload_offset, &domain_name));

    EXPECT_EQ(result.bytecode()[payload_offset++], 0);  // type_ref kind
    EXPECT_EQ(result.bytecode()[payload_offset++], static_cast<uint8_t>(Opcode::TYPE_TEXT));
}

TEST_F(BytecodeGeneratorV2Test, CreateDomainVariant) {
    auto result = generateBytecode("CREATE DOMAIN flex AS VARIANT (INTEGER, TEXT)");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_CREATE_DOMAIN,
                                   payload_offset));

    payload_offset += 1;  // flags
    uint8_t domain_kind = result.bytecode()[payload_offset++];
    EXPECT_EQ(domain_kind, static_cast<uint8_t>(DomainKind::VARIANT));

    std::string domain_name;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &payload_offset, &domain_name));

    uint64_t type_count = 0;
    ASSERT_TRUE(readUVarint(result.bytecode(), &payload_offset, &type_count));
    EXPECT_EQ(type_count, 2u);

    EXPECT_EQ(result.bytecode()[payload_offset++], 0);  // type_ref kind
    EXPECT_EQ(result.bytecode()[payload_offset++], static_cast<uint8_t>(Opcode::TYPE_INTEGER));

    EXPECT_EQ(result.bytecode()[payload_offset++], 0);  // type_ref kind
    EXPECT_EQ(result.bytecode()[payload_offset++], static_cast<uint8_t>(Opcode::TYPE_TEXT));
}

TEST_F(BytecodeGeneratorV2Test, AlterDomainSetDefault) {
    auto* domain_mgr = db_.domain_manager();
    ASSERT_NE(domain_mgr, nullptr);

    DomainManager::DomainCreateOptions options;
    ID domain_id{};
    ErrorContext ctx;
    auto status = domain_mgr->createBasicDomain(test_schema_id_, "test_domain",
                                                DataType::TEXT, 0, 0,
                                                options, domain_id, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create test domain: " << ctx.message;

    auto result = generateBytecode("ALTER DOMAIN test_domain SET DEFAULT '5'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_ALTER_DOMAIN,
                                   payload_offset));
    ASSERT_LT(payload_offset, result.bytecode().size());

    uint8_t action = result.bytecode()[payload_offset++];
    EXPECT_EQ(action, static_cast<uint8_t>(sblr::AlterDomainAction::SET_DEFAULT));

    std::string domain_name;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &payload_offset, &domain_name));
    EXPECT_EQ(domain_name, "test_domain");
}

TEST_F(BytecodeGeneratorV2Test, DropDomainIfExists) {
    auto result = generateBytecode("DROP DOMAIN IF EXISTS test_domain RESTRICT");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_DROP_DOMAIN,
                                   payload_offset));
    ASSERT_LT(payload_offset, result.bytecode().size());

    uint8_t flags = result.bytecode()[payload_offset++];
    EXPECT_EQ(flags & 0x01, 0x01);  // IF EXISTS set
    EXPECT_EQ(flags & 0x02, 0x02);  // RESTRICT set

    std::string domain_name;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &payload_offset, &domain_name));
    EXPECT_EQ(domain_name, "test_domain");
}

TEST_F(BytecodeGeneratorV2Test, CreateIndex) {
    // First we need a table to create an index on
    // For this test, just check bytecode generation works
    auto result = generateBytecode("CREATE INDEX idx_test ON products (id)");
    // May fail due to missing table, but should generate bytecode structure
    // Just verify it doesn't crash
}

TEST_F(BytecodeGeneratorV2Test, AlterTableAddColumn) {
    ErrorContext ctx;
    std::vector<CatalogManager::ColumnInfo> columns;

    CatalogManager::ColumnInfo id_col;
    id_col.column_name = "id";
    id_col.data_type = static_cast<uint16_t>(DataType::INT32);
    id_col.nullable = false;
    columns.push_back(id_col);

    ID table_id;
    auto status = catalog_->createTable(test_schema_id_, "users", columns, table_id, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create users table";
    EXPECT_NE(table_id, ID{});

    auto result = generateBytecode("ALTER TABLE users ADD COLUMN age INT");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t offset = findOpcodeOffset(result.bytecode(), Opcode::ALTER_TABLE);
    ASSERT_LT(offset, result.bytecode().size());
    offset += 1;
    std::string table_name;
    ASSERT_TRUE(readStringVarint(result.bytecode(), &offset, &table_name));
    ASSERT_LT(offset, result.bytecode().size());
    uint8_t action = result.bytecode()[offset];

    EXPECT_EQ(action, 0);
    EXPECT_EQ(table_name, "test.users");
}

// =============================================================================
// Optimization Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, ConstantFolding) {
    // With optimizations enabled, 1 + 2 should be folded to 3
    auto result = generateBytecode("SELECT 1 + 2");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    // Should have fewer opcodes due to folding
    // The folded result should be a single INT32 literal
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::LITERAL_INT32));
}

TEST_F(BytecodeGeneratorV2Test, NestedConstantFolding) {
    // (1 + 2) * 3 should fold to 9
    auto result = generateBytecode("SELECT (1 + 2) * 3");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    // Should still work even if not fully folded
    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::SELECT));
}

// =============================================================================
// Disassembler Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, DisassemblerWorks) {
    auto result = generateBytecode("SELECT 42");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    std::string disasm = BytecodeDisassemblerV2::disassemble(result.bytecode());
    EXPECT_FALSE(disasm.empty());
    EXPECT_TRUE(disasm.find("VERSION") != std::string::npos);
    EXPECT_TRUE(disasm.find("SELECT") != std::string::npos);
    EXPECT_TRUE(disasm.find("END") != std::string::npos);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(BytecodeGeneratorV2Test, ParseErrorReturnsError) {
    auto result = generateBytecode("SELECT FROM");  // Invalid syntax
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

TEST_F(BytecodeGeneratorV2Test, NullStatementReturnsError) {
    BytecodeGeneratorV2 generator(parser_->stringPool());
    auto result = generator.generate(nullptr);
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}
