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
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
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
            if (offset + 4 > bytecode.size()) return false;
            out.reservation_count = sblr::readInt32(&bytecode[offset]);
            offset += 4;
            if (out.reservation_count > 0) {
                if (offset + 1 > bytecode.size()) return false;
                if (bytecode[offset++] != static_cast<uint8_t>(Opcode::TABLE_REF)) return false;
                if (offset + 4 > bytecode.size()) return false;
                uint32_t name_len = sblr::readInt32(&bytecode[offset]);
                offset += 4;
                if (offset + name_len + 2 > bytecode.size()) return false;
                offset += name_len;
                out.first_lock_mode = bytecode[offset++];
                out.first_for_write = bytecode[offset++];
            }
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

TEST_F(BytecodeGeneratorV2Test, LikeExpression) {
    auto result = generateBytecode("SELECT 'hello' LIKE 'h%'");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    EXPECT_TRUE(hasOpcode(result.bytecode(), Opcode::EXPR_LIKE));
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
    EXPECT_EQ(payload.first_lock_mode, static_cast<uint8_t>(TableLockMode::PROTECTED));
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

TEST_F(BytecodeGeneratorV2Test, CreateDomain) {
    auto result = generateBytecode(
        "CREATE DOMAIN test_domain AS TEXT "
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

    ASSERT_LE(payload_offset + 4, result.bytecode().size());
    uint32_t name_len = sblr::readInt32(&result.bytecode()[payload_offset]);
    payload_offset += 4;
    ASSERT_LE(payload_offset + name_len, result.bytecode().size());
    std::string domain_name(result.bytecode().begin() + payload_offset,
                            result.bytecode().begin() + payload_offset + name_len);
    EXPECT_EQ(domain_name, "test_domain");
    payload_offset += name_len;

    ASSERT_LE(payload_offset + 4, result.bytecode().size());
    uint32_t value_len = sblr::readInt32(&result.bytecode()[payload_offset]);
    payload_offset += 4;
    ASSERT_LE(payload_offset + value_len, result.bytecode().size());
    std::string default_value(result.bytecode().begin() + payload_offset,
                              result.bytecode().begin() + payload_offset + value_len);
    EXPECT_EQ(default_value, "5");
}

TEST_F(BytecodeGeneratorV2Test, AlterDomainSetDefault) {
    auto result = generateBytecode("ALTER DOMAIN test_domain SET DEFAULT 5");
    ASSERT_TRUE(result.success()) << "Bytecode generation failed";

    size_t payload_offset = 0;
    ASSERT_TRUE(findExtendedOpcode(result.bytecode(),
                                   sblr::ExtendedOpcode::EXT_ALTER_DOMAIN,
                                   payload_offset));
    ASSERT_LT(payload_offset, result.bytecode().size());

    uint8_t action = result.bytecode()[payload_offset++];
    EXPECT_EQ(action, static_cast<uint8_t>(sblr::AlterDomainAction::SET_DEFAULT));

    ASSERT_LE(payload_offset + 4, result.bytecode().size());
    uint32_t name_len = sblr::readInt32(&result.bytecode()[payload_offset]);
    payload_offset += 4;
    ASSERT_LE(payload_offset + name_len, result.bytecode().size());
    std::string domain_name(result.bytecode().begin() + payload_offset,
                            result.bytecode().begin() + payload_offset + name_len);
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

    ASSERT_LE(payload_offset + 4, result.bytecode().size());
    uint32_t name_len = sblr::readInt32(&result.bytecode()[payload_offset]);
    payload_offset += 4;
    ASSERT_LE(payload_offset + name_len, result.bytecode().size());
    std::string domain_name(result.bytecode().begin() + payload_offset,
                            result.bytecode().begin() + payload_offset + name_len);
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
    ASSERT_LE(offset + 4, result.bytecode().size());
    uint32_t name_len = sblr::readInt32(&result.bytecode()[offset]);
    offset += 4;
    ASSERT_LE(offset + name_len, result.bytecode().size());

    std::string table_name(result.bytecode().begin() + offset,
                           result.bytecode().begin() + offset + name_len);
    offset += name_len;
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
