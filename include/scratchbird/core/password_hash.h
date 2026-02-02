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

namespace scratchbird {
namespace core {

/**
 * Password Hashing Utility
 *
 * Provides secure password hashing using bcrypt algorithm.
 *
 * Security Phase 3.0 - Password Hashing Implementation
 */
class PasswordHash {
public:
    /**
     * Hash a plaintext password using bcrypt
     *
     * @param password Plaintext password
     * @param cost Work factor (4-31, default 12)
     * @return BCrypt hash string (60 chars)
     *
     * Format: $2a$12$<salt(22)><hash(31)>
     *
     * Cost Recommendations:
     * - 10: Fast, ~100ms (testing)
     * - 12: Standard, ~250ms (default)
     * - 14: Strong, ~1s (high security)
     */
    static std::string hashPassword(const std::string& password, int cost = 12);

    /**
     * Verify a plaintext password against a bcrypt hash
     *
     * @param password Plaintext password to verify
     * @param hash Stored bcrypt hash
     * @return true if password matches, false otherwise
     *
     * Timing-safe comparison to prevent timing attacks
     */
    static bool verifyPassword(const std::string& password, const std::string& hash);

    /**
     * Check if a string is a valid bcrypt hash
     *
     * @param hash String to validate
     * @return true if valid bcrypt format, false otherwise
     */
    static bool isValidHash(const std::string& hash);

    /**
     * Get the cost factor from a bcrypt hash
     *
     * @param hash BCrypt hash
     * @return Cost factor (4-31), or -1 if invalid
     */
    static int getCost(const std::string& hash);

private:
    // Minimum and maximum allowed cost factors
    static constexpr int MIN_COST = 4;
    static constexpr int MAX_COST = 31;
    static constexpr int DEFAULT_COST = 12;
};

} // namespace core
} // namespace scratchbird
