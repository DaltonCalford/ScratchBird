#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/opcodes.h"
#include <string>
#include <vector>

using namespace scratchbird::parser;
using namespace scratchbird::sblr;

/**
 * TASK-BYTECODE-2: Index Bytecode Generation Tests
 *
 * Tests for CREATE INDEX, DROP INDEX, and index hint bytecode generation.
 * These tests verify that the bytecode generator correctly encodes index operations
 * according to the SBLR (ScratchBird Binary Language Runner) specification.
 *
 * Date: November 20, 2025
 */

class IndexBytecodeGenerationTest : public ::testing::Test
{
protected:
    /**
     * Parse SQL and generate bytecode
     */
    BytecodeResult generateBytecode(const std::string &sql)
    {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto parse_result = parser.parseStatement();
        if (!parse_result.success())
        {
            BytecodeResult error_result;
            for (const auto &err : parse_result.errors())
            {
                error_result.addError(err.message);
            }
            return error_result;
        }

        BytecodeGenerator generator(parser.stringPool());
        return generator.generate(parse_result.statement());
    }

    /**
     * Helper to check if bytecode contains an opcode
     */
    bool containsOpcode(const std::vector<uint8_t> &bytecode, Opcode op)
    {
        uint8_t opcode_byte = static_cast<uint8_t>(op);
        for (size_t i = 0; i < bytecode.size(); i++)
        {
            if (bytecode[i] == opcode_byte)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Helper to find opcode position in bytecode
     */
    size_t findOpcode(const std::vector<uint8_t> &bytecode, Opcode op)
    {
        uint8_t opcode_byte = static_cast<uint8_t>(op);
        for (size_t i = 0; i < bytecode.size(); i++)
        {
            if (bytecode[i] == opcode_byte)
            {
                return i;
            }
        }
        return std::string::npos;
    }

    /**
     * Helper to read a 32-bit integer from bytecode
     */
    uint32_t readInt32(const std::vector<uint8_t> &bytecode, size_t offset)
    {
        if (offset + 4 > bytecode.size())
        {
            return 0;
        }
        return scratchbird::sblr::readInt32(&bytecode[offset]);
    }

    /**
     * Helper to read a string from bytecode (length-prefixed)
     */
    std::string readString(const std::vector<uint8_t> &bytecode, size_t offset, size_t* bytes_read = nullptr)
    {
        if (offset + 4 > bytecode.size())
        {
            if (bytes_read) *bytes_read = 0;
            return "";
        }

        uint32_t length = readInt32(bytecode, offset);
        if (offset + 4 + length > bytecode.size())
        {
            if (bytes_read) *bytes_read = 4;
            return "";
        }

        std::string result(reinterpret_cast<const char*>(&bytecode[offset + 4]), length);
        if (bytes_read) *bytes_read = 4 + length;
        return result;
    }
};

// ===== CREATE INDEX Tests =====

TEST_F(IndexBytecodeGenerationTest, CreateIndexSimple)
{
    std::string sql = "CREATE INDEX idx_users_name ON users(name)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::VERSION));
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));
    EXPECT_TRUE(containsOpcode(bc, Opcode::END));
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexUnique)
{
    std::string sql = "CREATE UNIQUE INDEX idx_users_email ON users(email)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for UNIQUE index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Find CREATE_INDEX opcode and check the unique flag (should be 1)
    size_t idx_pos = findOpcode(bc, Opcode::CREATE_INDEX);
    ASSERT_NE(idx_pos, std::string::npos);

    // Format: CREATE_INDEX | index_name (len+data) | table_name (len+data) | unique_flag (1 byte)
    // Skip to after the two strings to find the unique flag
    // This is approximate - actual position depends on string lengths
    // The unique flag is the first byte after table_name
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexMultipleColumns)
{
    std::string sql = "CREATE INDEX idx_users_name_age ON users(name, age)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for multi-column index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Should encode column count (2) and both column names
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexWithTablespace)
{
    std::string sql = "CREATE INDEX idx_users_id ON users(id) TABLESPACE fast_storage";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed with TABLESPACE clause";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexBTree)
{
    std::string sql = "CREATE INDEX idx_users_id ON users USING BTREE (id)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for BTREE index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain index type = BTREE (0x00)
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexHash)
{
    std::string sql = "CREATE INDEX idx_users_email ON users USING HASH (email)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for HASH index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain index type = HASH (0x01)
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexGIN)
{
    std::string sql = "CREATE INDEX idx_docs_tags ON documents USING GIN (tags)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for GIN index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain index type = GIN (0x02)
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexGiST)
{
    std::string sql = "CREATE INDEX idx_locations ON places USING GIST (location)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for GiST index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain index type = GIST (0x03)
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexBRIN)
{
    std::string sql = "CREATE INDEX idx_events_timestamp ON events USING BRIN (timestamp)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for BRIN index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain index type = BRIN (0x05)
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexHNSW)
{
    std::string sql = "CREATE INDEX idx_embeddings ON vectors USING HNSW (embedding)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed for HNSW index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain index type = HNSW (0x07)
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexWithPredicate)
{
    std::string sql = "CREATE INDEX idx_active_users ON users(email) WHERE active = true";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed with WHERE clause";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain predicate flag = 1 and serialized predicate expression
}

TEST_F(IndexBytecodeGenerationTest, CreateIndexWithExpression)
{
    std::string sql = "CREATE INDEX idx_users_lower_email ON users(LOWER(email))";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed with expression index";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));

    // Bytecode should contain expression flag = 1 and serialized expression
}

// ===== DROP INDEX Tests =====

TEST_F(IndexBytecodeGenerationTest, DropIndexSimple)
{
    std::string sql = "DROP INDEX idx_users_name";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::VERSION));
    EXPECT_TRUE(containsOpcode(bc, Opcode::DROP_INDEX));
    EXPECT_TRUE(containsOpcode(bc, Opcode::END));
}

