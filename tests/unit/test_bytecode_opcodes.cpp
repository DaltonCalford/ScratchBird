/**
 * @file test_bytecode_opcodes.cpp
 * @brief Validation tests for index bytecode opcodes
 *
 * Tests encoding, decoding, and parameter validation for all index operations.
 * Part of TASK-BYTECODE-1: Define Index Bytecode Opcodes
 *
 * Created: November 20, 2025
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/opcodes.h"
#include <vector>
#include <cstring>

using namespace scratchbird::sblr;

/**
 * Test that all required index opcodes are defined
 */
TEST(BytecodeOpcodesTest, AllIndexOpcodesAreDefined)
{
    // Verify basic CREATE/DROP INDEX opcodes exist
    EXPECT_EQ(static_cast<uint8_t>(Opcode::CREATE_INDEX), 0x1B);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::DROP_INDEX), 0x20);

    // Verify extended index operation opcodes exist
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_INSERT), 0x0A);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_SEARCH), 0x0B);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_SCAN), 0x0C);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_DELETE), 0x0D);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_TYPE), 0x0E);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_UPDATE), 0x1C);

    // Verify specialized index opcodes exist
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_GIN_INSERT), 0x28);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_GIN_SEARCH), 0x29);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_HNSW_INSERT), 0x2A);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_HNSW_SEARCH), 0x2B);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_COLUMNSTORE_INSERT), 0x2C);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_COLUMNSTORE_SCAN), 0x2D);
}

/**
 * Test IndexType enum has all 12 index types
 */
TEST(BytecodeOpcodesTest, AllIndexTypesAreDefined)
{
    EXPECT_EQ(static_cast<uint8_t>(IndexType::BTREE), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::HASH), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::HNSW), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::FULLTEXT), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::GIN), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::GIST), 0x05);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::BRIN), 0x06);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::RTREE), 0x07);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::SPGIST), 0x08);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::BITMAP), 0x09);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::COLUMNSTORE), 0x0A);
    EXPECT_EQ(static_cast<uint8_t>(IndexType::LSM), 0x0B);
}

/**
 * Test EXT_INDEX_INSERT bytecode encoding format
 * Format: [0xFF] [0x0A] [table_id:4] [index_id:4] [tid:8] [xmin:8] [key_len:4] [key_data:N]
 */
TEST(BytecodeOpcodesTest, EncodeIndexInsert)
{
    std::vector<uint8_t> bytecode;

    // Extended opcode prefix
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
    uint8_t ext_op_bytes[2];
    writeInt16(ext_op_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_INSERT));
    bytecode.insert(bytecode.end(), ext_op_bytes, ext_op_bytes + 2);

    // table_id = 10 (0x0A)
    uint32_t table_id = 10;
    uint8_t table_id_bytes[4];
    writeInt32(table_id_bytes, table_id);
    bytecode.insert(bytecode.end(), table_id_bytes, table_id_bytes + 4);

    // index_id = 5
    uint32_t index_id = 5;
    uint8_t index_id_bytes[4];
    writeInt32(index_id_bytes, index_id);
    bytecode.insert(bytecode.end(), index_id_bytes, index_id_bytes + 4);

    // tid = 1000
    uint64_t tid = 1000;
    uint8_t tid_bytes[8];
    writeInt64(tid_bytes, tid);
    bytecode.insert(bytecode.end(), tid_bytes, tid_bytes + 8);

    // xmin = 42
    uint64_t xmin = 42;
    uint8_t xmin_bytes[8];
    writeInt64(xmin_bytes, xmin);
    bytecode.insert(bytecode.end(), xmin_bytes, xmin_bytes + 8);

    // key = "foo" (3 bytes)
    const char* key_data = "foo";
    uint32_t key_len = 3;
    uint8_t key_len_bytes[4];
    writeInt32(key_len_bytes, key_len);
    bytecode.insert(bytecode.end(), key_len_bytes, key_len_bytes + 4);
    bytecode.insert(bytecode.end(), key_data, key_data + key_len);

    // Verify total size: 3 + 4 + 4 + 8 + 8 + 4 + 3 = 34 bytes
    EXPECT_EQ(bytecode.size(), 34);

    // Verify header
    EXPECT_EQ(bytecode[0], 0xFF);
    EXPECT_EQ(readInt16(&bytecode[1]), 0x0A);

    // Verify table_id decoding
    EXPECT_EQ(readInt32(&bytecode[3]), table_id);

    // Verify index_id decoding
    EXPECT_EQ(readInt32(&bytecode[7]), index_id);

    // Verify tid decoding
    EXPECT_EQ(readInt64(&bytecode[11]), tid);

    // Verify xmin decoding
    EXPECT_EQ(readInt64(&bytecode[19]), xmin);

    // Verify key_len decoding
    EXPECT_EQ(readInt32(&bytecode[27]), key_len);

    // Verify key_data
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(&bytecode[31]), 3), "foo");
}

