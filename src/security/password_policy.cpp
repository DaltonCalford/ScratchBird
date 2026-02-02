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
// P3-1: Password Expiration & Policy Implementation
//
// November 25, 2025

#include "scratchbird/security/password_policy.h"
#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace scratchbird::security {

// ============================================================================
// PasswordPolicyManager Implementation
// ============================================================================

PasswordPolicyManager& PasswordPolicyManager::getInstance() {
    static PasswordPolicyManager instance;
    return instance;
}

void PasswordPolicyManager::setPolicy(const PasswordPolicy& policy) {
    policy_ = policy;
}

PasswordValidationResult PasswordPolicyManager::validatePassword(
    std::string_view password,
    std::string_view username,
    const std::vector<std::string>& password_history) const {

    PasswordValidationResult result = PasswordValidationResult::success();

    // Check minimum length
    if (password.length() < policy_.min_length) {
        result.addError(PasswordValidationError::TOO_SHORT,
            "Password must be at least " + std::to_string(policy_.min_length) + " characters");
    }

    // Check complexity if enforced
    if (policy_.enforce_complexity) {
        uint32_t uppercase_count = 0;
        uint32_t lowercase_count = 0;
        uint32_t digit_count = 0;
        uint32_t special_count = 0;

        for (char c : password) {
            if (std::isupper(static_cast<unsigned char>(c))) uppercase_count++;
            else if (std::islower(static_cast<unsigned char>(c))) lowercase_count++;
            else if (std::isdigit(static_cast<unsigned char>(c))) digit_count++;
            else special_count++;
        }

        if (uppercase_count < policy_.min_uppercase) {
            result.addError(PasswordValidationError::MISSING_UPPERCASE,
                "Password must contain at least " + std::to_string(policy_.min_uppercase) + " uppercase letters");
        }

        if (lowercase_count < policy_.min_lowercase) {
            result.addError(PasswordValidationError::MISSING_LOWERCASE,
                "Password must contain at least " + std::to_string(policy_.min_lowercase) + " lowercase letters");
        }

        if (digit_count < policy_.min_digits) {
            result.addError(PasswordValidationError::MISSING_DIGIT,
                "Password must contain at least " + std::to_string(policy_.min_digits) + " numbers");
        }

        if (special_count < policy_.min_special) {
            result.addError(PasswordValidationError::MISSING_SPECIAL,
                "Password must contain at least " + std::to_string(policy_.min_special) + " special characters");
        }
    }

    // Check if password contains username
    if (!username.empty()) {
        std::string lower_password(password);
        std::string lower_username(username);
        std::transform(lower_password.begin(), lower_password.end(), lower_password.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::transform(lower_username.begin(), lower_username.end(), lower_username.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower_password.find(lower_username) != std::string::npos) {
            result.addError(PasswordValidationError::CONTAINS_USERNAME,
                "Password cannot contain your username");
        }
    }

    // Check password reuse
    if (policy_.prevent_reuse && !password_history.empty()) {
        std::string hash = hashPassword(password);
        if (isPasswordReused(hash, password_history)) {
            result.addError(PasswordValidationError::REUSED_PASSWORD,
                "Cannot reuse one of your last " + std::to_string(policy_.history_count) + " passwords");
        }
    }

    // Check common passwords
    if (CommonPasswordChecker::getInstance().isCommonPassword(password)) {
        result.addError(PasswordValidationError::COMMON_PASSWORD,
            "This password is too common");
    }

    return result;
}

bool PasswordPolicyManager::meetsComplexityRequirements(std::string_view password) const {
    if (!policy_.enforce_complexity) return true;

    uint32_t uppercase_count = 0;
    uint32_t lowercase_count = 0;
    uint32_t digit_count = 0;
    uint32_t special_count = 0;

    for (char c : password) {
        if (std::isupper(static_cast<unsigned char>(c))) uppercase_count++;
        else if (std::islower(static_cast<unsigned char>(c))) lowercase_count++;
        else if (std::isdigit(static_cast<unsigned char>(c))) digit_count++;
        else special_count++;
    }

    return password.length() >= policy_.min_length &&
           uppercase_count >= policy_.min_uppercase &&
           lowercase_count >= policy_.min_lowercase &&
           digit_count >= policy_.min_digits &&
           special_count >= policy_.min_special;
}

bool PasswordPolicyManager::isPasswordReused(
    std::string_view password_hash,
    const std::vector<std::string>& history) const {

    // Check recent history up to history_count
    size_t check_count = std::min(static_cast<size_t>(policy_.history_count), history.size());

    for (size_t i = 0; i < check_count; ++i) {
        if (history[history.size() - 1 - i] == password_hash) {
            return true;
        }
    }
    return false;
}

PasswordState PasswordPolicyManager::createPasswordState(bool never_expires) const {
    PasswordState state;
    state.created_at = std::chrono::system_clock::now();
    state.last_changed = state.created_at;
    state.expires_at = calculateExpirationDate();
    state.never_expires = never_expires;
    state.force_change = false;
    state.failed_attempts = 0;
    state.locked_until = std::chrono::system_clock::time_point{};
    return state;
}

PasswordState PasswordPolicyManager::updatePasswordState(
    const PasswordState& current,
    std::string_view old_password_hash) const {

    PasswordState state = current;
    state.last_changed = std::chrono::system_clock::now();
    state.expires_at = calculateExpirationDate();
    state.force_change = false;
    state.failed_attempts = 0;

    // Add old password to history
    if (policy_.prevent_reuse && !old_password_hash.empty()) {
        state.password_history.push_back(std::string(old_password_hash));

        // Trim history to configured size
        while (state.password_history.size() > policy_.history_count) {
            state.password_history.erase(state.password_history.begin());
        }
    }

    return state;
}

std::chrono::system_clock::time_point PasswordPolicyManager::calculateExpirationDate() const {
    if (policy_.expiration_days == 0) {
        // Never expires - set to far future
        return std::chrono::system_clock::time_point::max();
    }
    return std::chrono::system_clock::now() +
           std::chrono::hours(24 * policy_.expiration_days);
}

PasswordPolicyManager::LoginCheckResult PasswordPolicyManager::checkLogin(
    const PasswordState& state,
    bool credentials_valid) const {

    // Check if account is locked
    if (state.isLocked()) {
        return LoginCheckResult::ACCOUNT_LOCKED;
    }

    // Check credentials first
    if (!credentials_valid) {
        return LoginCheckResult::INVALID_CREDENTIALS;
    }

    // Check if password change is forced
    if (state.force_change) {
        return LoginCheckResult::PASSWORD_CHANGE_REQUIRED;
    }

    // Check expiration
    if (state.isExpired()) {
        if (state.isInGracePeriod(policy_.grace_period_days)) {
            return LoginCheckResult::PASSWORD_EXPIRED_GRACE;
        }
        return LoginCheckResult::PASSWORD_EXPIRED_LOCKED;
    }

    return LoginCheckResult::SUCCESS;
}

PasswordState PasswordPolicyManager::recordFailedAttempt(PasswordState state) const {
    state.failed_attempts++;

    if (state.failed_attempts >= policy_.max_failed_attempts) {
        // Lock the account
        state.locked_until = std::chrono::system_clock::now() +
                            std::chrono::minutes(policy_.lockout_duration_minutes);
    }

    return state;
}

PasswordState PasswordPolicyManager::recordSuccessfulLogin(PasswordState state) const {
    state.failed_attempts = 0;
    return state;
}

PasswordState PasswordPolicyManager::forcePasswordChange(PasswordState state) const {
    state.force_change = true;
    return state;
}

PasswordState PasswordPolicyManager::unlockAccount(PasswordState state) const {
    state.locked_until = std::chrono::system_clock::time_point{};
    state.failed_attempts = 0;
    return state;
}

std::string PasswordPolicyManager::hashPassword(std::string_view password) const {
    // Simple hash for demonstration - in production use PBKDF2 or Argon2
    // This is a placeholder that should be replaced with proper cryptographic hashing

    std::string salt = generateSalt();
    std::string to_hash = salt + std::string(password);

    // Simple hash (NOT secure - use proper PBKDF2/Argon2 in production)
    std::hash<std::string> hasher;
    size_t hash = hasher(to_hash);

    std::stringstream ss;
    ss << "$sbhash$" << salt << "$" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

bool PasswordPolicyManager::verifyPassword(std::string_view password, std::string_view hash) const {
    // Parse the hash format: $sbhash$<salt>$<hash>
    std::string hash_str(hash);
    if (hash_str.substr(0, 8) != "$sbhash$") {
        return false;
    }

    size_t salt_start = 8;
    size_t salt_end = hash_str.find('$', salt_start);
    if (salt_end == std::string::npos) {
        return false;
    }

    std::string salt = hash_str.substr(salt_start, salt_end - salt_start);
    std::string stored_hash = hash_str.substr(salt_end + 1);

    // Compute hash with same salt
    std::string to_hash = salt + std::string(password);
    std::hash<std::string> hasher;
    size_t computed = hasher(to_hash);

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << computed;

    return ss.str() == stored_hash;
}

std::string PasswordPolicyManager::generateSalt() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    uint64_t salt = dist(gen);

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << salt;
    return ss.str();
}

// ============================================================================
// CommonPasswordChecker Implementation
// ============================================================================

CommonPasswordChecker::CommonPasswordChecker() {
    // Initialize with most common passwords
    common_passwords_ = {
        "password", "123456", "12345678", "qwerty", "abc123",
        "monkey", "1234567", "letmein", "trustno1", "dragon",
        "baseball", "iloveyou", "master", "sunshine", "ashley",
        "bailey", "passw0rd", "shadow", "123123", "654321",
        "superman", "qazwsx", "michael", "football", "password1",
        "password123", "batman", "login", "admin", "welcome",
        "hello", "charlie", "donald", "password2", "qwerty123",
        "mustang", "access", "master123", "password!", "123456789"
    };
}

CommonPasswordChecker& CommonPasswordChecker::getInstance() {
    static CommonPasswordChecker instance;
    return instance;
}

bool CommonPasswordChecker::isCommonPassword(std::string_view password) const {
    std::string lower_password(password);
    std::transform(lower_password.begin(), lower_password.end(), lower_password.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return std::find(common_passwords_.begin(), common_passwords_.end(), lower_password)
           != common_passwords_.end();
}

core::Status CommonPasswordChecker::loadFromFile(std::string_view filename, core::ErrorContext* ctx) {
    std::ifstream file{std::string{filename}};
    if (!file.is_open()) {
        if (ctx) {
            ctx->message = "Failed to open common passwords file: " + std::string(filename);
            ctx->code = core::Status::IO_ERROR;
        }
        return core::Status::IO_ERROR;
    }

    common_passwords_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            std::transform(line.begin(), line.end(), line.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            common_passwords_.push_back(line);
        }
    }

    return core::Status::OK;
}

void CommonPasswordChecker::addCommonPassword(std::string_view password) {
    std::string lower(password);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    common_passwords_.push_back(lower);
}

} // namespace scratchbird::security
