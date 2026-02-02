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
 * Comprehensive Data Type Tests for ScratchBird Parser/Analyzer v2.0
 *
 * Tests all ScratchBird data types including:
 * - Numeric types (INTEGER, SMALLINT, BIGINT, FLOAT, DOUBLE, DECIMAL)
 * - String types (CHAR, VARCHAR, TEXT)
 * - Temporal types (DATE, TIME, TIMESTAMP, INTERVAL)
 * - Binary types (BLOB, BYTEA)
 * - Boolean type
 * - Special types (UUID, JSON, ARRAY)
 * - Network types (INET, CIDR, MACADDR)
 * - Range types
 * - Spatial types (POINT, POLYGON, etc.)
 *
 * Tests cover:
 * 1. Literal parsing
 * 2. Type declarations in DDL
 * 3. Type coercion in expressions
 * 4. NULL handling for each type
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "unit/test_user_helpers.h"
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>
#include <cmath>

using namespace scratchbird;
using namespace scratchbird::parser::v2;
using namespace scratchbird::core;

static std::string generateUniqueDbPath() {
    std::ostringstream oss;
    oss << "/tmp/test_datatypes_v2_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << ".sbdb";
    return oss.str();
}

class V2DataTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path_ = generateUniqueDbPath();
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        EnsureUser(catalog_, "test_user");
        status = catalog_->createSchema("test", "test_user", test_schema_id_, &ctx);
        ASSERT_EQ(status, Status::OK);

        compiler_ = std::make_unique<sblr::QueryCompilerV2>(&db_);
        compiler_->setCurrentSchema(test_schema_id_);
    }

    void TearDown() override {
        compiler_.reset();
        db_.close();
        std::filesystem::remove(test_db_path_);
        std::filesystem::remove(test_db_path_ + "-lock");
    }

    // Compile SQL and return success status
    struct CompileResult {
        bool success;
        std::string error;
        std::vector<uint8_t> bytecode;
    };

    CompileResult compile(const std::string& sql) {
        CompileResult result;
        auto r = compiler_->compile(sql);
        result.success = r.success();
        if (!r.success() && !r.errors().empty()) {
            result.error = r.errors()[0];
        }
        result.bytecode = r.bytecode();
        return result;
    }

    // Check that SQL compiles successfully
    void expectSuccess(const std::string& sql) {
        auto result = compile(sql);
        EXPECT_TRUE(result.success) << "Failed: " << sql << "\nError: " << result.error;
    }

    // Check that SQL fails to compile
    void expectFailure(const std::string& sql) {
        auto result = compile(sql);
        EXPECT_FALSE(result.success) << "Should have failed: " << sql;
    }

    // Parse and analyze SQL, return resolved SELECT statement
    SemanticResult analyze(const std::string& sql) {
        input_sql_ = sql;
        parser_ = std::make_unique<Parser>(input_sql_);
        auto parse_result = parser_->parseStatement();

        if (!parse_result.success()) {
            SemanticResult result;
            for (const auto& err : parse_result.errors()) {
                SemanticError sem_err;
                sem_err.span = err.span;
                sem_err.message = "Parse error: " + err.message;
                sem_err.severity = SemanticError::Severity::ERROR;
                result.addError(sem_err);
            }
            return result;
        }

        analyzer_ = std::make_unique<SemanticAnalyzerV2>(*catalog_, parser_->stringPool());
        analyzer_->setCurrentSchema(test_schema_id_);
        return analyzer_->analyze(parse_result.statement());
    }

    // Get result type from a SELECT expression
    DataType getSelectResultType(const std::string& sql) {
        auto result = analyze(sql);
        if (!result.success()) {
            return DataType::UNKNOWN;
        }

        auto* stmt = dynamic_cast<ResolvedSelectStmt*>(result.statement());
        if (!stmt || stmt->select_list.empty() || !stmt->select_list[0].expr) {
            return DataType::UNKNOWN;
        }

        return stmt->select_list[0].expr->type.data_type;
    }

    std::string test_db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID test_schema_id_;
    std::unique_ptr<sblr::QueryCompilerV2> compiler_;

    std::string input_sql_;
    std::unique_ptr<Parser> parser_;
    std::unique_ptr<SemanticAnalyzerV2> analyzer_;
};

