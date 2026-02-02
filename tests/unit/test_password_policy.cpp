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
 * Unit Tests for Password Policy Enforcement
 *
 * P0-1: Password Policy Enforcement (Security Phase 3.5)
 * Tests password validation against security policies (CWE-521)
 */

#include <gtest/gtest.h>
#include "scratchbird/core/password_policy.h"

using namespace scratchbird::core;

/**
 * Test Fixture for Password Policy
 */
class PasswordPolicyTest : public ::testing::Test {
protected:
    PasswordPolicy default_policy_;
    ErrorContext ctx_;

    void SetUp() override {
        // Use default policy for most tests
        default_policy_ = PasswordPolicy();
    }

    void TearDown() override {
        // Clear error context
        ctx_ = ErrorContext();
    }
};

// ===== Length Requirements Tests =====

TEST_F(PasswordPolicyTest, MinimumLengthEnforced) {
    PasswordPolicy policy;
    policy.min_length = 8;

    // Too short
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("short", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("at least 8 characters"));

    // Exactly minimum
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("GoodPass1!", policy, &ctx_));

    // Longer than minimum
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("VeryLongPassword123!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, MaximumLengthEnforced) {
    PasswordPolicy policy;
    policy.max_length = 20;
    policy.require_uppercase = false;
    policy.require_lowercase = false;
    policy.require_digit = false;
    policy.require_symbol = false;
    policy.check_common_passwords = false;

    // Too long (21 chars)
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("012345678901234567890", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("at most 20 characters"));

    // Exactly maximum (20 chars)
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("01234567890123456789", policy, &ctx_));

    // Shorter than maximum
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("short", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, BCryptMaximumLength) {
    PasswordPolicy policy;
    policy.max_length = 72;  // BCrypt limit
    policy.require_uppercase = false;
    policy.require_lowercase = false;
    policy.require_digit = false;
    policy.require_symbol = false;
    policy.check_common_passwords = false;

    // 73 characters - exceeds BCrypt limit
    std::string too_long(73, 'a');
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy(too_long, policy, &ctx_));

    // 72 characters - exactly at BCrypt limit
    std::string exactly_max(72, 'a');
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy(exactly_max, policy, &ctx_));
}

// ===== Complexity Requirements Tests =====

TEST_F(PasswordPolicyTest, UppercaseRequirement) {
    PasswordPolicy policy = default_policy_;

    // No uppercase
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("alllowercase123!", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("uppercase letter"));

    // Has uppercase
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("HasUpperCase123!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, LowercaseRequirement) {
    PasswordPolicy policy = default_policy_;

    // No lowercase
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("ALLUPPERCASE123!", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("lowercase letter"));

    // Has lowercase
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("HasLowerCase123!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, DigitRequirement) {
    PasswordPolicy policy = default_policy_;

    // No digits
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("NoDigitsHere!", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("digit"));

    // Has digit
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("HasDigit1!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, SymbolRequirement) {
    PasswordPolicy policy = default_policy_;

    // No symbols
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("NoSymbols123", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("symbol"));

    // Has symbol
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("HasSymbol123!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, AllComplexityRequirementsMet) {
    PasswordPolicy policy = default_policy_;

    // Valid password with all requirements
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("ValidPass123!", policy, &ctx_));

    // Multiple symbols
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("C0mpl3x!@#P@ss", policy, &ctx_));

    // Unicode symbols
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("Passw0rd™", policy, &ctx_));
}

// ===== Relaxed Policy Tests =====

TEST_F(PasswordPolicyTest, RelaxedPolicyNoComplexity) {
    PasswordPolicy policy;
    policy.min_length = 8;
    policy.require_uppercase = false;
    policy.require_lowercase = false;
    policy.require_digit = false;
    policy.require_symbol = false;
    policy.check_common_passwords = false;

    // All lowercase, no complexity
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("alllowercase", policy, &ctx_));

    // All uppercase, no complexity
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("ALLUPPERCASE", policy, &ctx_));

    // All digits
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("12345678", policy, &ctx_));
}

// ===== Common Password Tests =====

