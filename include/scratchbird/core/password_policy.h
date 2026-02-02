/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <string>
#include <vector>
#include "status.h"
#include "error_context.h"

namespace scratchbird {
namespace core {

/**
 * Password Policy Configuration
 *
 * Defines password strength requirements to prevent weak passwords.
 *
 * Security Phase 3.5 - P0-1: Password Policy Enforcement
 * Addresses CWE-521: Weak Password Requirements
 */
struct PasswordPolicy {
    size_t min_length = 8;           // Minimum password length
    size_t max_length = 72;          // Maximum (BCrypt limit)
    bool require_uppercase = true;   // At least one uppercase letter
    bool require_lowercase = true;   // At least one lowercase letter
    bool require_digit = true;       // At least one digit
    bool require_symbol = true;      // At least one symbol
    bool check_common_passwords = true;  // Reject common passwords
    size_t history_count = 5;        // Prevent password reuse (future)
};

/**
 * Validates password against policy
 *
 * @param password Plaintext password to validate
 * @param policy Password policy rules
 * @param ctx Error context for detailed error messages
 * @return Status::OK if valid, error status otherwise
 *
 * Error Codes:
 * - Status::INVALID_ARGUMENT: Password does not meet policy requirements
 */
Status validatePasswordPolicy(
    const std::string& password,
    const PasswordPolicy& policy,
    ErrorContext* ctx
);

/**
 * Checks if password is in common password list
 *
 * @param password Password to check (case-insensitive)
 * @return true if password is common, false otherwise
 *
 * Uses top 10,000 most common passwords from known breaches.
 */
bool isCommonPassword(const std::string& password);

/**
 * Loads common password dictionary
 *
 * Lazy-loaded on first use. Safe to call multiple times.
 *
 * @param ctx Error context
 * @return Status::OK if loaded successfully
 */
Status loadCommonPasswordDictionary(ErrorContext* ctx);

}  // namespace core
}  // namespace scratchbird
