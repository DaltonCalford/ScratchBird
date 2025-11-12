// Test Mathematical Functions - ALPHA Phase A
// Tests all 29 mathematical functions implemented for ScratchBird

#include <gtest/gtest.h>
#include <scratchbird/core/database.h>
#include <scratchbird/parser/parser.h>
#include <scratchbird/sblr/bytecode_generator.h>
#include <scratchbird/sblr/executor.h>
#include <cmath>

using namespace scratchbird;

class MathematicalFunctionsTest : public ::testing::Test
{
protected:
    std::unique_ptr<core::Database> db;

    void SetUp() override
    {
        // Create test database
        core::DatabaseOptions options;
        options.create_if_missing = true;
        options.error_if_exists = false;

        core::ErrorContext ctx;
        db = core::Database::create("test_math_functions.db", options, &ctx);
        ASSERT_NE(db, nullptr) << "Failed to create database: " << ctx.message;
    }

    void TearDown() override
    {
        db.reset();
        // Clean up test database
        std::remove("test_math_functions.db");
    }

    // Helper to execute a simple SELECT statement and get the result
    double executeScalar(const std::string& sql)
    {
        parser::Parser parser(sql);
        auto ast = parser.parse();
        EXPECT_TRUE(ast != nullptr) << "Parse failed";
        if (!ast) return 0.0;

        sblr::BytecodeGenerator generator;
        auto bytecode_result = generator.generate(ast.get());
        EXPECT_TRUE(bytecode_result.success()) << "Bytecode generation failed: " << bytecode_result.error();
        if (!bytecode_result.success()) return 0.0;

        sblr::Executor executor(db.get());
        auto exec_result = executor.execute(bytecode_result.bytecode());
        EXPECT_TRUE(exec_result.success()) << "Execution failed: " << exec_result.error();
        if (!exec_result.success()) return 0.0;

        auto result_set = exec_result.resultSet();
        EXPECT_NE(result_set, nullptr);
        if (!result_set) return 0.0;

        EXPECT_EQ(result_set->rowCount(), 1) << "Expected 1 row";
        EXPECT_EQ(result_set->columnCount(), 1) << "Expected 1 column";
        if (result_set->rowCount() != 1 || result_set->columnCount() != 1) return 0.0;

        return result_set->getValue(0, 0).toDouble();
    }
};

// ===== Trigonometric Functions =====

