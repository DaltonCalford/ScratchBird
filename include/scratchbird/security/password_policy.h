/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-1: Password Expiration & Policy
//
// Implements password rotation policies and expiration checking.
// Features:
// - Configurable password expiration (default: 90 days)
// - Password history to prevent reuse
// - Complexity requirements (optional)
// - Grace period for expired passwords
// - Force password change on next login
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <cstdint>
#include <optional>

namespace scratchbird::security {

// ============================================================================
// Password Policy Configuration
// ============================================================================

struct PasswordPolicy {
    // Expiration settings
    uint32_t expiration_days = 90;            // Days until password expires (0 = never)
    uint32_t grace_period_days = 7;           // Days after expiration before lockout
    uint32_t warning_days = 14;               // Days before expiration to warn user

    // History settings
    uint32_t history_count = 5;               // Number of old passwords to remember
    bool prevent_reuse = true;                // Prevent reuse of old passwords

    // Complexity requirements (optional)
    bool enforce_complexity = false;
    uint32_t min_length = 8;                  // Minimum password length
    uint32_t min_uppercase = 0;               // Minimum uppercase characters
    uint32_t min_lowercase = 0;               // Minimum lowercase characters
    uint32_t min_digits = 0;                  // Minimum numeric characters
    uint32_t min_special = 0;                 // Minimum special characters

    // Account lockout
    uint32_t max_failed_attempts = 5;         // Lock after N failed attempts
    uint32_t lockout_duration_minutes = 30;   // Minutes to lock account

    // Default policy
    static PasswordPolicy defaultPolicy() {
        return PasswordPolicy{};
    }

    // Strict security policy
    static PasswordPolicy strictPolicy() {
        PasswordPolicy p;
        p.expiration_days = 60;
        p.history_count = 10;
        p.enforce_complexity = true;
        p.min_length = 12;
        p.min_uppercase = 1;
        p.min_lowercase = 1;
        p.min_digits = 1;
        p.min_special = 1;
        p.max_failed_attempts = 3;
        return p;
    }
};

// ============================================================================
// Password State for a User
// ============================================================================

struct PasswordState {
    std::chrono::system_clock::time_point created_at;      // When password was set
    std::chrono::system_clock::time_point expires_at;      // When password expires
    std::chrono::system_clock::time_point last_changed;    // Last password change
    std::vector<std::string> password_history;             // Hashes of previous passwords
    uint32_t failed_attempts = 0;                          // Consecutive failed logins
    std::chrono::system_clock::time_point locked_until;    // Account locked until
    bool force_change = false;                             // Must change on next login
    bool never_expires = false;                            // Override expiration for service accounts

    // Check if password is expired
    bool isExpired() const {
        if (never_expires) return false;
        return std::chrono::system_clock::now() >= expires_at;
    }

    // Check if in grace period
    bool isInGracePeriod(uint32_t grace_days) const {
        if (never_expires) return false;
        auto grace_end = expires_at + std::chrono::hours(24 * grace_days);
        auto now = std::chrono::system_clock::now();
        return now >= expires_at && now < grace_end;
    }

    // Check if warning should be shown
    bool shouldWarn(uint32_t warning_days) const {
        if (never_expires) return false;
        auto warning_start = expires_at - std::chrono::hours(24 * warning_days);
        auto now = std::chrono::system_clock::now();
        return now >= warning_start && now < expires_at;
    }

    // Check if account is locked
    bool isLocked() const {
        return std::chrono::system_clock::now() < locked_until;
    }

    // Days until expiration (negative if expired)
    int32_t daysUntilExpiration() const {
        if (never_expires) return INT32_MAX;
        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::hours>(expires_at - now);
        return static_cast<int32_t>(diff.count() / 24);
    }
};

// ============================================================================
// Password Validation Result
// ============================================================================

enum class PasswordValidationError : uint8_t {
    NONE = 0,
    TOO_SHORT,
    MISSING_UPPERCASE,
    MISSING_LOWERCASE,
    MISSING_DIGIT,
    MISSING_SPECIAL,
    REUSED_PASSWORD,
    COMMON_PASSWORD,
    CONTAINS_USERNAME,
};

struct PasswordValidationResult {
    bool valid = true;
    std::vector<PasswordValidationError> errors;
    std::string message;

    operator bool() const { return valid; }

    static PasswordValidationResult success() {
        return PasswordValidationResult{true, {}, "Password meets policy requirements"};
    }

    static PasswordValidationResult failure(PasswordValidationError error, std::string_view msg) {
        return PasswordValidationResult{false, {error}, std::string(msg)};
    }

    void addError(PasswordValidationError error, std::string_view msg) {
        valid = false;
        errors.push_back(error);
        if (!message.empty()) message += "; ";
        message += msg;
    }
};

// ============================================================================
// Password Policy Manager
// ============================================================================

class PasswordPolicyManager {
public:
    static PasswordPolicyManager& getInstance();

    // Set global password policy
    void setPolicy(const PasswordPolicy& policy);

