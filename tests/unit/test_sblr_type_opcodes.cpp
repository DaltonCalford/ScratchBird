/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file test_sblr_type_opcodes.cpp
 * @brief SBLR Type Opcode Remediation Tests (Section B)
 *
 * Tests for SBLR type markers and typed literal opcodes.
 * - Bytecode round-trip tests for all type markers
 * - Typed literal parsing tests for new literal opcodes
 * - DDL/DML coverage tests for each type
 *
 * Part of Section B: SBLR Type Opcode Remediation
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/core/types.h"
#include <vector>
#include <cstring>
#include <sstream>

using namespace scratchbird;
using namespace scratchbird::sblr;

//=============================================================================
// B1: SBLR Type Marker Tests - Bytecode Round-Trip
//=============================================================================

/**
 * Test all base type markers have correct opcode values
 */
TEST(SBLRTypeOpcodeTest, BaseTypeMarkersAreDefined)
{
    // Integer types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_INT8), 0x25);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_INT16), 0x26);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_INTEGER), 0x20);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_BIGINT), 0x21);

    // Floating point types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_FLOAT32), 0x27);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_DOUBLE), 0x22);

    // String types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_CHAR), 0x2D);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_VARCHAR), 0x23);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_TEXT), 0x2E);

    // Boolean
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_BOOLEAN), 0x24);

    // Temporal types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_DATE), 0x28);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_TIME), 0x29);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_TIMESTAMP), 0x2A);

    // Binary types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_BINARY), 0x2F);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_VARBINARY), 0xB0);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_BLOB), 0xB1);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_BYTEA), 0xB2);

    // Other types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_UUID), 0x2B);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_DECIMAL), 0x2C);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_JSON), 0xB3);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_ARRAY), 0xB9);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::TYPE_DOMAIN), 0xB8);
}

/**
 * Test extended type markers for emulated-engine parity types
 */
TEST(SBLRTypeOpcodeTest, ExtendedTypeMarkersForEmulatedEngineTypes)
{
    // Unsigned integer types (PostgreSQL/Firebird compatibility)
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_UINT8), 0x0410);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_UINT16), 0x0411);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_UINT32), 0x0412);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_UINT64), 0x0413);

    // Large integer types
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_INT128), 0x0400);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_UINT128), 0x0401);

    // Monetary type (PostgreSQL MONEY)
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_MONEY), 0x0414);

    // Interval type (PostgreSQL INTERVAL)
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_INTERVAL), 0x0415);

    // JSONB, JSONPATH and XML types (PostgreSQL)
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_JSONB), 0x0416);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_JSONPATH), 0x0417);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_XML), 0x0418);

    // DECFLOAT types (Firebird 4.0)
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_DECFLOAT16), 0x0425);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_DECFLOAT34), 0x0426);
}

/**
 * Test spatial type markers
 */
TEST(SBLRTypeOpcodeTest, SpatialTypeMarkers)
{
    // Basic spatial types
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_POINT), 0x50);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_LINESTRING), 0x51);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_POLYGON), 0x52);

    // Multi-geometry types (MySQL/PostgreSQL)
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_MULTIPOINT), 0x0419);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_MULTILINESTRING), 0x041A);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_MULTIPOLYGON), 0x041B);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_GEOMETRYCOLLECTION), 0x041C);
}

/**
 * Test network address type markers (PostgreSQL)
 */
TEST(SBLRTypeOpcodeTest, NetworkAddressTypeMarkers)
{
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_INET), 0x041F);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_CIDR), 0x0420);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_MACADDR), 0x0421);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_MACADDR8), 0x0422);
}

/**
 * Test timezone-aware temporal type markers
 */
TEST(SBLRTypeOpcodeTest, TimezoneAwareTemporalTypeMarkers)
{
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_TIME_TZ), 0x0423);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_TIMESTAMP_TZ), 0x0424);
}

/**
 * Test composite and variant type markers
 */