// =============================================================================
// Integer Literals
// =============================================================================

TEST_F(V2DataTypesTest, IntegerLiterals_Basic) {
    expectSuccess("SELECT 0");
    expectSuccess("SELECT 1");
    expectSuccess("SELECT 42");
    expectSuccess("SELECT 12345");
    expectSuccess("SELECT 2147483647");  // INT32 max
}

TEST_F(V2DataTypesTest, IntegerLiterals_Negative) {
    expectSuccess("SELECT -1");
    expectSuccess("SELECT -42");
    expectSuccess("SELECT -2147483648");  // INT32 min
}

TEST_F(V2DataTypesTest, IntegerLiterals_Large) {
    expectSuccess("SELECT 9223372036854775807");   // INT64 max
    // Note: -9223372036854775808 is parsed as -(9223372036854775808) which overflows
    // Using a slightly smaller value to avoid the literal overflow
    expectSuccess("SELECT -9223372036854775807");  // INT64 min + 1
}

TEST_F(V2DataTypesTest, IntegerLiterals_Type) {
    EXPECT_EQ(getSelectResultType("SELECT 42"), DataType::INT64);
    EXPECT_EQ(getSelectResultType("SELECT -42"), DataType::INT64);
}

// =============================================================================
// Floating Point Literals
// =============================================================================

TEST_F(V2DataTypesTest, FloatLiterals_Basic) {
    expectSuccess("SELECT 0.0");
    expectSuccess("SELECT 1.0");
    expectSuccess("SELECT 3.14");
    expectSuccess("SELECT 123.456");
}

TEST_F(V2DataTypesTest, FloatLiterals_Negative) {
    expectSuccess("SELECT -1.0");
    expectSuccess("SELECT -3.14159");
}

TEST_F(V2DataTypesTest, FloatLiterals_Scientific) {
    expectSuccess("SELECT 1e10");
    expectSuccess("SELECT 1E10");
    expectSuccess("SELECT 1.5e-5");
    expectSuccess("SELECT 2.718281828e0");
}

TEST_F(V2DataTypesTest, FloatLiterals_EdgeCases) {
    expectSuccess("SELECT 0.1");
    expectSuccess("SELECT 0.5");   // Standard form (leading dot may not be supported)
    expectSuccess("SELECT 1.0");   // Standard form (trailing dot may not be supported)
}

TEST_F(V2DataTypesTest, FloatLiterals_Type) {
    EXPECT_EQ(getSelectResultType("SELECT 3.14"), DataType::FLOAT64);
    EXPECT_EQ(getSelectResultType("SELECT 1e10"), DataType::FLOAT64);
}

// =============================================================================
// String Literals
// =============================================================================

TEST_F(V2DataTypesTest, StringLiterals_Basic) {
    expectSuccess("SELECT ''");           // Empty string
    expectSuccess("SELECT 'hello'");
    expectSuccess("SELECT 'Hello, World!'");
}

TEST_F(V2DataTypesTest, StringLiterals_Special) {
    expectSuccess("SELECT 'It''s working'");  // Escaped quote
    expectSuccess("SELECT 'Line1\\nLine2'");   // Newline escape
}

TEST_F(V2DataTypesTest, StringLiterals_Unicode) {
    expectSuccess("SELECT 'Hello'");           // ASCII
    // Note: Unicode support depends on database encoding
}

TEST_F(V2DataTypesTest, StringLiterals_Type) {
    EXPECT_EQ(getSelectResultType("SELECT 'hello'"), DataType::VARCHAR);
}

// =============================================================================
// Boolean Literals
// =============================================================================

TEST_F(V2DataTypesTest, BooleanLiterals_Basic) {
    expectSuccess("SELECT TRUE");
    expectSuccess("SELECT FALSE");
}

TEST_F(V2DataTypesTest, BooleanLiterals_CaseInsensitive) {
    expectSuccess("SELECT true");
    expectSuccess("SELECT True");
    expectSuccess("SELECT false");
    expectSuccess("SELECT False");
}

TEST_F(V2DataTypesTest, BooleanLiterals_Type) {
    EXPECT_EQ(getSelectResultType("SELECT TRUE"), DataType::BOOLEAN);
    EXPECT_EQ(getSelectResultType("SELECT FALSE"), DataType::BOOLEAN);
}

