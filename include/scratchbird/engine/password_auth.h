#pragma once

#include "scratchbird/engine/authentication.h"

#include <chrono>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace ScratchBird
{

    /**
     * Password hashing algorithm types
     */
    enum class PasswordHashAlgorithm {
        Bcrypt,   // bcrypt with configurable cost
        Argon2id, // Argon2id (recommended)
        PBKDF2,   // PBKDF2-HMAC-SHA256
        Scrypt    // scrypt
    };

    /**
     * Password policy configuration
     */
    struct PasswordPolicy {
        // Length requirements
        std::uint32_t min_length = 8;
        std::uint32_t max_length = 128;

        // Character requirements
        bool require_lowercase = true;
        bool require_uppercase = true;
        bool require_digits = true;
        bool require_special_chars = true;
        std::string allowed_special_chars = "!@#$%^&*()_+-=[]{}|;:,.<>?";

        // Pattern restrictions
        std::vector<std::string> forbidden_patterns = {"password", "123456", "qwerty", "admin",
                                                       "root"};
        bool forbid_username_in_password = true;
        bool forbid_repeated_chars = true; // e.g., "aaa", "111"
        std::uint32_t max_repeated_chars = 2;

        // History and expiration
        std::uint32_t history_count = 12;       // Remember last N passwords
        std::chrono::hours expiration_hours{0}; // 0 = no expiration
        std::chrono::hours warning_hours{168};  // Warn 7 days before expiration

        // Account lockout
        std::uint32_t max_failed_attempts = 5;
        std::chrono::minutes lockout_duration{30};

        // Dictionary check
        bool enable_dictionary_check = true;
        std::string dictionary_file_path;
    };

    /**
     * Password hash result
     */
    struct PasswordHash {
        PasswordHashAlgorithm algorithm;
        std::string hash;       // The actual hash
        std::string salt;       // Salt used for hashing
        std::string parameters; // Algorithm-specific parameters (e.g., cost, iterations)
        std::chrono::system_clock::time_point created;

        // Serialize to string for storage
        std::string serialize() const;

        // Deserialize from stored string
        static PasswordHash deserialize(const std::string& data);

        // Verify if this hash needs upgrading (algorithm or parameters changed)
        bool needs_upgrade(const PasswordPolicy& policy) const;
    };

    /**
     * Password validation result
     */
    struct PasswordValidationResult {
        bool is_valid = false;
        std::vector<std::string> error_messages;
        std::vector<std::string> warnings;

        void add_error(const std::string& error)
        {
            is_valid = false;
            error_messages.push_back(error);
        }

        void add_warning(const std::string& warning)
        {
            warnings.push_back(warning);
        }

        std::string get_error_summary() const;
    };

    /**
     * Secure password hashing and validation
     */
    class PasswordHasher
    {
      public:
        PasswordHasher(PasswordHashAlgorithm algorithm = PasswordHashAlgorithm::Argon2id);
        ~PasswordHasher();

        // Hashing operations
        PasswordHash hash_password(const std::string& password,
                                   const PasswordPolicy& policy = {}) const;

        bool verify_password(const std::string& password, const PasswordHash& stored_hash) const;

        // Password strength and policy validation
        PasswordValidationResult validate_password(const std::string& password,
                                                   const std::string& username,
                                                   const PasswordPolicy& policy) const;

        // Hash algorithm management
        void set_algorithm(PasswordHashAlgorithm algorithm)
        {
            algorithm_ = algorithm;
        }
        PasswordHashAlgorithm get_algorithm() const
        {
            return algorithm_;
        }

        // Algorithm configuration
        void set_bcrypt_cost(int cost)
        {
            bcrypt_cost_ = cost;
        }
        void set_argon2_memory_kb(std::uint32_t memory_kb)
        {
            argon2_memory_kb_ = memory_kb;
        }
        void set_argon2_iterations(std::uint32_t iterations)
        {
            argon2_iterations_ = iterations;
        }
        void set_argon2_parallelism(std::uint32_t parallelism)
        {
            argon2_parallelism_ = parallelism;
        }
        void set_pbkdf2_iterations(std::uint32_t iterations)
        {
            pbkdf2_iterations_ = iterations;
        }
        void set_scrypt_n(std::uint32_t n)
        {
            scrypt_n_ = n;
        }
        void set_scrypt_r(std::uint32_t r)
        {
            scrypt_r_ = r;
        }
        void set_scrypt_p(std::uint32_t p)
        {
            scrypt_p_ = p;
        }

        // Utility methods
        std::string generate_salt(size_t length = 32) const;
        double estimate_password_strength(const std::string& password) const;

      private:
        PasswordHashAlgorithm algorithm_;

        // Algorithm-specific parameters
        int bcrypt_cost_ = 12;
        std::uint32_t argon2_memory_kb_ = 65536; // 64MB
        std::uint32_t argon2_iterations_ = 3;
        std::uint32_t argon2_parallelism_ = 4;
        std::uint32_t pbkdf2_iterations_ = 100000;
        std::uint32_t scrypt_n_ = 32768;
        std::uint32_t scrypt_r_ = 8;
        std::uint32_t scrypt_p_ = 1;

        // Internal implementation methods
        PasswordHash hash_bcrypt(const std::string& password, const std::string& salt) const;
        PasswordHash hash_argon2id(const std::string& password, const std::string& salt) const;
        PasswordHash hash_pbkdf2(const std::string& password, const std::string& salt) const;
        PasswordHash hash_scrypt(const std::string& password, const std::string& salt) const;

        bool verify_bcrypt(const std::string& password, const PasswordHash& hash) const;
        bool verify_argon2id(const std::string& password, const PasswordHash& hash) const;
        bool verify_pbkdf2(const std::string& password, const PasswordHash& hash) const;
        bool verify_scrypt(const std::string& password, const PasswordHash& hash) const;

        // Validation helpers
        bool check_character_requirements(const std::string& password, const PasswordPolicy& policy,
                                          PasswordValidationResult& result) const;
        bool check_forbidden_patterns(const std::string& password, const std::string& username,
                                      const PasswordPolicy& policy,
                                      PasswordValidationResult& result) const;
        bool check_dictionary(const std::string& password, const PasswordPolicy& policy,
                              PasswordValidationResult& result) const;
        double calculate_entropy(const std::string& password) const;
    };

    /**
     * User password storage and management
     */
    class PasswordStore
    {
      public:
        virtual ~PasswordStore() = default;

        // Password operations
        virtual bool store_password_hash(const std::string& username, const PasswordHash& hash) = 0;
        virtual std::optional<PasswordHash> get_password_hash(const std::string& username) = 0;
        virtual bool delete_password_hash(const std::string& username) = 0;

        // Password history
        virtual bool store_password_history(const std::string& username,
                                            const PasswordHash& hash) = 0;
        virtual std::vector<PasswordHash> get_password_history(const std::string& username,
                                                               std::uint32_t count = 12) = 0;
        virtual void cleanup_password_history(const std::string& username,
                                              std::uint32_t keep_count = 12) = 0;

        // Account management
        virtual void set_password_expiry(const std::string& username,
                                         std::chrono::system_clock::time_point expiry) = 0;
        virtual std::optional<std::chrono::system_clock::time_point>
        get_password_expiry(const std::string& username) = 0;

        // Utility
        virtual std::vector<std::string> get_all_usernames() = 0;
        virtual bool user_exists(const std::string& username) = 0;
    };

    /**
     * Simple file-based password store (for development/testing)
     */
    class FilePasswordStore : public PasswordStore
    {
      public:
        explicit FilePasswordStore(const std::string& storage_directory);
        ~FilePasswordStore() override;

        bool store_password_hash(const std::string& username, const PasswordHash& hash) override;
        std::optional<PasswordHash> get_password_hash(const std::string& username) override;
        bool delete_password_hash(const std::string& username) override;

        bool store_password_history(const std::string& username, const PasswordHash& hash) override;
        std::vector<PasswordHash> get_password_history(const std::string& username,
                                                       std::uint32_t count = 12) override;
        void cleanup_password_history(const std::string& username,
                                      std::uint32_t keep_count = 12) override;

        void set_password_expiry(const std::string& username,
                                 std::chrono::system_clock::time_point expiry) override;
        std::optional<std::chrono::system_clock::time_point>
        get_password_expiry(const std::string& username) override;

        std::vector<std::string> get_all_usernames() override;
        bool user_exists(const std::string& username) override;

      private:
        std::string storage_directory_;
        std::string get_user_file_path(const std::string& username) const;
        std::string get_history_file_path(const std::string& username) const;
        std::string get_expiry_file_path(const std::string& username) const;
    };

    /**
     * Password authentication provider
     */
    class PasswordAuthenticationProvider : public AuthenticationProvider
    {
      public:
        PasswordAuthenticationProvider(
            std::unique_ptr<PasswordStore> store, const PasswordPolicy& policy = {},
            PasswordHashAlgorithm algorithm = PasswordHashAlgorithm::Argon2id);
        ~PasswordAuthenticationProvider() override;

        // AuthenticationProvider interface
        std::string get_provider_name() const override
        {
            return "Password";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::Password;
        }
        std::vector<std::string> get_supported_capabilities() const override;

        AuthenticationResult authenticate(AuthenticationContext& context) override;

        bool supports_challenge_response() const override
        {
            return false;
        }

        // Password management support
        bool supports_password_management() const override
        {
            return true;
        }
        AuthenticationResult change_password(const std::string& username,
                                             const std::string& old_password,
                                             const std::string& new_password) override;

        // Account management support
        bool supports_account_management() const override
        {
            return true;
        }
        AuthenticationResult lock_account(const std::string& username) override;
        AuthenticationResult unlock_account(const std::string& username) override;

        // Configuration validation
        bool validate_configuration() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // User management
        bool create_user(const std::string& username, const std::string& password);
        bool delete_user(const std::string& username);
        bool user_exists(const std::string& username);
        std::vector<std::string> get_all_users();

        // Password policy management
        void set_password_policy(const PasswordPolicy& policy)
        {
            policy_ = policy;
        }
        const PasswordPolicy& get_password_policy() const
        {
            return policy_;
        }

        // Password operations
        PasswordValidationResult validate_new_password(const std::string& username,
                                                       const std::string& password);
        bool force_password_change(const std::string& username);
        bool is_password_expired(const std::string& username);
        std::chrono::system_clock::time_point get_password_expiry(const std::string& username);

      private:
        std::unique_ptr<PasswordStore> store_;
        PasswordPolicy policy_;
        PasswordHasher hasher_;

        bool check_password_history(const std::string& username, const std::string& new_password);
        void update_password_history(const std::string& username, const PasswordHash& hash);
    };

} // namespace ScratchBird