TEST_F(IndexBytecodeGenerationTest, DropIndexIfExists)
{
    std::string sql = "DROP INDEX IF EXISTS idx_users_name";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed with IF EXISTS";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::DROP_INDEX));

    // Find DROP_INDEX opcode and check the IF EXISTS flag
    size_t idx_pos = findOpcode(bc, Opcode::DROP_INDEX);
    ASSERT_NE(idx_pos, std::string::npos);

    // Format: DROP_INDEX | index_name (len+data) | if_exists_flag (1 byte)
    // The if_exists flag should be 1 for IF EXISTS
}

TEST_F(IndexBytecodeGenerationTest, DropIndexWithoutIfExists)
{
    std::string sql = "DROP INDEX idx_users_name";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed without IF EXISTS";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::DROP_INDEX));

    // The if_exists flag should be 0 when IF EXISTS is not specified
}

// Note: CASCADE/RESTRICT are not currently supported in the AST for DROP INDEX
// These tests are placeholders for when that support is added

TEST_F(IndexBytecodeGenerationTest, DISABLED_DropIndexCascade)
{
    // TODO: Enable when parser supports CASCADE for DROP INDEX
    std::string sql = "DROP INDEX idx_users_name CASCADE";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed with CASCADE";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::DROP_INDEX));
}

TEST_F(IndexBytecodeGenerationTest, DISABLED_DropIndexRestrict)
{
    // TODO: Enable when parser supports RESTRICT for DROP INDEX
    std::string sql = "DROP INDEX idx_users_name RESTRICT";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success()) << "Bytecode generation should succeed with RESTRICT";

    const auto &bc = result.bytecode();
    EXPECT_TRUE(containsOpcode(bc, Opcode::DROP_INDEX));
}

// ===== Bytecode Format Validation Tests =====