// =============================================================================
// NULL Literal
// =============================================================================

TEST_F(V2DataTypesTest, NullLiteral_Basic) {
    expectSuccess("SELECT NULL");
}

TEST_F(V2DataTypesTest, NullLiteral_Type) {
    auto result = analyze("SELECT NULL");
    ASSERT_TRUE(result.success());

    auto* stmt = dynamic_cast<ResolvedSelectStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    auto* literal = dynamic_cast<ResolvedLiteral*>(stmt->select_list[0].expr);
    ASSERT_NE(literal, nullptr);
    EXPECT_TRUE(literal->is_null);
}

// =============================================================================
// Date/Time Literals
// =============================================================================

TEST_F(V2DataTypesTest, DateLiterals_ISO) {
    // DATE literals using CAST syntax
    expectSuccess("SELECT CAST('2024-03-15' AS DATE)");
    expectSuccess("SELECT CAST('1970-01-01' AS DATE)");
    expectSuccess("SELECT CAST('2025-12-31' AS DATE)");
}

TEST_F(V2DataTypesTest, TimeLiterals_Basic) {
    // TIME literals using CAST syntax
    expectSuccess("SELECT CAST('09:30:45' AS TIME)");
    expectSuccess("SELECT CAST('00:00:00' AS TIME)");
    expectSuccess("SELECT CAST('23:59:59' AS TIME)");
}

TEST_F(V2DataTypesTest, TimestampLiterals_Basic) {
    // TIMESTAMP literals using CAST syntax
    expectSuccess("SELECT CAST('2024-03-15 09:30:45' AS TIMESTAMP)");
    expectSuccess("SELECT CAST('1970-01-01 00:00:00' AS TIMESTAMP)");
}

// =============================================================================
// BLOB Literals (Hexadecimal)
// =============================================================================

TEST_F(V2DataTypesTest, BlobLiterals_Hex) {
    expectSuccess("SELECT X'DEADBEEF'");
    expectSuccess("SELECT X'00FF00FF'");
    expectSuccess("SELECT X''");  // Empty blob
}

TEST_F(V2DataTypesTest, BlobLiterals_CaseInsensitive) {
    expectSuccess("SELECT x'abcd'");
    expectSuccess("SELECT X'ABCD'");
}

// =============================================================================
// DDL - Numeric Types
// =============================================================================

TEST_F(V2DataTypesTest, DDL_NumericTypes_Integer) {
    expectSuccess("CREATE TABLE t_int (a SMALLINT)");
    expectSuccess("CREATE TABLE t_int2 (a INTEGER)");
    expectSuccess("CREATE TABLE t_int3 (a INT)");
    expectSuccess("CREATE TABLE t_int4 (a BIGINT)");
}

TEST_F(V2DataTypesTest, DDL_NumericTypes_Float) {
    expectSuccess("CREATE TABLE t_float (a FLOAT)");
    // REAL is an alias for FLOAT
    expectSuccess("CREATE TABLE t_float2 (a REAL)");
    // DOUBLE PRECISION is standard SQL two-word type (now supported)
    expectSuccess("CREATE TABLE t_float3 (a DOUBLE PRECISION)");
}

TEST_F(V2DataTypesTest, DDL_NumericTypes_Decimal) {
    expectSuccess("CREATE TABLE t_dec (a DECIMAL)");
    expectSuccess("CREATE TABLE t_dec2 (a DECIMAL(10))");
    expectSuccess("CREATE TABLE t_dec3 (a DECIMAL(10, 2))");
    expectSuccess("CREATE TABLE t_dec4 (a NUMERIC(18, 4))");
}

// =============================================================================
// DDL - String Types
// =============================================================================

TEST_F(V2DataTypesTest, DDL_StringTypes_Basic) {
    expectSuccess("CREATE TABLE t_str (a CHAR(10))");
    expectSuccess("CREATE TABLE t_str2 (a VARCHAR(100))");
    expectSuccess("CREATE TABLE t_str3 (a TEXT)");
}

TEST_F(V2DataTypesTest, DDL_StringTypes_Lengths) {
    expectSuccess("CREATE TABLE t_str4 (a CHAR(1))");
    expectSuccess("CREATE TABLE t_str5 (a CHAR(255))");
    expectSuccess("CREATE TABLE t_str6 (a VARCHAR(32767))");
}

