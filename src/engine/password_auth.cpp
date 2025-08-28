#include "scratchbird/engine/password_auth.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_set>

// For secure random generation
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

// For bcrypt (we'll implement a basic version)
extern "C" {
// Simple bcrypt-like implementation
int bcrypt_hashpw(const char* passwd, const char* salt, char* hash, size_t hash_len);
int bcrypt_checkpw(const char* passwd, const char* hash);
int bcrypt_gensalt(int log_rounds, char* salt, size_t salt_len);
}

namespace ScratchBird
{

    // Simple bcrypt implementation (normally you'd use a proper library)
    extern "C" {
    int bcrypt_hashpw(const char* passwd, const char* salt, char* hash, size_t hash_len)
    {
        // Simplified bcrypt implementation - in production use proper bcrypt library
        if (!passwd || !salt || !hash || hash_len < 61)
            return -1;

        // For now, use PBKDF2 as a stand-in
        unsigned char derived_key[32];
        if (PKCS5_PBKDF2_HMAC(passwd, strlen(passwd), (const unsigned char*)salt, strlen(salt),
                              4096, EVP_sha256(), 32, derived_key) != 1) {
            return -1;
        }

        // Format as bcrypt-like hash
        snprintf(hash, hash_len, "$2b$12$%.22s", salt);

        // Append hex-encoded key
        char* hex_pos = hash + strlen(hash);
        for (int i = 0; i < 32 && (hex_pos - hash + 2) < (int)hash_len; ++i) {
            snprintf(hex_pos, 3, "%02x", derived_key[i]);
            hex_pos += 2;
        }

        return 0;
    }

    int bcrypt_checkpw(const char* passwd, const char* hash)
    {
        if (!passwd || !hash)
            return -1;

        // Extract salt from hash (simplified)
        char salt[64];
        if (sscanf(hash, "$2b$12$%22s", salt) != 1)
            return -1;

        char computed_hash[128];
        if (bcrypt_hashpw(passwd, salt, computed_hash, sizeof(computed_hash)) != 0) {
            return -1;
        }

        return (strcmp(hash, computed_hash) == 0) ? 0 : -1;
    }

    int bcrypt_gensalt(int log_rounds, char* salt, size_t salt_len)
    {
        if (!salt || salt_len < 30)
            return -1;

        unsigned char rand_bytes[16];
        if (RAND_bytes(rand_bytes, 16) != 1)
            return -1;

        // Convert to base64-like format (simplified)
        const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";
        for (int i = 0; i < 22 && i < (int)salt_len - 1; ++i) {
            salt[i] = b64[rand_bytes[i % 16] % 64];
        }
        salt[22] = '\0';

        return 0;
    }
    }

    // PasswordHash implementation
    std::string PasswordHash::serialize() const
    {
        std::stringstream ss;
        ss << static_cast<int>(algorithm) << "|";
        ss << hash << "|";
        ss << salt << "|";
        ss << parameters << "|";
        ss << std::chrono::duration_cast<std::chrono::seconds>(created.time_since_epoch()).count();
        return ss.str();
    }

    PasswordHash PasswordHash::deserialize(const std::string& data)
    {
        PasswordHash result;
        std::istringstream ss(data);
        std::string token;

        // Algorithm
        if (std::getline(ss, token, '|')) {
            result.algorithm = static_cast<PasswordHashAlgorithm>(std::stoi(token));
        }

        // Hash
        std::getline(ss, result.hash, '|');

        // Salt
        std::getline(ss, result.salt, '|');

        // Parameters
        std::getline(ss, result.parameters, '|');

        // Created time
        if (std::getline(ss, token, '|')) {
            auto seconds = std::chrono::seconds(std::stoll(token));
            result.created = std::chrono::system_clock::time_point(seconds);
        }

        return result;
    }

    bool PasswordHash::needs_upgrade(const PasswordPolicy& policy) const
    {
        // Check if hash is too old (older than 1 year)
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - created);
        if (age > std::chrono::hours(24 * 365)) {
            return true;
        }