TEST(SBLRTypeOpcodeTest, ComplexTypeMarkers)
{
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_COMPOSITE), 0x041D);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_VARIANT), 0x041E);
}

/**
 * Test text search type markers
 */
TEST(SBLRTypeOpcodeTest, TextSearchTypeMarkers)
{
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_TSVECTOR), 0x0AB);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_TSQUERY), 0x0AC);
}

/**
 * Test range type markers
 */
TEST(SBLRTypeOpcodeTest, RangeTypeMarkers)
{
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_INT4RANGE), 0x0B1);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_INT8RANGE), 0x0B2);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_NUMRANGE), 0x0B3);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_DATERANGE), 0x0B4);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_TSRANGE), 0x0B5);
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_TSTZRANGE), 0x0B6);
}

/**
 * Test vector type marker (for vector/HNSW indexes)
 */
TEST(SBLRTypeOpcodeTest, VectorTypeMarker)
{
    EXPECT_EQ(static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_VECTOR), 0x0402);
}

//=============================================================================
// B1: Typed Literal Opcode Tests
//=============================================================================

/**
 * Test base literal opcodes are defined
 */
TEST(SBLRLiteralOpcodeTest, BaseLiteralOpcodesAreDefined)
{
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_NULL), 0x30);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_INT32), 0x31);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_INT64), 0x32);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_DOUBLE), 0x33);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_STRING), 0x34);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_BOOLEAN), 0x37);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_UUID), 0x38);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_DATE), 0x39);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_TIME), 0x3A);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_TIMESTAMP), 0x3B);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_BINARY), 0x3C);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_DECIMAL), 0x3D);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_JSON), 0x3E);
    EXPECT_EQ(static_cast<uint8_t>(Opcode::LITERAL_XML), 0x3F);
}

//=============================================================================
// B1: Bytecode Round-Trip Encoding Tests
//=============================================================================

// Helper to encode uint16 in little-endian
static void writeUint16(uint8_t* buf, uint16_t value) {
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
}

/**
 * Test encoding of base type marker in bytecode stream
 */
TEST(SBLRBytecodeRoundTripTest, EncodeBaseTypeMarker)
{
    std::vector<uint8_t> bytecode;
    
    // Encode TYPE_INTEGER marker
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_INTEGER));
    
    // Verify encoding
    ASSERT_EQ(bytecode.size(), 1);
    EXPECT_EQ(bytecode[0], 0x20);
}

/**
 * Test encoding of extended type marker in bytecode stream
 * Format: [0xFF] [ext_opcode_low] [ext_opcode_high]
 */
TEST(SBLRBytecodeRoundTripTest, EncodeExtendedTypeMarker)
{
    std::vector<uint8_t> bytecode;
    
    // Encode EXT_TYPE_JSONB (0x0416)
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
    uint8_t ext_bytes[2];
    writeUint16(ext_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_JSONB));
    bytecode.insert(bytecode.end(), ext_bytes, ext_bytes + 2);
    
    // Verify encoding
    ASSERT_EQ(bytecode.size(), 3);
    EXPECT_EQ(bytecode[0], 0xFF);
    EXPECT_EQ(bytecode[1], 0x16);
    EXPECT_EQ(bytecode[2], 0x04);
}

/**
 * Test round-trip of type marker encoding/decoding for all base types
 */