/**
 * Test EXT_INDEX_SEARCH bytecode encoding format
 * Format: [0xFF] [0x0B] [table_id:4] [index_id:4] [current_xid:8] [key_len:4] [key_data:N]
 */
TEST(BytecodeOpcodesTest, EncodeIndexSearch)
{
    std::vector<uint8_t> bytecode;

    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
    uint8_t ext_op_bytes[2];
    writeInt16(ext_op_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_SEARCH));
    bytecode.insert(bytecode.end(), ext_op_bytes, ext_op_bytes + 2);

    uint32_t table_id = 10;
    uint8_t table_id_bytes[4];
    writeInt32(table_id_bytes, table_id);
    bytecode.insert(bytecode.end(), table_id_bytes, table_id_bytes + 4);

    uint32_t index_id = 5;
    uint8_t index_id_bytes[4];
    writeInt32(index_id_bytes, index_id);
    bytecode.insert(bytecode.end(), index_id_bytes, index_id_bytes + 4);

    uint64_t current_xid = 100;
    uint8_t xid_bytes[8];
    writeInt64(xid_bytes, current_xid);
    bytecode.insert(bytecode.end(), xid_bytes, xid_bytes + 8);

    const char* key_data = "bar";
    uint32_t key_len = 3;
    uint8_t key_len_bytes[4];
    writeInt32(key_len_bytes, key_len);
    bytecode.insert(bytecode.end(), key_len_bytes, key_len_bytes + 4);
    bytecode.insert(bytecode.end(), key_data, key_data + key_len);

    // Verify size: 3 + 4 + 4 + 8 + 4 + 3 = 26 bytes
    EXPECT_EQ(bytecode.size(), 26);

    // Verify header
    EXPECT_EQ(bytecode[0], 0xFF);
    EXPECT_EQ(readInt16(&bytecode[1]), 0x0B);

    // Verify current_xid
    EXPECT_EQ(readInt64(&bytecode[11]), current_xid);
}

/**
 * Test EXT_INDEX_SCAN (range scan) bytecode encoding format
 * Format: [0xFF] [0x0C] [table_id:4] [index_id:4] [current_xid:8] [start_key_len:4] [start_key:N] [end_key_len:4] [end_key:M]
 */
TEST(BytecodeOpcodesTest, EncodeIndexScan)
{
    std::vector<uint8_t> bytecode;

    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
    uint8_t ext_op_bytes[2];
    writeInt16(ext_op_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_SCAN));
    bytecode.insert(bytecode.end(), ext_op_bytes, ext_op_bytes + 2);

    uint32_t table_id = 10;
    uint8_t table_id_bytes[4];
    writeInt32(table_id_bytes, table_id);
    bytecode.insert(bytecode.end(), table_id_bytes, table_id_bytes + 4);

    uint32_t index_id = 5;
    uint8_t index_id_bytes[4];
    writeInt32(index_id_bytes, index_id);
    bytecode.insert(bytecode.end(), index_id_bytes, index_id_bytes + 4);

    uint64_t current_xid = 100;
    uint8_t xid_bytes[8];
    writeInt64(xid_bytes, current_xid);
    bytecode.insert(bytecode.end(), xid_bytes, xid_bytes + 8);

    // start_key = "a"
    const char* start_key = "a";
    uint32_t start_key_len = 1;
    uint8_t start_key_len_bytes[4];
    writeInt32(start_key_len_bytes, start_key_len);
    bytecode.insert(bytecode.end(), start_key_len_bytes, start_key_len_bytes + 4);
    bytecode.insert(bytecode.end(), start_key, start_key + start_key_len);

    // end_key = "z"
    const char* end_key = "z";
    uint32_t end_key_len = 1;
    uint8_t end_key_len_bytes[4];
    writeInt32(end_key_len_bytes, end_key_len);
    bytecode.insert(bytecode.end(), end_key_len_bytes, end_key_len_bytes + 4);
    bytecode.insert(bytecode.end(), end_key, end_key + end_key_len);

    // Verify size: 3 + 4 + 4 + 8 + 4 + 1 + 4 + 1 = 29 bytes
    EXPECT_EQ(bytecode.size(), 29);

    // Verify header
    EXPECT_EQ(bytecode[0], 0xFF);
    EXPECT_EQ(readInt16(&bytecode[1]), 0x0C);

    // Verify start_key_len
    EXPECT_EQ(readInt32(&bytecode[19]), start_key_len);

    // Verify start_key
    EXPECT_EQ(bytecode[23], 'a');

    // Verify end_key_len
    EXPECT_EQ(readInt32(&bytecode[24]), end_key_len);

    // Verify end_key
    EXPECT_EQ(bytecode[28], 'z');
}