TEST_F(MathematicalFunctionsTest, SIN_Function)
{
    double result = executeScalar("SELECT SIN(0)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT SIN(3.14159265358979323846 / 2)");  // π/2
    EXPECT_NEAR(result, 1.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, COS_Function)
{
    double result = executeScalar("SELECT COS(0)");
    EXPECT_NEAR(result, 1.0, 1e-10);

    result = executeScalar("SELECT COS(3.14159265358979323846)");  // π
    EXPECT_NEAR(result, -1.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, TAN_Function)
{
    double result = executeScalar("SELECT TAN(0)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT TAN(0.785398163397448)");  // π/4
    EXPECT_NEAR(result, 1.0, 1e-6);
}

TEST_F(MathematicalFunctionsTest, ASIN_Function)
{
    double result = executeScalar("SELECT ASIN(0)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT ASIN(1)");
    EXPECT_NEAR(result, M_PI / 2.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, ACOS_Function)
{
    double result = executeScalar("SELECT ACOS(1)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT ACOS(0)");
    EXPECT_NEAR(result, M_PI / 2.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, ATAN_Function)
{
    double result = executeScalar("SELECT ATAN(0)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT ATAN(1)");
    EXPECT_NEAR(result, M_PI / 4.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, ATAN2_Function)
{
    double result = executeScalar("SELECT ATAN2(0, 1)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT ATAN2(1, 1)");
    EXPECT_NEAR(result, M_PI / 4.0, 1e-10);
}

// ===== Angle Conversion Functions =====

TEST_F(MathematicalFunctionsTest, DEGREES_Function)
{
    double result = executeScalar("SELECT DEGREES(0)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT DEGREES(3.14159265358979323846)");  // π
    EXPECT_NEAR(result, 180.0, 1e-6);
}

TEST_F(MathematicalFunctionsTest, RADIANS_Function)
{
    double result = executeScalar("SELECT RADIANS(0)");
    EXPECT_NEAR(result, 0.0, 1e-10);

    result = executeScalar("SELECT RADIANS(180)");
    EXPECT_NEAR(result, M_PI, 1e-10);
}

TEST_F(MathematicalFunctionsTest, PI_Function)
{
    double result = executeScalar("SELECT PI()");
    EXPECT_NEAR(result, M_PI, 1e-15);
}

// ===== Algebraic Functions =====

TEST_F(MathematicalFunctionsTest, ABS_Function)
{
    double result = executeScalar("SELECT ABS(-5)");
    EXPECT_EQ(result, 5.0);

    result = executeScalar("SELECT ABS(3.5)");
    EXPECT_EQ(result, 3.5);
}

TEST_F(MathematicalFunctionsTest, SIGN_Function)
{
    double result = executeScalar("SELECT SIGN(-5)");
    EXPECT_EQ(result, -1.0);

    result = executeScalar("SELECT SIGN(0)");
    EXPECT_EQ(result, 0.0);

    result = executeScalar("SELECT SIGN(3.5)");
    EXPECT_EQ(result, 1.0);
}

TEST_F(MathematicalFunctionsTest, ROUND_Function)
{
    double result = executeScalar("SELECT ROUND(3.14159)");
    EXPECT_EQ(result, 3.0);

    result = executeScalar("SELECT ROUND(3.14159, 2)");
    EXPECT_NEAR(result, 3.14, 1e-10);
}

TEST_F(MathematicalFunctionsTest, CEIL_Function)
{
    double result = executeScalar("SELECT CEIL(3.2)");
    EXPECT_EQ(result, 4.0);

    result = executeScalar("SELECT CEILING(-3.2)");
    EXPECT_EQ(result, -3.0);
}

TEST_F(MathematicalFunctionsTest, FLOOR_Function)
{
    double result = executeScalar("SELECT FLOOR(3.8)");
    EXPECT_EQ(result, 3.0);

    result = executeScalar("SELECT FLOOR(-3.2)");
    EXPECT_EQ(result, -4.0);
}

TEST_F(MathematicalFunctionsTest, TRUNC_Function)
{
    double result = executeScalar("SELECT TRUNC(3.8)");
    EXPECT_EQ(result, 3.0);

    result = executeScalar("SELECT TRUNC(-3.8)");
    EXPECT_EQ(result, -3.0);
}

TEST_F(MathematicalFunctionsTest, MOD_Function)
{
    double result = executeScalar("SELECT MOD(10, 3)");
    EXPECT_EQ(result, 1.0);

    result = executeScalar("SELECT MOD(10.5, 3.0)");
    EXPECT_NEAR(result, 1.5, 1e-10);
}

TEST_F(MathematicalFunctionsTest, SQRT_Function)
{
    double result = executeScalar("SELECT SQRT(4)");
    EXPECT_EQ(result, 2.0);

    result = executeScalar("SELECT SQRT(2)");
    EXPECT_NEAR(result, 1.41421356237, 1e-10);
}

TEST_F(MathematicalFunctionsTest, CBRT_Function)
{
    double result = executeScalar("SELECT CBRT(8)");
    EXPECT_NEAR(result, 2.0, 1e-10);

    result = executeScalar("SELECT CBRT(27)");
    EXPECT_NEAR(result, 3.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, POWER_Function)
{
    double result = executeScalar("SELECT POWER(2, 3)");
    EXPECT_EQ(result, 8.0);

    result = executeScalar("SELECT POW(10, 2)");
    EXPECT_EQ(result, 100.0);
}

TEST_F(MathematicalFunctionsTest, EXP_Function)
{
    double result = executeScalar("SELECT EXP(0)");
    EXPECT_EQ(result, 1.0);

    result = executeScalar("SELECT EXP(1)");
    EXPECT_NEAR(result, M_E, 1e-10);
}

// ===== Logarithmic Functions =====

TEST_F(MathematicalFunctionsTest, LN_Function)
{
    double result = executeScalar("SELECT LN(1)");
    EXPECT_EQ(result, 0.0);

    result = executeScalar("SELECT LN(2.718281828459045)");  // e
    EXPECT_NEAR(result, 1.0, 1e-10);
}

TEST_F(MathematicalFunctionsTest, LOG_Function)
{
    double result = executeScalar("SELECT LOG(100)");
    EXPECT_EQ(result, 2.0);

    result = executeScalar("SELECT LOG(2, 8)");
    EXPECT_EQ(result, 3.0);
}

TEST_F(MathematicalFunctionsTest, LOG10_Function)
{
    double result = executeScalar("SELECT LOG10(100)");
    EXPECT_EQ(result, 2.0);

    result = executeScalar("SELECT LOG10(1000)");
    EXPECT_EQ(result, 3.0);
}

TEST_F(MathematicalFunctionsTest, LOG2_Function)
{
    double result = executeScalar("SELECT LOG2(8)");
    EXPECT_EQ(result, 3.0);

    result = executeScalar("SELECT LOG2(1024)");
    EXPECT_EQ(result, 10.0);
}

// ===== Combined Function Test =====

TEST_F(MathematicalFunctionsTest, ComplexExpression)
{
    // Test: SQRT(POWER(3, 2) + POWER(4, 2)) should equal 5 (Pythagorean theorem)
    double result = executeScalar("SELECT SQRT(POWER(3, 2) + POWER(4, 2))");
    EXPECT_NEAR(result, 5.0, 1e-10);
}