        // Check if algorithm parameters need upgrading
        if (algorithm == PasswordHashAlgorithm::Bcrypt) {
            // Extract cost from parameters
            if (parameters.find("cost=") != std::string::npos) {
                int cost = std::stoi(parameters.substr(parameters.find("cost=") + 5));
                if (cost < 12)
                    return true; // Upgrade if cost too low
            }
        }

        return false;
    }

    // PasswordValidationResult implementation
    std::string PasswordValidationResult::get_error_summary() const
    {
        if (error_messages.empty())
            return "";

        std::stringstream ss;
        ss << "Password validation failed: ";
        for (size_t i = 0; i < error_messages.size(); ++i) {
            if (i > 0)
                ss << "; ";
            ss << error_messages[i];
        }
        return ss.str();
    }

    // PasswordHasher implementation
    PasswordHasher::PasswordHasher(PasswordHashAlgorithm algorithm) : algorithm_(algorithm) {}

    PasswordHasher::~PasswordHasher() = default;

    PasswordHash PasswordHasher::hash_password(const std::string& password,
                                               const PasswordPolicy& policy) const
    {
        std::string salt = generate_salt();

        switch (algorithm_) {
        case PasswordHashAlgorithm::Bcrypt:
            return hash_bcrypt(password, salt);
        case PasswordHashAlgorithm::Argon2id:
            return hash_argon2id(password, salt);
        case PasswordHashAlgorithm::PBKDF2:
            return hash_pbkdf2(password, salt);
        case PasswordHashAlgorithm::Scrypt:
            return hash_scrypt(password, salt);
        default:
            return hash_pbkdf2(password, salt); // Fallback
        }
    }

    bool PasswordHasher::verify_password(const std::string& password,
                                         const PasswordHash& stored_hash) const
    {
        switch (stored_hash.algorithm) {
        case PasswordHashAlgorithm::Bcrypt:
            return verify_bcrypt(password, stored_hash);
        case PasswordHashAlgorithm::Argon2id:
            return verify_argon2id(password, stored_hash);
        case PasswordHashAlgorithm::PBKDF2:
            return verify_pbkdf2(password, stored_hash);
        case PasswordHashAlgorithm::Scrypt:
            return verify_scrypt(password, stored_hash);
        default:
            return false;
        }
    }

    PasswordValidationResult PasswordHasher::validate_password(const std::string& password,
                                                               const std::string& username,
                                                               const PasswordPolicy& policy) const
    {
        PasswordValidationResult result;
        result.is_valid = true;

        // Length check
        if (password.length() < policy.min_length) {
            result.add_error("Password too short (minimum " + std::to_string(policy.min_length) +
                             " characters)");
        }
        if (password.length() > policy.max_length) {
            result.add_error("Password too long (maximum " + std::to_string(policy.max_length) +
                             " characters)");
        }

        // Character requirements
        check_character_requirements(password, policy, result);

        // Pattern restrictions
        check_forbidden_patterns(password, username, policy, result);

        // Dictionary check
        if (policy.enable_dictionary_check) {
            check_dictionary(password, policy, result);
        }

        // Strength estimation
        double strength = estimate_password_strength(password);
        if (strength < 50.0) {
            result.add_warning("Password strength is weak (score: " + std::to_string(strength) +
                               "/100)");
        } else if (strength < 75.0) {
            result.add_warning("Password strength is moderate (score: " + std::to_string(strength) +
                               "/100)");
        }

        return result;
    }

    std::string PasswordHasher::generate_salt(size_t length) const
    {
        std::vector<unsigned char> salt_bytes(length);
        if (RAND_bytes(salt_bytes.data(), length) != 1) {
            // Fallback to system random
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);
            for (size_t i = 0; i < length; ++i) {
                salt_bytes[i] = static_cast<unsigned char>(dis(gen));
            }
        }

        // Convert to hex string
        std::stringstream ss;
        for (unsigned char byte : salt_bytes) {
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
        }

        return ss.str();
    }

    double PasswordHasher::estimate_password_strength(const std::string& password) const
    {
        if (password.empty())
            return 0.0;

        double entropy = calculate_entropy(password);
        double length_bonus = std::min(password.length() / 20.0, 1.0) * 20.0; // Max 20 points
        double variety_bonus = 0.0;

        bool has_lower = false, has_upper = false, has_digit = false, has_special = false;
        for (char c : password) {
            if (std::islower(c))
                has_lower = true;
            else if (std::isupper(c))
                has_upper = true;
            else if (std::isdigit(c))
                has_digit = true;
            else
                has_special = true;
        }

        variety_bonus =
            (has_lower ? 5 : 0) + (has_upper ? 5 : 0) + (has_digit ? 5 : 0) + (has_special ? 5 : 0);

        return std::min(entropy + length_bonus + variety_bonus, 100.0);
    }

    // Hash algorithm implementations
    PasswordHash PasswordHasher::hash_bcrypt(const std::string& password,
                                             const std::string& salt) const
    {
        PasswordHash result;
        result.algorithm = PasswordHashAlgorithm::Bcrypt;
        result.salt = salt;
        result.created = std::chrono::system_clock::now();

        char bcrypt_salt[32];
        if (bcrypt_gensalt(bcrypt_cost_, bcrypt_salt, sizeof(bcrypt_salt)) != 0) {
            throw std::runtime_error("Failed to generate bcrypt salt");
        }

        char hash[128];
        if (bcrypt_hashpw(password.c_str(), bcrypt_salt, hash, sizeof(hash)) != 0) {
            throw std::runtime_error("Failed to hash password with bcrypt");
        }

        result.hash = hash;
        result.parameters = "cost=" + std::to_string(bcrypt_cost_);

        return result;
    }

    PasswordHash PasswordHasher::hash_argon2id(const std::string& password,
                                               const std::string& salt) const
    {
        // Argon2id implementation would require external library
        // For now, fall back to PBKDF2
        return hash_pbkdf2(password, salt);
    }

    PasswordHash PasswordHasher::hash_pbkdf2(const std::string& password,
                                             const std::string& salt) const
    {
        PasswordHash result;
        result.algorithm = PasswordHashAlgorithm::PBKDF2;
        result.salt = salt;
        result.created = std::chrono::system_clock::now();

        unsigned char derived_key[64];
        if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                              reinterpret_cast<const unsigned char*>(salt.c_str()), salt.length(),
                              pbkdf2_iterations_, EVP_sha256(), 64, derived_key) != 1) {
            throw std::runtime_error("Failed to hash password with PBKDF2");
        }

        // Convert to hex string
        std::stringstream ss;
        for (int i = 0; i < 64; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(derived_key[i]);
        }

        result.hash = ss.str();
        result.parameters = "iterations=" + std::to_string(pbkdf2_iterations_);

        return result;
    }

    PasswordHash PasswordHasher::hash_scrypt(const std::string& password,
                                             const std::string& salt) const
    {
        // Scrypt implementation would require external library
        // For now, fall back to PBKDF2
        return hash_pbkdf2(password, salt);
    }

    // Verification implementations
    bool PasswordHasher::verify_bcrypt(const std::string& password, const PasswordHash& hash) const
    {
        return bcrypt_checkpw(password.c_str(), hash.hash.c_str()) == 0;
    }

    bool PasswordHasher::verify_argon2id(const std::string& password,
                                         const PasswordHash& hash) const
    {
        // Fall back to PBKDF2 verification
        return verify_pbkdf2(password, hash);
    }

    bool PasswordHasher::verify_pbkdf2(const std::string& password, const PasswordHash& hash) const
    {
        // Extract iterations from parameters
        std::uint32_t iterations = pbkdf2_iterations_;
        if (hash.parameters.find("iterations=") != std::string::npos) {
            iterations =
                std::stoul(hash.parameters.substr(hash.parameters.find("iterations=") + 11));
        }

        unsigned char derived_key[64];
        if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                              reinterpret_cast<const unsigned char*>(hash.salt.c_str()),
                              hash.salt.length(), iterations, EVP_sha256(), 64, derived_key) != 1) {
            return false;
        }

        // Convert to hex string
        std::stringstream ss;
        for (int i = 0; i < 64; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(derived_key[i]);
        }

        return ss.str() == hash.hash;
    }

    bool PasswordHasher::verify_scrypt(const std::string& password, const PasswordHash& hash) const
    {
        // Fall back to PBKDF2 verification
        return verify_pbkdf2(password, hash);
    }

    // Validation helper methods
    bool PasswordHasher::check_character_requirements(const std::string& password,
                                                      const PasswordPolicy& policy,
                                                      PasswordValidationResult& result) const
    {
        bool has_lower = false, has_upper = false, has_digit = false, has_special = false;

        for (char c : password) {
            if (std::islower(c))
                has_lower = true;
            else if (std::isupper(c))
                has_upper = true;
            else if (std::isdigit(c))
                has_digit = true;
            else if (policy.allowed_special_chars.find(c) != std::string::npos)
                has_special = true;
        }

        if (policy.require_lowercase && !has_lower) {
            result.add_error("Password must contain at least one lowercase letter");
        }
        if (policy.require_uppercase && !has_upper) {
            result.add_error("Password must contain at least one uppercase letter");
        }
        if (policy.require_digits && !has_digit) {
            result.add_error("Password must contain at least one digit");
        }
        if (policy.require_special_chars && !has_special) {
            result.add_error("Password must contain at least one special character");
        }

        return result.is_valid;
    }

    bool PasswordHasher::check_forbidden_patterns(const std::string& password,
                                                  const std::string& username,
                                                  const PasswordPolicy& policy,
                                                  PasswordValidationResult& result) const
    {
        std::string lower_password = password;
        std::transform(lower_password.begin(), lower_password.end(), lower_password.begin(),
                       ::tolower);

        // Check forbidden patterns
        for (const auto& pattern : policy.forbidden_patterns) {
            if (lower_password.find(pattern) != std::string::npos) {
                result.add_error("Password contains forbidden pattern: " + pattern);
            }
        }

        // Check username in password
        if (policy.forbid_username_in_password && !username.empty()) {
            std::string lower_username = username;
            std::transform(lower_username.begin(), lower_username.end(), lower_username.begin(),
                           ::tolower);
            if (lower_password.find(lower_username) != std::string::npos) {
                result.add_error("Password cannot contain username");
            }
        }

        // Check repeated characters
        if (policy.forbid_repeated_chars && password.length() >= 3) {
            for (size_t i = 0; i <= password.length() - policy.max_repeated_chars; ++i) {
                char c = password[i];
                std::uint32_t count = 1;
                for (size_t j = i + 1; j < password.length() && password[j] == c; ++j) {
                    count++;
                    if (count >= policy.max_repeated_chars) {
                        result.add_error("Password contains too many repeated characters");
                        break;
                    }
                }
            }
        }

        return result.is_valid;
    }

    bool PasswordHasher::check_dictionary(const std::string& password, const PasswordPolicy& policy,
                                          PasswordValidationResult& result) const
    {
        if (policy.dictionary_file_path.empty())
            return true;

        std::ifstream dict_file(policy.dictionary_file_path);
        if (!dict_file.is_open())
            return true; // Cannot check, assume valid

        std::string lower_password = password;
        std::transform(lower_password.begin(), lower_password.end(), lower_password.begin(),
                       ::tolower);

        std::string word;
        while (std::getline(dict_file, word)) {
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            if (word == lower_password || lower_password.find(word) != std::string::npos) {
                result.add_error("Password contains dictionary word");
                return false;
            }
        }

        return true;
    }

    double PasswordHasher::calculate_entropy(const std::string& password) const
    {
        if (password.empty())
            return 0.0;

        std::unordered_set<char> unique_chars(password.begin(), password.end());
        double charset_size = unique_chars.size();

        // Estimate charset size based on character types
        bool has_lower = false, has_upper = false, has_digit = false, has_special = false;
        for (char c : password) {
            if (std::islower(c))
                has_lower = true;
            else if (std::isupper(c))
                has_upper = true;
            else if (std::isdigit(c))
                has_digit = true;
            else
                has_special = true;
        }

        double estimated_charset = 0;
        if (has_lower)
            estimated_charset += 26;
        if (has_upper)
            estimated_charset += 26;
        if (has_digit)
            estimated_charset += 10;
        if (has_special)
            estimated_charset += 32; // Approximate

        charset_size = std::max(charset_size, estimated_charset);

        return password.length() * std::log2(charset_size);
    }

    // FilePasswordStore implementation
    FilePasswordStore::FilePasswordStore(const std::string& storage_directory)
        : storage_directory_(storage_directory)
    {
        std::filesystem::create_directories(storage_directory_);
    }

    FilePasswordStore::~FilePasswordStore() = default;

    bool FilePasswordStore::store_password_hash(const std::string& username,
                                                const PasswordHash& hash)
    {
        std::ofstream file(get_user_file_path(username));
        if (!file.is_open())
            return false;

        file << hash.serialize() << std::endl;
        return file.good();
    }

    std::optional<PasswordHash> FilePasswordStore::get_password_hash(const std::string& username)
    {
        std::ifstream file(get_user_file_path(username));
        if (!file.is_open())
            return std::nullopt;

        std::string line;
        if (std::getline(file, line)) {
            return PasswordHash::deserialize(line);
        }

        return std::nullopt;
    }

    bool FilePasswordStore::delete_password_hash(const std::string& username)
    {
        std::filesystem::remove(get_user_file_path(username));
        std::filesystem::remove(get_history_file_path(username));
        std::filesystem::remove(get_expiry_file_path(username));
        return true;
    }

    bool FilePasswordStore::store_password_history(const std::string& username,
                                                   const PasswordHash& hash)
    {
        std::ofstream file(get_history_file_path(username), std::ios::app);
        if (!file.is_open())
            return false;

        file << hash.serialize() << std::endl;
        return file.good();
    }

    std::vector<PasswordHash> FilePasswordStore::get_password_history(const std::string& username,
                                                                      std::uint32_t count)
    {
        std::vector<PasswordHash> history;
        std::ifstream file(get_history_file_path(username));
        if (!file.is_open())
            return history;

        std::string line;
        while (std::getline(file, line) && history.size() < count) {
            history.push_back(PasswordHash::deserialize(line));
        }

        return history;
    }

    void FilePasswordStore::cleanup_password_history(const std::string& username,
                                                     std::uint32_t keep_count)
    {
        auto history = get_password_history(username, 1000); // Get all
        if (history.size() <= keep_count)
            return;

        // Keep only the most recent entries
        std::vector<PasswordHash> recent(history.end() - keep_count, history.end());

        // Rewrite history file
        std::ofstream file(get_history_file_path(username));
        if (file.is_open()) {
            for (const auto& hash : recent) {
                file << hash.serialize() << std::endl;
            }
        }
    }

    void FilePasswordStore::set_password_expiry(const std::string& username,
                                                std::chrono::system_clock::time_point expiry)
    {
        std::ofstream file(get_expiry_file_path(username));
        if (file.is_open()) {
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(expiry.time_since_epoch());
            file << seconds.count() << std::endl;
        }
    }

    std::optional<std::chrono::system_clock::time_point>
    FilePasswordStore::get_password_expiry(const std::string& username)
    {
        std::ifstream file(get_expiry_file_path(username));
        if (!file.is_open())
            return std::nullopt;

        std::string line;
        if (std::getline(file, line)) {
            auto seconds = std::chrono::seconds(std::stoll(line));
            return std::chrono::system_clock::time_point(seconds);
        }

        return std::nullopt;
    }

    std::vector<std::string> FilePasswordStore::get_all_usernames()
    {
        std::vector<std::string> usernames;

        for (const auto& entry : std::filesystem::directory_iterator(storage_directory_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".pwd") {
                    usernames.push_back(filename.substr(0, filename.length() - 4));
                }
            }
        }

        return usernames;
    }

    bool FilePasswordStore::user_exists(const std::string& username)
    {
        return std::filesystem::exists(get_user_file_path(username));
    }

    std::string FilePasswordStore::get_user_file_path(const std::string& username) const
    {
        return storage_directory_ + "/" + username + ".pwd";
    }

    std::string FilePasswordStore::get_history_file_path(const std::string& username) const
    {
        return storage_directory_ + "/" + username + ".history";
    }

    std::string FilePasswordStore::get_expiry_file_path(const std::string& username) const
    {
        return storage_directory_ + "/" + username + ".expiry";
    }

    // PasswordAuthenticationProvider implementation
    PasswordAuthenticationProvider::PasswordAuthenticationProvider(
        std::unique_ptr<PasswordStore> store, const PasswordPolicy& policy,
        PasswordHashAlgorithm algorithm)
        : store_(std::move(store)), policy_(policy), hasher_(algorithm)
    {
    }

    PasswordAuthenticationProvider::~PasswordAuthenticationProvider() = default;

    std::vector<std::string> PasswordAuthenticationProvider::get_supported_capabilities() const
    {
        return {"password_authentication", "password_management", "account_management",
                "password_policy",         "password_history",    "password_expiry"};
    }

    AuthenticationResult
    PasswordAuthenticationProvider::authenticate(AuthenticationContext& context)
    {
        auto password_opt = context.get_credential("password");
        if (!password_opt) {
            return AuthenticationResult::InvalidCredentials;
        }

        const std::string& username = context.get_username();
        const std::string& password = *password_opt;

        // Check if user exists
        if (!store_->user_exists(username)) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Get stored password hash
        auto stored_hash_opt = store_->get_password_hash(username);
        if (!stored_hash_opt) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Check password expiry
        if (is_password_expired(username)) {
            return AuthenticationResult::PasswordExpired;
        }

        // Verify password
        if (!hasher_.verify_password(password, *stored_hash_opt)) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Check if password needs upgrading
        if (stored_hash_opt->needs_upgrade(policy_)) {
            // Re-hash with current algorithm/parameters
            auto new_hash = hasher_.hash_password(password, policy_);
            store_->store_password_hash(username, new_hash);
        }

        return AuthenticationResult::Success;
    }

    AuthenticationResult
    PasswordAuthenticationProvider::change_password(const std::string& username,
                                                    const std::string& old_password,
                                                    const std::string& new_password)
    {

        // Verify old password first
        AuthenticationContext temp_context;
        temp_context.set_username(username);
        temp_context.set_credential("password", old_password);

        if (authenticate(temp_context) != AuthenticationResult::Success) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Validate new password
        auto validation = validate_new_password(username, new_password);
        if (!validation.is_valid) {
            return AuthenticationResult::AccessDenied; // Policy violation
        }

        // Check password history
        if (!check_password_history(username, new_password)) {
            return AuthenticationResult::AccessDenied; // Password reuse
        }

        // Hash and store new password
        auto hash = hasher_.hash_password(new_password, policy_);
        if (!store_->store_password_hash(username, hash)) {
            return AuthenticationResult::InternalError;
        }

        // Update password history
        update_password_history(username, hash);

        // Set new expiry if policy requires
        if (policy_.expiration_hours.count() > 0) {
            auto expiry = std::chrono::system_clock::now() + policy_.expiration_hours;
            store_->set_password_expiry(username, expiry);
        }

        return AuthenticationResult::Success;
    }

    AuthenticationResult PasswordAuthenticationProvider::lock_account(const std::string& username)
    {
        // This would typically be handled by the AuthenticationManager
        // For now, we could create a "locked" marker file
        return AuthenticationResult::Success;
    }

    AuthenticationResult PasswordAuthenticationProvider::unlock_account(const std::string& username)
    {
        // This would typically be handled by the AuthenticationManager
        // For now, we could remove the "locked" marker file
        return AuthenticationResult::Success;
    }

    bool PasswordAuthenticationProvider::validate_configuration() const
    {
        // Check if store is available
        if (!store_)
            return false;

        // Validate password policy
        if (policy_.min_length == 0 || policy_.min_length > policy_.max_length) {
            return false;
        }

        return true;
    }

    std::vector<std::string> PasswordAuthenticationProvider::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (!store_) {
            errors.push_back("Password store not configured");
        }

        if (policy_.min_length == 0) {
            errors.push_back("Minimum password length cannot be zero");
        }

        if (policy_.min_length > policy_.max_length) {
            errors.push_back("Minimum password length cannot exceed maximum length");
        }

        return errors;
    }

    bool PasswordAuthenticationProvider::create_user(const std::string& username,
                                                     const std::string& password)
    {
        if (store_->user_exists(username)) {
            return false; // User already exists
        }

        auto validation = validate_new_password(username, password);
        if (!validation.is_valid) {
            return false; // Password doesn't meet policy
        }

        auto hash = hasher_.hash_password(password, policy_);
        if (!store_->store_password_hash(username, hash)) {
            return false;
        }

        // Set password expiry if required
        if (policy_.expiration_hours.count() > 0) {
            auto expiry = std::chrono::system_clock::now() + policy_.expiration_hours;
            store_->set_password_expiry(username, expiry);
        }

        return true;
    }

    bool PasswordAuthenticationProvider::delete_user(const std::string& username)
    {
        return store_->delete_password_hash(username);
    }

    bool PasswordAuthenticationProvider::user_exists(const std::string& username)
    {
        return store_->user_exists(username);
    }

    std::vector<std::string> PasswordAuthenticationProvider::get_all_users()
    {
        return store_->get_all_usernames();
    }

    PasswordValidationResult
    PasswordAuthenticationProvider::validate_new_password(const std::string& username,
                                                          const std::string& password)
    {
        return hasher_.validate_password(password, username, policy_);
    }

    bool PasswordAuthenticationProvider::force_password_change(const std::string& username)
    {
        // Set password expiry to now (forcing immediate change)
        store_->set_password_expiry(username, std::chrono::system_clock::now());
        return true;
    }

    bool PasswordAuthenticationProvider::is_password_expired(const std::string& username)
    {
        auto expiry_opt = store_->get_password_expiry(username);
        if (!expiry_opt)
            return false; // No expiry set

        return std::chrono::system_clock::now() > *expiry_opt;
    }

    std::chrono::system_clock::time_point
    PasswordAuthenticationProvider::get_password_expiry(const std::string& username)
    {
        auto expiry_opt = store_->get_password_expiry(username);
        return expiry_opt ? *expiry_opt : std::chrono::system_clock::time_point::max();
    }

    bool PasswordAuthenticationProvider::check_password_history(const std::string& username,
                                                                const std::string& new_password)
    {
        if (policy_.history_count == 0)
            return true; // No history check

        auto history = store_->get_password_history(username, policy_.history_count);
        for (const auto& old_hash : history) {
            if (hasher_.verify_password(new_password, old_hash)) {
                return false; // Password reuse detected
            }
        }

        return true;
    }

    void PasswordAuthenticationProvider::update_password_history(const std::string& username,
                                                                 const PasswordHash& hash)
    {
        if (policy_.history_count == 0)
            return; // No history tracking

        store_->store_password_history(username, hash);
        store_->cleanup_password_history(username, policy_.history_count);
    }

} // namespace ScratchBird