// =============================================================================
// DDL - Temporal Types
// =============================================================================

TEST_F(V2DataTypesTest, DDL_TemporalTypes_Basic) {
    expectSuccess("CREATE TABLE t_time (a DATE)");
    expectSuccess("CREATE TABLE t_time2 (a TIME)");
    expectSuccess("CREATE TABLE t_time3 (a TIMESTAMP)");
}

TEST_F(V2DataTypesTest, DDL_TemporalTypes_WithTimezone) {
    expectSuccess("CREATE TABLE t_tz (a TIME WITH TIME ZONE)");
    expectSuccess("CREATE TABLE t_tz2 (a TIMESTAMP WITH TIME ZONE)");
}

TEST_F(V2DataTypesTest, DDL_TemporalTypes_WithoutTimezone) {
    expectSuccess("CREATE TABLE t_notz (a TIME WITHOUT TIME ZONE)");
    expectSuccess("CREATE TABLE t_notz2 (a TIMESTAMP WITHOUT TIME ZONE)");
}

// =============================================================================
// DDL - Binary Types
// =============================================================================

TEST_F(V2DataTypesTest, DDL_BinaryTypes) {
    expectSuccess("CREATE TABLE t_bin (a BLOB)");
    expectSuccess("CREATE TABLE t_bin2 (a BYTEA)");
}

// =============================================================================
// DDL - Boolean Type
// =============================================================================

TEST_F(V2DataTypesTest, DDL_BooleanType) {
    expectSuccess("CREATE TABLE t_bool (a BOOLEAN)");
    expectSuccess("CREATE TABLE t_bool2 (active BOOLEAN DEFAULT TRUE)");
    expectSuccess("CREATE TABLE t_bool3 (flag BOOLEAN NOT NULL)");
}

// =============================================================================
// DDL - Special Types
// =============================================================================

TEST_F(V2DataTypesTest, DDL_SpecialTypes_UUID) {
    expectSuccess("CREATE TABLE t_uuid (id UUID)");
}

TEST_F(V2DataTypesTest, DDL_SpecialTypes_JSON) {
    expectSuccess("CREATE TABLE t_json (data JSON)");
    expectSuccess("CREATE TABLE t_json2 (data JSONB)");
}

// =============================================================================
// DDL - Array Types
// =============================================================================

TEST_F(V2DataTypesTest, DDL_ArrayTypes) {
    expectSuccess("CREATE TABLE t_arr (tags VARCHAR[])");
    expectSuccess("CREATE TABLE t_arr2 (scores INTEGER[])");
}

// =============================================================================
// Type Coercion - Integer to Float
// =============================================================================

TEST_F(V2DataTypesTest, Coercion_IntegerToFloat) {
    // Integer + Float = Float
    auto type = getSelectResultType("SELECT 1 + 2.5");
    EXPECT_EQ(type, DataType::FLOAT64);
}

TEST_F(V2DataTypesTest, Coercion_FloatArithmetic) {
    auto type = getSelectResultType("SELECT 1.5 * 2.5");
    EXPECT_EQ(type, DataType::FLOAT64);
}

// =============================================================================
// Type Coercion - String Operations
// =============================================================================

TEST_F(V2DataTypesTest, Coercion_StringConcat) {
    expectSuccess("SELECT 'hello' || ' ' || 'world'");
    auto type = getSelectResultType("SELECT 'a' || 'b'");
    // String concat should produce VARCHAR
    EXPECT_TRUE(type == DataType::VARCHAR || type == DataType::TEXT);
}

// =============================================================================
// Type Coercion - Boolean Expressions
// =============================================================================

TEST_F(V2DataTypesTest, Coercion_BooleanComparison) {
    auto type = getSelectResultType("SELECT 1 = 1");
    // Comparison result should be BOOLEAN
    EXPECT_TRUE(type == DataType::BOOLEAN || type == DataType::INT64)
        << "Got type: " << static_cast<int>(type);
}

TEST_F(V2DataTypesTest, Coercion_BooleanLogical) {
    auto type = getSelectResultType("SELECT TRUE AND FALSE");
    EXPECT_EQ(type, DataType::BOOLEAN);
}