    // Get current policy
    const PasswordPolicy& getPolicy() const { return policy_; }

    // ========================================================================
    // Password Validation
    // ========================================================================

    // Validate a new password against policy
    PasswordValidationResult validatePassword(
        std::string_view password,
        std::string_view username,
        const std::vector<std::string>& password_history = {}) const;

    // Check password complexity
    bool meetsComplexityRequirements(std::string_view password) const;

    // Check if password is in history
    bool isPasswordReused(
        std::string_view password_hash,
        const std::vector<std::string>& history) const;

    // ========================================================================
    // Password State Management
    // ========================================================================

    // Create new password state when password is set
    PasswordState createPasswordState(bool never_expires = false) const;

    // Update password state on password change
    PasswordState updatePasswordState(
        const PasswordState& current,
        std::string_view old_password_hash) const;

    // Calculate expiration date from now
    std::chrono::system_clock::time_point calculateExpirationDate() const;

    // ========================================================================
    // Login Handling
    // ========================================================================

    // Check login result - returns status code
    enum class LoginCheckResult : uint8_t {
        SUCCESS,
        PASSWORD_EXPIRED,
        PASSWORD_EXPIRED_GRACE,    // In grace period
        PASSWORD_EXPIRED_LOCKED,   // Past grace period
        PASSWORD_CHANGE_REQUIRED,
        ACCOUNT_LOCKED,
        INVALID_CREDENTIALS,
    };

    LoginCheckResult checkLogin(
        const PasswordState& state,
        bool credentials_valid) const;

    // Record failed login attempt
    PasswordState recordFailedAttempt(PasswordState state) const;

    // Record successful login
    PasswordState recordSuccessfulLogin(PasswordState state) const;

    // Force password change on next login
    PasswordState forcePasswordChange(PasswordState state) const;

    // Unlock account
    PasswordState unlockAccount(PasswordState state) const;

    // ========================================================================
    // Password Hashing
    // ========================================================================

    // Hash a password (PBKDF2 with salt)
    std::string hashPassword(std::string_view password) const;

    // Verify password against hash
    bool verifyPassword(std::string_view password, std::string_view hash) const;

    // Generate secure random salt
    std::string generateSalt() const;

private:
    PasswordPolicyManager() = default;

    PasswordPolicy policy_;
};

// ============================================================================
// Common Password Checker
// ============================================================================

class CommonPasswordChecker {
public:
    static CommonPasswordChecker& getInstance();

    // Check if password is in common passwords list
    bool isCommonPassword(std::string_view password) const;

    // Load common passwords from file
    core::Status loadFromFile(std::string_view filename, core::ErrorContext* ctx = nullptr);

    // Add password to common list
    void addCommonPassword(std::string_view password);

    // Get count of common passwords
    size_t count() const { return common_passwords_.size(); }

private:
    CommonPasswordChecker();

    std::vector<std::string> common_passwords_;
};

// ============================================================================
// Helper Functions
// ============================================================================

// Get human-readable error message
inline const char* passwordValidationErrorMessage(PasswordValidationError error) {
    switch (error) {
        case PasswordValidationError::NONE:
            return "No error";
        case PasswordValidationError::TOO_SHORT:
            return "Password is too short";
        case PasswordValidationError::MISSING_UPPERCASE:
            return "Password must contain uppercase letters";
        case PasswordValidationError::MISSING_LOWERCASE:
            return "Password must contain lowercase letters";
        case PasswordValidationError::MISSING_DIGIT:
            return "Password must contain numbers";
        case PasswordValidationError::MISSING_SPECIAL:
            return "Password must contain special characters";
        case PasswordValidationError::REUSED_PASSWORD:
            return "Cannot reuse recent passwords";
        case PasswordValidationError::COMMON_PASSWORD:
            return "Password is too common";
        case PasswordValidationError::CONTAINS_USERNAME:
            return "Password cannot contain username";
        default:
            return "Unknown password error";
    }
}

inline const char* loginCheckResultMessage(PasswordPolicyManager::LoginCheckResult result) {
    switch (result) {
        case PasswordPolicyManager::LoginCheckResult::SUCCESS:
            return "Login successful";
        case PasswordPolicyManager::LoginCheckResult::PASSWORD_EXPIRED:
            return "Password has expired";
        case PasswordPolicyManager::LoginCheckResult::PASSWORD_EXPIRED_GRACE:
            return "Password has expired - please change immediately";
        case PasswordPolicyManager::LoginCheckResult::PASSWORD_EXPIRED_LOCKED:
            return "Account locked - password expired";
        case PasswordPolicyManager::LoginCheckResult::PASSWORD_CHANGE_REQUIRED:
            return "Password change required";
        case PasswordPolicyManager::LoginCheckResult::ACCOUNT_LOCKED:
            return "Account is locked";
        case PasswordPolicyManager::LoginCheckResult::INVALID_CREDENTIALS:
            return "Invalid credentials";
        default:
            return "Unknown login error";
    }
}

} // namespace scratchbird::security