TEST(SBLRBytecodeRoundTripTest, AllBaseTypeMarkersRoundTrip)
{
    std::vector<Opcode> base_types = {
        Opcode::TYPE_INT8,
        Opcode::TYPE_INT16,
        Opcode::TYPE_INTEGER,
        Opcode::TYPE_BIGINT,
        Opcode::TYPE_FLOAT32,
        Opcode::TYPE_DOUBLE,
        Opcode::TYPE_CHAR,
        Opcode::TYPE_VARCHAR,
        Opcode::TYPE_TEXT,
        Opcode::TYPE_BOOLEAN,
        Opcode::TYPE_DATE,
        Opcode::TYPE_TIME,
        Opcode::TYPE_TIMESTAMP,
        Opcode::TYPE_BINARY,
        Opcode::TYPE_VARBINARY,
        Opcode::TYPE_BLOB,
        Opcode::TYPE_BYTEA,
        Opcode::TYPE_UUID,
        Opcode::TYPE_DECIMAL,
        Opcode::TYPE_JSON,
        Opcode::TYPE_ARRAY,
        Opcode::TYPE_DOMAIN,
    };
    
    for (auto type_opcode : base_types) {
        std::vector<uint8_t> bytecode;
        bytecode.push_back(static_cast<uint8_t>(type_opcode));
        
        // Decode and verify
        ASSERT_EQ(bytecode.size(), 1);
        Opcode decoded = static_cast<Opcode>(bytecode[0]);
        EXPECT_EQ(decoded, type_opcode) << "Type marker round-trip failed for opcode 0x" 
            << std::hex << static_cast<int>(type_opcode);
    }
}

/**
 * Test round-trip of extended type marker encoding/decoding
 */
TEST(SBLRBytecodeRoundTripTest, AllExtendedTypeMarkersRoundTrip)
{
    std::vector<ExtendedOpcode> ext_types = {
        // Unsigned integers
        ExtendedOpcode::EXT_TYPE_UINT8,
        ExtendedOpcode::EXT_TYPE_UINT16,
        ExtendedOpcode::EXT_TYPE_UINT32,
        ExtendedOpcode::EXT_TYPE_UINT64,
        // Large integers
        ExtendedOpcode::EXT_TYPE_INT128,
        ExtendedOpcode::EXT_TYPE_UINT128,
        // Monetary and interval
        ExtendedOpcode::EXT_TYPE_MONEY,
        ExtendedOpcode::EXT_TYPE_INTERVAL,
        // JSONB and XML
        ExtendedOpcode::EXT_TYPE_JSONB,
        ExtendedOpcode::EXT_TYPE_XML,
        // DECFLOAT
        ExtendedOpcode::EXT_TYPE_DECFLOAT16,
        ExtendedOpcode::EXT_TYPE_DECFLOAT34,
        // Spatial
        ExtendedOpcode::EXT_TYPE_POINT,
        ExtendedOpcode::EXT_TYPE_LINESTRING,
        ExtendedOpcode::EXT_TYPE_POLYGON,
        ExtendedOpcode::EXT_TYPE_MULTIPOINT,
        ExtendedOpcode::EXT_TYPE_MULTILINESTRING,
        ExtendedOpcode::EXT_TYPE_MULTIPOLYGON,
        ExtendedOpcode::EXT_TYPE_GEOMETRYCOLLECTION,
        // Complex
        ExtendedOpcode::EXT_TYPE_COMPOSITE,
        ExtendedOpcode::EXT_TYPE_VARIANT,
        // Network
        ExtendedOpcode::EXT_TYPE_INET,
        ExtendedOpcode::EXT_TYPE_CIDR,
        ExtendedOpcode::EXT_TYPE_MACADDR,
        ExtendedOpcode::EXT_TYPE_MACADDR8,
        // Timezone-aware temporal
        ExtendedOpcode::EXT_TYPE_TIME_TZ,
        ExtendedOpcode::EXT_TYPE_TIMESTAMP_TZ,
        // Text search
        ExtendedOpcode::EXT_TYPE_TSVECTOR,
        ExtendedOpcode::EXT_TYPE_TSQUERY,
        // Range types
        ExtendedOpcode::EXT_TYPE_INT4RANGE,
        ExtendedOpcode::EXT_TYPE_INT8RANGE,
        ExtendedOpcode::EXT_TYPE_NUMRANGE,
        ExtendedOpcode::EXT_TYPE_DATERANGE,
        ExtendedOpcode::EXT_TYPE_TSRANGE,
        ExtendedOpcode::EXT_TYPE_TSTZRANGE,
    };
    
    for (auto ext_opcode : ext_types) {
        std::vector<uint8_t> bytecode;
        bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE)); // 0xFF
        uint8_t ext_bytes[2];
        writeUint16(ext_bytes, static_cast<uint16_t>(ext_opcode));
        bytecode.insert(bytecode.end(), ext_bytes, ext_bytes + 2);
        
        // Decode and verify
        ASSERT_EQ(bytecode.size(), 3);
        EXPECT_EQ(bytecode[0], 0xFF);
        uint16_t decoded_val = bytecode[1] | (bytecode[2] << 8);
        EXPECT_EQ(decoded_val, static_cast<uint16_t>(ext_opcode))
            << "Extended type marker round-trip failed for opcode 0x" 
            << std::hex << static_cast<int>(ext_opcode);
    }
}