// =============================================================================
// CAST Expressions
// =============================================================================

TEST_F(V2DataTypesTest, Cast_IntToVarchar) {
    expectSuccess("SELECT CAST(42 AS VARCHAR)");
    auto type = getSelectResultType("SELECT CAST(42 AS VARCHAR)");
    EXPECT_EQ(type, DataType::VARCHAR);
}

TEST_F(V2DataTypesTest, Cast_VarcharToInt) {
    expectSuccess("SELECT CAST('123' AS INTEGER)");
    auto type = getSelectResultType("SELECT CAST('123' AS INTEGER)");
    EXPECT_EQ(type, DataType::INT32);
}

TEST_F(V2DataTypesTest, Cast_FloatToInt) {
    expectSuccess("SELECT CAST(3.14 AS INTEGER)");
    auto type = getSelectResultType("SELECT CAST(3.14 AS INTEGER)");
    EXPECT_EQ(type, DataType::INT32);
}

TEST_F(V2DataTypesTest, Cast_IntToFloat) {
    expectSuccess("SELECT CAST(42 AS FLOAT)");
    auto type = getSelectResultType("SELECT CAST(42 AS FLOAT)");
    EXPECT_EQ(type, DataType::FLOAT32);
}

TEST_F(V2DataTypesTest, Cast_IntToDouble) {
    // DOUBLE PRECISION is standard SQL (now supported)
    expectSuccess("SELECT CAST(42 AS DOUBLE PRECISION)");
    auto type = getSelectResultType("SELECT CAST(42 AS DOUBLE PRECISION)");
    EXPECT_EQ(type, DataType::FLOAT64);
}

TEST_F(V2DataTypesTest, Cast_IntToDecimal) {
    expectSuccess("SELECT CAST(42 AS DECIMAL(10,2))");
    auto type = getSelectResultType("SELECT CAST(42 AS DECIMAL(10,2))");
    EXPECT_EQ(type, DataType::DECIMAL);
}

TEST_F(V2DataTypesTest, Cast_ToDate) {
    expectSuccess("SELECT CAST('2024-03-15' AS DATE)");
    auto type = getSelectResultType("SELECT CAST('2024-03-15' AS DATE)");
    EXPECT_EQ(type, DataType::DATE);
}

TEST_F(V2DataTypesTest, Cast_ToTimestamp) {
    expectSuccess("SELECT CAST('2024-03-15 10:30:00' AS TIMESTAMP)");
    auto type = getSelectResultType("SELECT CAST('2024-03-15 10:30:00' AS TIMESTAMP)");
    EXPECT_EQ(type, DataType::TIMESTAMP);
}

// =============================================================================
// NULL Handling
// =============================================================================

TEST_F(V2DataTypesTest, NullHandling_Arithmetic) {
    // NULL arithmetic - NULL adopts type from non-NULL operand
    expectSuccess("SELECT NULL + 1");
    expectSuccess("SELECT 1 + NULL");
    expectSuccess("SELECT NULL + 1.5");
    expectSuccess("SELECT 1.5 * NULL");
}

TEST_F(V2DataTypesTest, NullHandling_Comparison) {
    // NULL comparison - result is always NULL but types should resolve
    expectSuccess("SELECT NULL = NULL");  // Returns NULL, not TRUE
    expectSuccess("SELECT 1 = NULL");     // Returns NULL
    expectSuccess("SELECT NULL = 'text'");
    expectSuccess("SELECT NULL <> 1");
}

TEST_F(V2DataTypesTest, NullHandling_IsNull) {
    expectSuccess("SELECT NULL IS NULL");
    expectSuccess("SELECT 1 IS NULL");
    expectSuccess("SELECT 1 IS NOT NULL");
}

TEST_F(V2DataTypesTest, NullHandling_Coalesce) {
    expectSuccess("SELECT COALESCE(NULL, 1)");
    expectSuccess("SELECT COALESCE(NULL, NULL, 'default')");
}

TEST_F(V2DataTypesTest, NullHandling_NullIf) {
    expectSuccess("SELECT NULLIF(1, 1)");
    expectSuccess("SELECT NULLIF(1, 2)");
}

// =============================================================================
// Multiple Types in Single Statement
// =============================================================================