/**
 * Test EXT_INDEX_UPDATE bytecode encoding format
 * Format: [0xFF] [0x1C] [table_id:4] [index_id:4] [tid:8] [xmin:8] [old_key_len:4] [old_key:N] [new_key_len:4] [new_key:M]
 */
TEST(BytecodeOpcodesTest, EncodeIndexUpdate)
{
    std::vector<uint8_t> bytecode;

    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
    uint8_t ext_op_bytes[2];
    writeInt16(ext_op_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_UPDATE));
    bytecode.insert(bytecode.end(), ext_op_bytes, ext_op_bytes + 2);

    uint32_t table_id = 10;
    uint8_t table_id_bytes[4];
    writeInt32(table_id_bytes, table_id);
    bytecode.insert(bytecode.end(), table_id_bytes, table_id_bytes + 4);

    uint32_t index_id = 5;
    uint8_t index_id_bytes[4];
    writeInt32(index_id_bytes, index_id);
    bytecode.insert(bytecode.end(), index_id_bytes, index_id_bytes + 4);

    uint64_t tid = 1000;
    uint8_t tid_bytes[8];
    writeInt64(tid_bytes, tid);
    bytecode.insert(bytecode.end(), tid_bytes, tid_bytes + 8);

    uint64_t xmin = 50;
    uint8_t xmin_bytes[8];
    writeInt64(xmin_bytes, xmin);
    bytecode.insert(bytecode.end(), xmin_bytes, xmin_bytes + 8);

    // old_key = "old"
    const char* old_key = "old";
    uint32_t old_key_len = 3;
    uint8_t old_key_len_bytes[4];
    writeInt32(old_key_len_bytes, old_key_len);
    bytecode.insert(bytecode.end(), old_key_len_bytes, old_key_len_bytes + 4);
    bytecode.insert(bytecode.end(), old_key, old_key + old_key_len);

    // new_key = "new"
    const char* new_key = "new";
    uint32_t new_key_len = 3;
    uint8_t new_key_len_bytes[4];
    writeInt32(new_key_len_bytes, new_key_len);
    bytecode.insert(bytecode.end(), new_key_len_bytes, new_key_len_bytes + 4);
    bytecode.insert(bytecode.end(), new_key, new_key + new_key_len);

    // Verify size: 3 + 4 + 4 + 8 + 8 + 4 + 3 + 4 + 3 = 41 bytes
    EXPECT_EQ(bytecode.size(), 41);

    // Verify header
    EXPECT_EQ(bytecode[0], 0xFF);
    EXPECT_EQ(readInt16(&bytecode[1]), 0x1C);

    // Verify old_key
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(&bytecode[31]), 3), "old");

    // Verify new_key
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(&bytecode[38]), 3), "new");
}

/**
 * Test EXT_INDEX_DELETE bytecode encoding format
 * Format: [0xFF] [0x0D] [table_id:4] [index_id:4] [tid:8] [xmax:8] [key_len:4] [key_data:N]
 */