TEST_F(PasswordPolicyTest, CommonPasswordRejection) {
    PasswordPolicy policy = default_policy_;

    // Common passwords (case-insensitive)
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Password123!", policy, &ctx_));
    EXPECT_NE(std::string::npos, ctx_.error_message.find("too common"));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("PASSWORD123!", policy, &ctx_));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Admin123!", policy, &ctx_));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Qwerty123!", policy, &ctx_));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Welcome123!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, CommonPasswordVariations) {
    PasswordPolicy policy = default_policy_;

    // These should still be rejected (case-insensitive)
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("PaSsWoRd123!", policy, &ctx_));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("password1!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, StrongPasswordAccepted) {
    PasswordPolicy policy = default_policy_;

    // Strong, unique passwords
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("MyStr0ng!P@ss", policy, &ctx_));

    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("Tr0ub4dor&3", policy, &ctx_));

    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("C0rrect!H0rse", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, CommonPasswordCheckDisabled) {
    PasswordPolicy policy = default_policy_;
    policy.check_common_passwords = false;

    // Common password allowed when check is disabled
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("Password123!", policy, &ctx_));
}

// ===== Common Password Dictionary Tests =====

TEST_F(PasswordPolicyTest, CommonPasswordDictionaryLoaded) {
    // Load dictionary
    Status status = loadCommonPasswordDictionary(&ctx_);
    EXPECT_EQ(Status::OK, status);

    // Should be idempotent (safe to call multiple times)
    status = loadCommonPasswordDictionary(&ctx_);
    EXPECT_EQ(Status::OK, status);
}

TEST_F(PasswordPolicyTest, CommonPasswordFunction) {
    // Known common passwords
    EXPECT_TRUE(isCommonPassword("password"));
    EXPECT_TRUE(isCommonPassword("123456"));
    EXPECT_TRUE(isCommonPassword("qwerty"));
    EXPECT_TRUE(isCommonPassword("admin"));

    // Case-insensitive
    EXPECT_TRUE(isCommonPassword("PASSWORD"));
    EXPECT_TRUE(isCommonPassword("PaSsWoRd"));

    // Unique passwords
    EXPECT_FALSE(isCommonPassword("MyUniqueP@ssw0rd123"));
    EXPECT_FALSE(isCommonPassword("Tr0ub4dor&3"));
}

// ===== Edge Cases Tests =====

TEST_F(PasswordPolicyTest, EmptyPassword) {
    PasswordPolicy policy = default_policy_;

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, WhitespaceOnlyPassword) {
    PasswordPolicy policy = default_policy_;

    // Spaces are symbols, but no letters or digits
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("        ", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, UnicodeCharacters) {
    PasswordPolicy policy = default_policy_;

    // Unicode characters count toward length and complexity
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("Pássw0rd™", policy, &ctx_));

    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("密码Pass1!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, NullByteInPassword) {
    PasswordPolicy policy = default_policy_;

    // std::string can contain null bytes
    std::string password_with_null = "Pass\0word123!";
    // Note: This tests implementation behavior; null bytes may be problematic
    // Current implementation should handle this safely
}

// ===== Minimum Length Edge Cases =====

TEST_F(PasswordPolicyTest, MinimumLengthZero) {
    PasswordPolicy policy;
    policy.min_length = 0;
    policy.require_uppercase = false;
    policy.require_lowercase = false;
    policy.require_digit = false;
    policy.require_symbol = false;
    policy.check_common_passwords = false;

    // Empty password allowed if min_length is 0
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, MinimumLengthOne) {
    PasswordPolicy policy;
    policy.min_length = 1;
    policy.require_uppercase = false;
    policy.require_lowercase = false;
    policy.require_digit = false;
    policy.require_symbol = false;
    policy.check_common_passwords = false;

    // Single character
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("a", policy, &ctx_));
}

// ===== Integration Test: Realistic Password Scenarios =====

TEST_F(PasswordPolicyTest, RealisticWeakPasswords) {
    PasswordPolicy policy = default_policy_;

    // Weak patterns that should be rejected
    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Password1!", policy, &ctx_));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Admin123!", policy, &ctx_));

    EXPECT_EQ(Status::INVALID_ARGUMENT,
              validatePasswordPolicy("Qwerty123!", policy, &ctx_));
}

TEST_F(PasswordPolicyTest, RealisticStrongPasswords) {
    PasswordPolicy policy = default_policy_;

    // Strong passwords that should be accepted
    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("Xk9#mPq2$vLn", policy, &ctx_));

    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("Tr0ub4dor&3", policy, &ctx_));

    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("C0rrect!H0rse", policy, &ctx_));

    EXPECT_EQ(Status::OK,
              validatePasswordPolicy("MyS3cur3!Pass", policy, &ctx_));
}