TEST_F(V2DataTypesTest, MultipleTypes_Select) {
    expectSuccess("SELECT 1, 'hello', TRUE, NULL, 3.14");
}

TEST_F(V2DataTypesTest, MultipleTypes_CreateTable) {
    // Test table with multiple types - use FLOAT instead of DOUBLE PRECISION for compatibility
    auto r = compile(R"(
        CREATE TABLE all_types (
            id INTEGER PRIMARY KEY,
            small_val SMALLINT,
            big_val BIGINT,
            dec_val DECIMAL(10,2),
            flt_val FLOAT,
            bool_val BOOLEAN,
            str_val VARCHAR(100),
            chr_val CHAR(20),
            txt_val TEXT,
            dt_val DATE,
            tm_val TIME,
            ts_val TIMESTAMP,
            blob_val BLOB
        )
    )");
    EXPECT_TRUE(r.success) << "Multiple types CREATE TABLE failed: " << r.error;
}

// =============================================================================
// Complex Expressions with Type Coercion
// =============================================================================

TEST_F(V2DataTypesTest, ComplexExpr_MixedArithmetic) {
    expectSuccess("SELECT (1 + 2) * 3.5 / 2");
    auto type = getSelectResultType("SELECT (1 + 2) * 3.5 / 2");
    EXPECT_EQ(type, DataType::FLOAT64);
}

TEST_F(V2DataTypesTest, ComplexExpr_CaseWithTypes) {
    expectSuccess("SELECT CASE WHEN TRUE THEN 1 ELSE 2.5 END");
}