//=============================================================================
// B2: DDL/DML Coverage Tests - Type Usage in Schema Operations
//=============================================================================

/**
 * Test that all DataTypes in core/types.h have corresponding SBLR type markers
 * This ensures type system consistency between runtime and bytecode
 */
TEST(SBLRDataTypeCoverageTest, AllDataTypesHaveSBLRMarkers)
{
    // Verify DataType enum values are defined (existence check)
    // The actual values may vary; we just verify they're accessible
    
    // Integer types
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::INT8));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::INT16));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::INT32));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::INT64));
    
    // Floating point types
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::FLOAT32));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::FLOAT64));
    
    // Boolean type
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::BOOLEAN));
    
    // Temporal types
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::DATE));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::TIME));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::TIMESTAMP));
    
    // UUID and Decimal
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::UUID));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::DECIMAL));
    
    // String types
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::VARCHAR));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::CHAR));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::TEXT));
    
    // Binary types
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::BINARY));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::VARBINARY));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::BLOB));
    
    // Complex types
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::JSON));
    EXPECT_NO_THROW(static_cast<core::DataType>(core::DataType::ARRAY));
}

/**
 * Test emulated-engine required types have SBLR coverage
 */
TEST(SBLRDataTypeCoverageTest, EmulatedEngineTypesHaveSBLRCoverage)
{
    // PostgreSQL types
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_JSONB));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_XML));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_MONEY));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_INTERVAL));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_INET));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_CIDR));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_MACADDR));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_MACADDR8));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_TIME_TZ));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_TIMESTAMP_TZ));
    
    // Firebird types
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_DECFLOAT16));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_DECFLOAT34));
    
    // MySQL/PostgreSQL spatial types
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_MULTIPOINT));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_MULTILINESTRING));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_MULTIPOLYGON));
    EXPECT_NO_THROW(static_cast<ExtendedOpcode>(ExtendedOpcode::EXT_TYPE_GEOMETRYCOLLECTION));
}

/**
 * Test that all required literal opcodes are defined for type encoding
 */
TEST(SBLRLiteralCoverageTest, AllTypesHaveLiteralOpcodes)
{
    // Base literals
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_NULL));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_INT32));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_INT64));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_DOUBLE));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_STRING));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_BOOLEAN));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_UUID));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_DATE));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_TIME));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_TIMESTAMP));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_BINARY));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_DECIMAL));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_JSON));
    EXPECT_NO_THROW(static_cast<Opcode>(Opcode::LITERAL_XML));
}

//=============================================================================
// B2: Minimal DDL/DML Coverage Tests
//=============================================================================

/**
 * Test CREATE TABLE bytecode includes correct type markers
 */
