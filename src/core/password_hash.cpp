/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/password_hash.h"

#include <cstring>
#include <stdexcept>
#include <random>
#include <sstream>
#include <iomanip>

// Use bcrypt from crypt.h (Linux/Unix standard)
// Note: On some systems, may need to link with -lcrypt
#ifdef __unix__
#include <unistd.h>
#if defined(_GNU_SOURCE) || defined(__linux__)
#include <crypt.h>
#define HAVE_CRYPT_R 1
#endif
#endif

// Try to use OpenSSL for secure random if available
#ifdef __has_include
#if __has_include(<openssl/rand.h>)
#include <openssl/rand.h>
#define HAVE_OPENSSL_RAND 1
#endif
#endif

namespace scratchbird {
namespace core {

namespace {

/**
 * Generate cryptographically secure random bytes
 */
void secureRandomBytes(unsigned char* buffer, size_t length)
{
#ifdef HAVE_OPENSSL_RAND
    // Use OpenSSL's cryptographically secure RNG
    if (RAND_bytes(buffer, static_cast<int>(length)) != 1)
    {
        throw std::runtime_error("Failed to generate secure random bytes");
    }
#else
    // Fallback to std::random_device (less ideal but better than nothing)
    std::random_device rd;
    for (size_t i = 0; i < length; i++)
    {
        buffer[i] = static_cast<unsigned char>(rd());
    }
#endif
}

/**
 * Generate bcrypt-compatible salt
 */
std::string generateSalt(int cost)
{
    // BCrypt salt format: $2a$<cost>$<22-char-salt>
    const char* salt_chars = "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    // Generate 16 random bytes (will be encoded to 22 chars)
    unsigned char random_bytes[16];
    secureRandomBytes(random_bytes, 16);

    // Build salt string
    std::ostringstream salt;
    salt << "$2a$" << std::setfill('0') << std::setw(2) << cost << "$";

    // Encode random bytes to base64-like format (22 chars)
    for (int i = 0; i < 16; i++)
    {
        salt << salt_chars[random_bytes[i] % 64];
        if (i < 15)
        {
            salt << salt_chars[(random_bytes[i] >> 2) % 64];
        }
    }

    // Truncate to exact salt length (29 chars total: $2a$12$<22-chars>)
    std::string result = salt.str();
    if (result.length() > 29)
    {
        result = result.substr(0, 29);
    }
    else
    {
        // Pad with random chars if needed
        while (result.length() < 29)
        {
            unsigned char rb;
            secureRandomBytes(&rb, 1);
            result += salt_chars[rb % 64];
        }
    }

    return result;
}

} // anonymous namespace

std::string PasswordHash::hashPassword(const std::string& password, int cost)
{
    // Validate cost parameter
    if (cost < MIN_COST || cost > MAX_COST)
    {
        throw std::invalid_argument("BCrypt cost must be between " +
                                    std::to_string(MIN_COST) + " and " +
                                    std::to_string(MAX_COST));
    }

    // Validate password length (bcrypt has 72 byte limit)
    if (password.length() > 72)
    {
        throw std::invalid_argument("Password exceeds bcrypt limit of 72 bytes");
    }

#ifdef HAVE_CRYPT_R
    // Use crypt_r for thread-safety
    struct crypt_data data;
    memset(&data, 0, sizeof(data));

    // Generate cryptographically secure salt
    std::string salt = generateSalt(cost);

    // Hash the password
    char* hash = crypt_r(password.c_str(), salt.c_str(), &data);
    if (hash == nullptr)
    {
        throw std::runtime_error("Password hashing failed");
    }

    std::string result(hash);

    // Validate result is proper bcrypt format
    if (!isValidHash(result))
    {
        throw std::runtime_error("Generated hash is not valid bcrypt format");
    }

    return result;
#else
    // SECURITY FIX (CRITICAL-2): Fail safely instead of using insecure fallback
    // Weak password hashing would expose all passwords if database is compromised
    throw std::runtime_error(
        "Password hashing requires bcrypt support (crypt_r). "
        "Please rebuild with bcrypt support enabled. "
        "On Debian/Ubuntu: apt install libcrypt-dev. "
        "On RHEL/CentOS: yum install glibc-devel. "
        "This is a security requirement and cannot be bypassed."
    );
#endif
}

bool PasswordHash::verifyPassword(const std::string& password, const std::string& hash)
{
    // Validate hash format
    if (!isValidHash(hash))
    {
        return false;
    }

    // Validate password length
    if (password.length() > 72)
    {
        return false;
    }

#ifdef HAVE_CRYPT_R
    // Use crypt_r for thread-safety
    struct crypt_data data;
    memset(&data, 0, sizeof(data));

    // Hash the password with the stored hash as the salt
    // The hash contains the salt, so crypt will use the same salt
    char* computed_hash = crypt_r(password.c_str(), hash.c_str(), &data);
    if (computed_hash == nullptr)
    {
        return false;
    }

    // Timing-safe comparison
    // Use constant-time comparison to prevent timing attacks
    size_t hash_len = hash.length();
    size_t computed_len = strlen(computed_hash);

    if (hash_len != computed_len)
    {
        return false;
    }

    // Constant-time comparison
    int result = 0;
    for (size_t i = 0; i < hash_len; i++)
    {
        result |= hash[i] ^ computed_hash[i];
    }

    return result == 0;
#else
    // SECURITY FIX (CRITICAL-2): Fail safely instead of using insecure fallback
    throw std::runtime_error(
        "Password verification requires bcrypt support (crypt_r). "
        "Please rebuild with bcrypt support enabled. "
        "This is a security requirement and cannot be bypassed."
    );
#endif
}

bool PasswordHash::isValidHash(const std::string& hash)
{
    // BCrypt hash format: $2a$<cost>$<salt(22)><hash(31)>
    // Total length: 60 characters

    if (hash.length() != 60)
    {
        return false;
    }

    // Check prefix
    if (hash.substr(0, 4) != "$2a$" && hash.substr(0, 4) != "$2b$" && hash.substr(0, 4) != "$2y$")
    {
        return false;
    }

    // Check cost (2 digits)
    if (!isdigit(hash[4]) || !isdigit(hash[5]))
    {
        return false;
    }

    // Check separator
    if (hash[6] != '$')
    {
        return false;
    }

    return true;
}

int PasswordHash::getCost(const std::string& hash)
{
    if (!isValidHash(hash))
    {
        return -1;
    }

    // Extract cost from positions 4-5
    int cost = (hash[4] - '0') * 10 + (hash[5] - '0');

    if (cost < MIN_COST || cost > MAX_COST)
    {
        return -1;
    }

    return cost;
}

} // namespace core
} // namespace scratchbird