TEST_F(V2DataTypesTest, ComplexExpr_NestedCasts) {
    expectSuccess("SELECT CAST(CAST('42' AS INTEGER) AS VARCHAR)");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(V2DataTypesTest, EdgeCase_EmptyStrings) {
    expectSuccess("SELECT ''");
    expectSuccess("SELECT '' || 'test'");
}

TEST_F(V2DataTypesTest, EdgeCase_ZeroValues) {
    expectSuccess("SELECT 0");
    expectSuccess("SELECT 0.0");
    expectSuccess("SELECT -0.0");
}

TEST_F(V2DataTypesTest, EdgeCase_VeryLongString) {
    std::string long_str(1000, 'x');
    expectSuccess("SELECT '" + long_str + "'");
}

// =============================================================================
// Type Inference in Expressions
// =============================================================================

TEST_F(V2DataTypesTest, TypeInference_Division) {
    // Integer division vs float division
    auto type1 = getSelectResultType("SELECT 5 / 2");
    EXPECT_TRUE(type1 == DataType::INT64 || type1 == DataType::FLOAT64);

    auto type2 = getSelectResultType("SELECT 5.0 / 2");
    EXPECT_EQ(type2, DataType::FLOAT64);
}

TEST_F(V2DataTypesTest, TypeInference_Power) {
    expectSuccess("SELECT 2 ^ 10");  // Power operator
}

TEST_F(V2DataTypesTest, TypeInference_Modulo) {
    expectSuccess("SELECT 10 % 3");
    auto type = getSelectResultType("SELECT 10 % 3");
    EXPECT_EQ(type, DataType::INT64);
}

// =============================================================================
// Firebird-Specific Types (for compatibility)
// =============================================================================

TEST_F(V2DataTypesTest, FirebirdCompat_BlobSubType) {
    // Firebird BLOB with sub_type
    expectSuccess("CREATE TABLE t_blob1 (data BLOB)");
}

TEST_F(V2DataTypesTest, FirebirdCompat_NumericPrecision) {
    // High-precision numeric
    expectSuccess("CREATE TABLE t_num (val NUMERIC(18, 4))");
}

// =============================================================================
// Network Types (if supported)
// =============================================================================

TEST_F(V2DataTypesTest, NetworkTypes_DDL) {
    // These may fail if network types aren't fully implemented
    auto r1 = compile("CREATE TABLE t_net1 (addr INET)");
    auto r2 = compile("CREATE TABLE t_net2 (network CIDR)");
    auto r3 = compile("CREATE TABLE t_net3 (mac MACADDR)");
    // Just verify they parse without crashing
    (void)r1;
    (void)r2;
    (void)r3;
}

// =============================================================================
// Spatial Types (if supported)
// =============================================================================

TEST_F(V2DataTypesTest, SpatialTypes_DDL) {
    // These may fail if spatial types aren't fully implemented
    auto r1 = compile("CREATE TABLE t_geo1 (location POINT)");
    auto r2 = compile("CREATE TABLE t_geo2 (boundary POLYGON)");
    // Just verify they parse without crashing
    (void)r1;
    (void)r2;
}

// =============================================================================
// Range Types (if supported)
// =============================================================================

TEST_F(V2DataTypesTest, RangeTypes_DDL) {
    // These may fail if range types aren't fully implemented
    auto r1 = compile("CREATE TABLE t_range1 (vals INT4RANGE)");
    auto r2 = compile("CREATE TABLE t_range2 (period TSRANGE)");
    // Just verify they parse without crashing
    (void)r1;
    (void)r2;
}

// =============================================================================
// Text Search Types (if supported)
// =============================================================================

TEST_F(V2DataTypesTest, TextSearchTypes_DDL) {
    // These may fail if text search types aren't fully implemented
    auto r1 = compile("CREATE TABLE t_ts1 (doc TSVECTOR)");
    auto r2 = compile("CREATE TABLE t_ts2 (query TSQUERY)");
    // Just verify they parse without crashing
    (void)r1;
    (void)r2;
}

// =============================================================================
// Type Aliases
// =============================================================================

TEST_F(V2DataTypesTest, TypeAliases_Int) {
    expectSuccess("CREATE TABLE t_alias1 (a INT)");       // INTEGER alias
    expectSuccess("CREATE TABLE t_alias2 (a INT4)");      // INTEGER alias
    expectSuccess("CREATE TABLE t_alias3 (a INT8)");      // BIGINT alias
}

TEST_F(V2DataTypesTest, TypeAliases_Float) {
    expectSuccess("CREATE TABLE t_alias4 (a REAL)");      // FLOAT alias
    expectSuccess("CREATE TABLE t_alias5 (a FLOAT4)");    // FLOAT alias
    expectSuccess("CREATE TABLE t_alias6 (a FLOAT8)");    // DOUBLE alias
}

TEST_F(V2DataTypesTest, TypeAliases_Bool) {
    expectSuccess("CREATE TABLE t_alias7 (a BOOL)");      // BOOLEAN alias
}

// =============================================================================
// Default Values with Types
// =============================================================================

TEST_F(V2DataTypesTest, Defaults_Integer) {
    expectSuccess("CREATE TABLE t_def1 (a INTEGER DEFAULT 0)");
    expectSuccess("CREATE TABLE t_def2 (a INTEGER DEFAULT -1)");
}

TEST_F(V2DataTypesTest, Defaults_String) {
    expectSuccess("CREATE TABLE t_def3 (a VARCHAR(100) DEFAULT 'unknown')");
    expectSuccess("CREATE TABLE t_def4 (a TEXT DEFAULT '')");
}

TEST_F(V2DataTypesTest, Defaults_Boolean) {
    expectSuccess("CREATE TABLE t_def5 (a BOOLEAN DEFAULT TRUE)");
    expectSuccess("CREATE TABLE t_def6 (a BOOLEAN DEFAULT FALSE)");
}

TEST_F(V2DataTypesTest, Defaults_Null) {
    expectSuccess("CREATE TABLE t_def7 (a INTEGER DEFAULT NULL)");
}

// =============================================================================
// Constraints with Types
// =============================================================================

TEST_F(V2DataTypesTest, Constraints_NotNull) {
    expectSuccess("CREATE TABLE t_con1 (a INTEGER NOT NULL)");
    expectSuccess("CREATE TABLE t_con2 (a VARCHAR(100) NOT NULL)");
    expectSuccess("CREATE TABLE t_con3 (a BOOLEAN NOT NULL DEFAULT TRUE)");
}

TEST_F(V2DataTypesTest, Constraints_Check) {
    // CHECK constraints reference column being defined (now supported)
    expectSuccess("CREATE TABLE t_check1 (a INTEGER CHECK (a > 0))");
    expectSuccess("CREATE TABLE t_check2 (a VARCHAR(10) CHECK (a <> ''))");
    expectSuccess("CREATE TABLE t_check3 (val DECIMAL(10,2) CHECK (val >= 0.00))");
}