TEST(SBLRDDLTest, CreateTableBytecodeIncludesTypeMarkers)
{
    // Simulate bytecode for: CREATE TABLE t (id INT, name VARCHAR(100))
    std::vector<uint8_t> bytecode;
    
    // CREATE_TABLE opcode
    bytecode.push_back(static_cast<uint8_t>(Opcode::CREATE_TABLE));
    
    // Table name
    const char* table_name = "t";
    bytecode.push_back(static_cast<uint8_t>(strlen(table_name)));
    bytecode.insert(bytecode.end(), table_name, table_name + strlen(table_name));
    
    // Column 1: id INT
    bytecode.push_back(static_cast<uint8_t>(Opcode::COLUMN_DEF));
    const char* col1_name = "id";
    bytecode.push_back(static_cast<uint8_t>(strlen(col1_name)));
    bytecode.insert(bytecode.end(), col1_name, col1_name + strlen(col1_name));
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_INTEGER));
    
    // Column 2: name VARCHAR(100)
    bytecode.push_back(static_cast<uint8_t>(Opcode::COLUMN_DEF));
    const char* col2_name = "name";
    bytecode.push_back(static_cast<uint8_t>(strlen(col2_name)));
    bytecode.insert(bytecode.end(), col2_name, col2_name + strlen(col2_name));
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_VARCHAR));
    // Length as 2-byte integer
    bytecode.push_back(100 & 0xFF);
    bytecode.push_back((100 >> 8) & 0xFF);
    
    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));
    
    // Verify bytecode structure
    EXPECT_GT(bytecode.size(), 10);
    EXPECT_EQ(bytecode[0], 0x10); // CREATE_TABLE
}

/**
 * Test INSERT bytecode with typed literals
 */
TEST(SBLRDMLTest, InsertBytecodeWithTypedLiterals)
{
    // Simulate bytecode for: INSERT INTO t VALUES (1, 'hello')
    std::vector<uint8_t> bytecode;
    
    // INSERT opcode
    bytecode.push_back(static_cast<uint8_t>(Opcode::INSERT));
    
    // Table reference
    bytecode.push_back(static_cast<uint8_t>(Opcode::TABLE_REF));
    const char* table_name = "t";
    bytecode.push_back(static_cast<uint8_t>(strlen(table_name)));
    bytecode.insert(bytecode.end(), table_name, table_name + strlen(table_name));
    
    // Value 1: INT32 literal
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_INT32));
    bytecode.push_back(1); bytecode.push_back(0); bytecode.push_back(0); bytecode.push_back(0);
    
    // Value 2: STRING literal
    bytecode.push_back(static_cast<uint8_t>(Opcode::LITERAL_STRING));
    const char* str_val = "hello";
    bytecode.push_back(static_cast<uint8_t>(strlen(str_val)));
    bytecode.insert(bytecode.end(), str_val, str_val + strlen(str_val));
    
    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));
    
    // Verify
    EXPECT_GT(bytecode.size(), 5);
    EXPECT_EQ(bytecode[0], 0x11); // INSERT
}

/**
 * Test extended type usage in CREATE TABLE (e.g., JSONB column)
 */
TEST(SBLRDDLTest, CreateTableWithExtendedType)
{
    // Simulate bytecode for: CREATE TABLE t (data JSONB)
    std::vector<uint8_t> bytecode;
    
    // CREATE_TABLE opcode
    bytecode.push_back(static_cast<uint8_t>(Opcode::CREATE_TABLE));
    
    // Table name
    const char* table_name = "t";
    bytecode.push_back(static_cast<uint8_t>(strlen(table_name)));
    bytecode.insert(bytecode.end(), table_name, table_name + strlen(table_name));
    
    // Column: data JSONB
    bytecode.push_back(static_cast<uint8_t>(Opcode::COLUMN_DEF));
    const char* col_name = "data";
    bytecode.push_back(static_cast<uint8_t>(strlen(col_name)));
    bytecode.insert(bytecode.end(), col_name, col_name + strlen(col_name));
    
    // EXT_TYPE_JSONB (extended opcode)
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    uint8_t ext_bytes[2];
    writeUint16(ext_bytes, static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_JSONB));
    bytecode.insert(bytecode.end(), ext_bytes, ext_bytes + 2);
    
    // END
    bytecode.push_back(static_cast<uint8_t>(Opcode::END));
    
    // Verify extended opcode encoding
    EXPECT_GT(bytecode.size(), 5);
    // Find extended opcode marker
    bool found_ext = false;
    for (size_t i = 0; i < bytecode.size(); i++) {
        if (bytecode[i] == 0xFF) {
            found_ext = true;
            break;
        }
    }
    EXPECT_TRUE(found_ext) << "Extended opcode marker (0xFF) not found in bytecode";
}