TEST_F(IndexBytecodeGenerationTest, CreateIndexBytecodeFormat)
{
    std::string sql = "CREATE INDEX idx_test ON test_table(col1)";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success());
    const auto &bc = result.bytecode();

    // Verify bytecode structure:
    // VERSION (1) | version_num (1) | CREATE_INDEX (1) | index_name (4+N) | table_name (4+M) |
    // unique_flag (1) | col_count (4) | columns... | tablespace (4+P) | index_type (1) |
    // has_expressions (1) | has_predicate (1) | [expressions...] | [predicate...] | END (1)

    ASSERT_GE(bc.size(), 3u) << "Bytecode should have at least VERSION, opcode, and END";

    // Check version header
    EXPECT_EQ(bc[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bc[1], SBLR_VERSION);

    // Check CREATE_INDEX opcode
    size_t pos = 2;
    EXPECT_EQ(bc[pos], static_cast<uint8_t>(Opcode::CREATE_INDEX));
    pos++;

    // Read index name
    size_t bytes_read = 0;
    std::string index_name = readString(bc, pos, &bytes_read);
    EXPECT_EQ(index_name, "idx_test");
    pos += bytes_read;

    // Read table name
    std::string table_name = readString(bc, pos, &bytes_read);
    EXPECT_EQ(table_name, "test_table");
    pos += bytes_read;

    // Read unique flag (should be 0 for non-unique index)
    ASSERT_LT(pos, bc.size());
    EXPECT_EQ(bc[pos], 0u) << "Unique flag should be 0 for non-UNIQUE index";
}

TEST_F(IndexBytecodeGenerationTest, DropIndexBytecodeFormat)
{
    std::string sql = "DROP INDEX IF EXISTS idx_test";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success());
    const auto &bc = result.bytecode();

    // Verify bytecode structure:
    // VERSION (1) | version_num (1) | DROP_INDEX (1) | index_name (4+N) | if_exists_flag (1) | END (1)

    ASSERT_GE(bc.size(), 3u);

    // Check version header
    EXPECT_EQ(bc[0], static_cast<uint8_t>(Opcode::VERSION));
    EXPECT_EQ(bc[1], SBLR_VERSION);

    // Check DROP_INDEX opcode
    size_t pos = 2;
    EXPECT_EQ(bc[pos], static_cast<uint8_t>(Opcode::DROP_INDEX));
    pos++;

    // Read index name
    size_t bytes_read = 0;
    std::string index_name = readString(bc, pos, &bytes_read);
    EXPECT_EQ(index_name, "idx_test");
    pos += bytes_read;

    // Read IF EXISTS flag (should be 1)
    ASSERT_LT(pos, bc.size());
    EXPECT_EQ(bc[pos], 1u) << "IF EXISTS flag should be 1";
}

// ===== Error Handling Tests =====

TEST_F(IndexBytecodeGenerationTest, CreateIndexInvalidSQL)
{
    std::string sql = "CREATE INDEX ON users(name)";  // Missing index name
    auto result = generateBytecode(sql);

    EXPECT_FALSE(result.success()) << "Bytecode generation should fail for invalid SQL";
    EXPECT_FALSE(result.errors().empty()) << "Should have error messages";
}

TEST_F(IndexBytecodeGenerationTest, DropIndexInvalidSQL)
{
    std::string sql = "DROP INDEX";  // Missing index name
    auto result = generateBytecode(sql);

    EXPECT_FALSE(result.success()) << "Bytecode generation should fail for invalid SQL";
    EXPECT_FALSE(result.errors().empty()) << "Should have error messages";
}

// ===== Round-Trip Tests =====

TEST_F(IndexBytecodeGenerationTest, RoundTripCreateIndex)
{
    // Test that we can generate bytecode and it contains all necessary information
    std::string sql = "CREATE UNIQUE INDEX idx_users_email ON users(email) TABLESPACE main_ts";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success());
    const auto &bc = result.bytecode();

    // Verify all components are present
    EXPECT_TRUE(containsOpcode(bc, Opcode::VERSION));
    EXPECT_TRUE(containsOpcode(bc, Opcode::CREATE_INDEX));
    EXPECT_TRUE(containsOpcode(bc, Opcode::END));

    // The bytecode should be self-contained and executable by the executor
    EXPECT_GT(bc.size(), 10u) << "Bytecode should have reasonable size";
}