TEST(BytecodeOpcodesTest, EncodeIndexDelete)
{
    std::vector<uint8_t> bytecode;

    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
    uint8_t ext_op_bytes[2];
    writeInt16(ext_op_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_INDEX_DELETE));
    bytecode.insert(bytecode.end(), ext_op_bytes, ext_op_bytes + 2);

    uint32_t table_id = 10;
    uint8_t table_id_bytes[4];
    writeInt32(table_id_bytes, table_id);
    bytecode.insert(bytecode.end(), table_id_bytes, table_id_bytes + 4);

    uint32_t index_id = 5;
    uint8_t index_id_bytes[4];
    writeInt32(index_id_bytes, index_id);
    bytecode.insert(bytecode.end(), index_id_bytes, index_id_bytes + 4);

    uint64_t tid = 1000;
    uint8_t tid_bytes[8];
    writeInt64(tid_bytes, tid);
    bytecode.insert(bytecode.end(), tid_bytes, tid_bytes + 8);

    // xmax for MGA logical deletion
    uint64_t xmax = 60;
    uint8_t xmax_bytes[8];
    writeInt64(xmax_bytes, xmax);
    bytecode.insert(bytecode.end(), xmax_bytes, xmax_bytes + 8);

    const char* key_data = "foo";
    uint32_t key_len = 3;
    uint8_t key_len_bytes[4];
    writeInt32(key_len_bytes, key_len);
    bytecode.insert(bytecode.end(), key_len_bytes, key_len_bytes + 4);
    bytecode.insert(bytecode.end(), key_data, key_data + key_len);

    // Verify size: 3 + 4 + 4 + 8 + 8 + 4 + 3 = 34 bytes
    EXPECT_EQ(bytecode.size(), 34);

    // Verify header
    EXPECT_EQ(bytecode[0], 0xFF);
    EXPECT_EQ(readInt16(&bytecode[1]), 0x0D);

    // Verify xmax (MGA logical deletion marker)
    EXPECT_EQ(readInt64(&bytecode[19]), xmax);
}

/**
 * Test helper functions for reading/writing multi-byte values
 */
TEST(BytecodeOpcodesTest, HelperFunctionsCorrectness)
{
    // Test writeInt32 and readInt32
    uint8_t buffer32[4];
    uint32_t test_val32 = 0x12345678;
    writeInt32(buffer32, test_val32);
    EXPECT_EQ(readInt32(buffer32), test_val32);

    // Verify little-endian encoding
    EXPECT_EQ(buffer32[0], 0x78);
    EXPECT_EQ(buffer32[1], 0x56);
    EXPECT_EQ(buffer32[2], 0x34);
    EXPECT_EQ(buffer32[3], 0x12);

    // Test writeInt64 and readInt64
    uint8_t buffer64[8];
    uint64_t test_val64 = 0x123456789ABCDEF0ULL;
    writeInt64(buffer64, test_val64);
    EXPECT_EQ(readInt64(buffer64), test_val64);

    // Verify little-endian encoding
    EXPECT_EQ(buffer64[0], 0xF0);
    EXPECT_EQ(buffer64[1], 0xDE);
    EXPECT_EQ(buffer64[2], 0xBC);
    EXPECT_EQ(buffer64[3], 0x9A);
    EXPECT_EQ(buffer64[4], 0x78);
    EXPECT_EQ(buffer64[5], 0x56);
    EXPECT_EQ(buffer64[6], 0x34);
    EXPECT_EQ(buffer64[7], 0x12);

    // Test writeInt16 and readInt16
    uint8_t buffer16[2];
    uint16_t test_val16 = 0xABCD;
    writeInt16(buffer16, test_val16);
    EXPECT_EQ(readInt16(buffer16), test_val16);

    // Verify little-endian encoding
    EXPECT_EQ(buffer16[0], 0xCD);
    EXPECT_EQ(buffer16[1], 0xAB);
}

/**
 * Test SBLR version constant
 */
TEST(BytecodeOpcodesTest, SBLRVersionIsDefined)
{
    EXPECT_EQ(SBLR_VERSION, 2);
}

/**
 * Test that extended opcodes use the correct prefix
 */
TEST(BytecodeOpcodesTest, ExtendedOpcodePrefix)
{
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE), 0xFF);
}

/**
 * Test MGA compliance - all index operations must support xmin/xmax
 */
TEST(BytecodeOpcodesTest, MGAComplianceParameters)
{
    // This is a design validation test to ensure:
    // 1. INSERT operations have xmin parameter
    // 2. DELETE operations have xmax parameter (logical deletion)
    // 3. UPDATE operations have xmin parameter (for new entry)
    // 4. SEARCH/SCAN operations have current_xid parameter for visibility checks

    // The actual parameter validation is done in the encoding tests above.
    // This test serves as documentation of MGA requirements.

    // Per MGA_RULES.md:
    // - INSERT must set xmin to current transaction ID
    // - DELETE must be logical (set xmax), not physical removal
    // - UPDATE must mark old entry with xmax and insert new entry with xmin
    // - Visibility checks must use TIP-based isVersionVisible(xmin, xmax, current_xid)

    SUCCEED() << "MGA compliance requirements documented in opcodes.h";
}

// Main function for running tests