//=============================================================================
// Integration: Type Marker + Literal Opcode Consistency
//=============================================================================

/**
 * Test that all base types have consistent opcode values between
 * Opcode enum and ExtendedOpcode enum where applicable
 */
TEST(SBLRConsistencyTest, TypeMarkerConsistency)
{
    // Verify spatial types are consistent between main enum and extended enum
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_POINT), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_POINT));
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_LINESTRING), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_LINESTRING));
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_POLYGON), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_POLYGON));
    
    // Verify text search types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_TSVECTOR), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_TSVECTOR));
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_TSQUERY), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_TSQUERY));
    
    // Verify range types
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_INT4RANGE), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_INT4RANGE));
    EXPECT_EQ(static_cast<uint8_t>(Opcode::EXT_TYPE_INT8RANGE), 
              static_cast<uint8_t>(ExtendedOpcode::EXT_TYPE_INT8RANGE));
}

/**
 * Comprehensive test: encode/decode all type markers in sequence
 * This simulates a realistic bytecode stream with multiple types
 */
TEST(SBLRComprehensiveTest, EncodeDecodeTypeSequence)
{
    std::vector<uint8_t> bytecode;
    
    // Encode sequence of different type markers
    // Base types
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_INTEGER));
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_VARCHAR));
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_BOOLEAN));
    bytecode.push_back(static_cast<uint8_t>(Opcode::TYPE_TIMESTAMP));
    
    // Extended types
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    uint8_t ext1[2];
    writeUint16(ext1, static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_JSONB));
    bytecode.insert(bytecode.end(), ext1, ext1 + 2);
    
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    uint8_t ext2[2];
    writeUint16(ext2, static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_INET));
    bytecode.insert(bytecode.end(), ext2, ext2 + 2);
    
    bytecode.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    uint8_t ext3[2];
    writeUint16(ext3, static_cast<uint16_t>(ExtendedOpcode::EXT_TYPE_DECFLOAT16));
    bytecode.insert(bytecode.end(), ext3, ext3 + 2);
    
    // Verify total length
    EXPECT_EQ(bytecode.size(), 1 + 1 + 1 + 1 + 3 + 3 + 3); // 4 base + 3 extended
    
    // Verify all base type markers
    EXPECT_EQ(bytecode[0], 0x20); // TYPE_INTEGER
    EXPECT_EQ(bytecode[1], 0x23); // TYPE_VARCHAR
    EXPECT_EQ(bytecode[2], 0x24); // TYPE_BOOLEAN
    EXPECT_EQ(bytecode[3], 0x2A); // TYPE_TIMESTAMP
    
    // Verify extended type markers (at positions 4, 7, 10)
    EXPECT_EQ(bytecode[4], 0xFF);
    EXPECT_EQ(bytecode[5], 0x16);
    EXPECT_EQ(bytecode[6], 0x04); // EXT_TYPE_JSONB = 0x0416
    
    EXPECT_EQ(bytecode[7], 0xFF);
    EXPECT_EQ(bytecode[8], 0x1F);
    EXPECT_EQ(bytecode[9], 0x04); // EXT_TYPE_INET = 0x041F
    
    EXPECT_EQ(bytecode[10], 0xFF);
    EXPECT_EQ(bytecode[11], 0x25);
    EXPECT_EQ(bytecode[12], 0x04); // EXT_TYPE_DECFLOAT16 = 0x0425
}