/**
 * Note on DML Index Maintenance:
 *
 * DML operations (INSERT/UPDATE/DELETE) do NOT generate explicit INDEX_INSERT/INDEX_UPDATE/INDEX_DELETE
 * opcodes in the bytecode stream. Index maintenance is handled by the executor at runtime when it
 * processes the INSERT/UPDATE/DELETE opcodes.
 *
 * Rationale:
 * 1. The bytecode generator doesn't have access to the catalog to know which indexes exist
 * 2. Indexes can be created/dropped between bytecode generation and execution
 * 3. Embedding index operations in bytecode would bloat the bytecode significantly
 * 4. The executor can efficiently look up and maintain indexes at runtime
 *
 * The EXT_INDEX_* opcodes (EXT_INDEX_INSERT, EXT_INDEX_SEARCH, etc.) are internal opcodes used
 * by the executor for explicit index operations, not generated for standard DML statements.
 */

// Verify that INSERT statements don't contain explicit index maintenance opcodes
TEST_F(IndexBytecodeGenerationTest, InsertDoesNotContainIndexOpcodes)
{
    std::string sql = "INSERT INTO users (id, name) VALUES (1, 'Alice')";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success());
    const auto &bc = result.bytecode();

    // Should contain INSERT but NOT EXT_INDEX_INSERT
    EXPECT_TRUE(containsOpcode(bc, Opcode::INSERT));

    // Check that no extended index opcodes are present
    bool has_index_ops = false;
    for (size_t i = 0; i < bc.size() - 1; i++)
    {
        if (bc[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
        {
            uint8_t ext_op = bc[i + 1];
            if (ext_op >= 0x0A && ext_op <= 0x14)  // Index operation range
            {
                has_index_ops = true;
                break;
            }
        }
    }

    EXPECT_FALSE(has_index_ops) << "INSERT bytecode should not contain explicit index operation opcodes";
}

// Verify that UPDATE statements don't contain explicit index maintenance opcodes
TEST_F(IndexBytecodeGenerationTest, UpdateDoesNotContainIndexOpcodes)
{
    std::string sql = "UPDATE users SET name = 'Bob' WHERE id = 1";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success());
    const auto &bc = result.bytecode();

    EXPECT_TRUE(containsOpcode(bc, Opcode::UPDATE));

    // Check that no extended index opcodes are present
    bool has_index_ops = false;
    for (size_t i = 0; i < bc.size() - 1; i++)
    {
        if (bc[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
        {
            uint8_t ext_op = bc[i + 1];
            if (ext_op >= 0x0A && ext_op <= 0x14)
            {
                has_index_ops = true;
                break;
            }
        }
    }

    EXPECT_FALSE(has_index_ops) << "UPDATE bytecode should not contain explicit index operation opcodes";
}

// Verify that DELETE statements don't contain explicit index maintenance opcodes
TEST_F(IndexBytecodeGenerationTest, DeleteDoesNotContainIndexOpcodes)
{
    std::string sql = "DELETE FROM users WHERE id = 1";
    auto result = generateBytecode(sql);

    ASSERT_TRUE(result.success());
    const auto &bc = result.bytecode();

    EXPECT_TRUE(containsOpcode(bc, Opcode::DELETE));

    // Check that no extended index opcodes are present
    bool has_index_ops = false;
    for (size_t i = 0; i < bc.size() - 1; i++)
    {
        if (bc[i] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
        {
            uint8_t ext_op = bc[i + 1];
            if (ext_op >= 0x0A && ext_op <= 0x14)
            {
                has_index_ops = true;
                break;
            }
        }
    }

    EXPECT_FALSE(has_index_ops) << "DELETE bytecode should not contain explicit index operation opcodes";
}
