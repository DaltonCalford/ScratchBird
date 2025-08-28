#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ScratchBird
{

    // Forward declarations
    class Session;
    class SecurityContext;

    /**
     * Authentication result status
     */
    enum class AuthenticationResult {
        Success,
        InvalidCredentials,
        AccountLocked,
        PasswordExpired,
        RequiresTwoFactor,
        RequiresPasswordChange,
        AccessDenied,
        InternalError,
        Timeout,
        Cancelled
    };

    /**
     * Authentication method types
     */
    enum class AuthenticationMethod {
        Password,     // Username/password authentication
        TrustedOS,    // SSPI/Kerberos/PAM authentication
        Certificate,  // X.509 certificate authentication
        TwoFactor,    // Multi-factor authentication
        SingleSignOn, // SSO integration
        Token,        // JWT/API token authentication
        Custom        // Custom authentication provider
    };

    /**
     * Authentication context containing user credentials and session info
     */
    class AuthenticationContext
    {
      public:
        AuthenticationContext() = default;
        virtual ~AuthenticationContext() = default;

        // User identification
        void set_username(const std::string& username)
        {
            username_ = username;
        }
        const std::string& get_username() const
        {
            return username_;
        }

        void set_database(const std::string& database)
        {
            database_ = database;
        }
        const std::string& get_database() const
        {
            return database_;
        }

        // Connection information
        void set_remote_address(const std::string& address)
        {
            remote_address_ = address;
        }
        const std::string& get_remote_address() const
        {
            return remote_address_;
        }

        void set_client_info(const std::string& info)
        {
            client_info_ = info;
        }
        const std::string& get_client_info() const
        {
            return client_info_;
        }

        // Authentication specific data
        void set_credential(const std::string& key, const std::string& value)
        {
            credentials_[key] = value;
        }

        std::optional<std::string> get_credential(const std::string& key) const
        {
            auto it = credentials_.find(key);
            return (it != credentials_.end()) ? std::optional<std::string>(it->second)
                                              : std::nullopt;
        }

        // Session and timing
        void set_session_start(std::chrono::system_clock::time_point start)
        {
            session_start_ = start;
        }
        std::chrono::system_clock::time_point get_session_start() const
        {
            return session_start_;
        }

        void set_last_activity(std::chrono::system_clock::time_point activity)
        {
            last_activity_ = activity;
        }
        std::chrono::system_clock::time_point get_last_activity() const
        {
            return last_activity_;
        }

        // Authentication state
        void set_authenticated(bool authenticated)
        {
            is_authenticated_ = authenticated;
        }
        bool is_authenticated() const
        {
            return is_authenticated_;
        }

        void set_requires_2fa(bool requires_2fa)
        {
            requires_2fa_ = requires_2fa;
        }
        bool requires_2fa() const
        {
            return requires_2fa_;
        }

      private:
        std::string username_;
        std::string database_;
        std::string remote_address_;
        std::string client_info_;
        std::map<std::string, std::string> credentials_;
        std::chrono::system_clock::time_point session_start_;
        std::chrono::system_clock::time_point last_activity_;
        bool is_authenticated_ = false;
        bool requires_2fa_ = false;
    };

    /**
     * Authentication challenge for multi-step authentication
     */
    class AuthenticationChallenge
    {
      public:
        AuthenticationChallenge(const std::string& challenge_id, AuthenticationMethod method,
                                const std::string& challenge_data)
            : challenge_id_(challenge_id), method_(method), challenge_data_(challenge_data)
        {
        }

        const std::string& get_challenge_id() const
        {
            return challenge_id_;
        }
        AuthenticationMethod get_method() const
        {
            return method_;
        }
        const std::string& get_challenge_data() const
        {
            return challenge_data_;
        }

        void set_response(const std::string& response)
        {
            response_ = response;
        }
        const std::string& get_response() const
        {
            return response_;
        }

        void set_expiry(std::chrono::system_clock::time_point expiry)
        {
            expiry_time_ = expiry;
        }
        std::chrono::system_clock::time_point get_expiry() const
        {
            return expiry_time_;
        }

        bool is_expired() const
        {
            return std::chrono::system_clock::now() > expiry_time_;
        }

      private:
        std::string challenge_id_;
        AuthenticationMethod method_;
        std::string challenge_data_;
        std::string response_;
        std::chrono::system_clock::time_point expiry_time_;
    };

    /**
     * Authentication provider interface
     */
    class AuthenticationProvider
    {
      public:
        AuthenticationProvider() = default;
        virtual ~AuthenticationProvider() = default;

        // Provider identification
        virtual std::string get_provider_name() const = 0;
        virtual AuthenticationMethod get_authentication_method() const = 0;
        virtual std::vector<std::string> get_supported_capabilities() const = 0;

        // Authentication flow
        virtual AuthenticationResult authenticate(AuthenticationContext& context) = 0;
        virtual bool supports_challenge_response() const
        {
            return false;
        }
        virtual std::unique_ptr<AuthenticationChallenge>
        create_challenge(const AuthenticationContext& context)
        {
            return nullptr;
        }
        virtual AuthenticationResult
        validate_challenge_response(const AuthenticationChallenge& challenge,
                                    AuthenticationContext& context)
        {
            return AuthenticationResult::InternalError;
        }

        // Password management (optional)
        virtual bool supports_password_management() const
        {
            return false;
        }
        virtual AuthenticationResult change_password(const std::string& username,
                                                     const std::string& old_password,
                                                     const std::string& new_password)
        {
            return AuthenticationResult::InternalError;
        }

        // Account management (optional)
        virtual bool supports_account_management() const
        {
            return false;
        }
        virtual AuthenticationResult lock_account(const std::string& username)
        {
            return AuthenticationResult::InternalError;
        }
        virtual AuthenticationResult unlock_account(const std::string& username)
        {
            return AuthenticationResult::InternalError;
        }

        // Configuration and validation
        virtual bool validate_configuration() const = 0;
        virtual std::vector<std::string> get_configuration_errors() const
        {
            return {};
        }
    };

    /**
     * Authentication manager coordinating multiple providers
     */
    class AuthenticationManager
    {
      public:
        AuthenticationManager();
        ~AuthenticationManager();

        // Provider management
        bool register_provider(std::unique_ptr<AuthenticationProvider> provider);
        bool unregister_provider(const std::string& provider_name);
        std::vector<std::string> get_registered_providers() const;
        AuthenticationProvider* get_provider(const std::string& provider_name);
        AuthenticationProvider* get_provider(AuthenticationMethod method);

        // Authentication workflow
        AuthenticationResult authenticate_user(AuthenticationContext& context,
                                               const std::string& preferred_provider = "");

        // Challenge-response authentication
        std::unique_ptr<AuthenticationChallenge> initiate_challenge(const std::string& username,
                                                                    AuthenticationMethod method);
        AuthenticationResult complete_challenge(const AuthenticationChallenge& challenge,
                                                AuthenticationContext& context);

        // Session management
        bool validate_session(const std::string& session_id);
        void invalidate_session(const std::string& session_id);
        void cleanup_expired_sessions();

        // Password management
        AuthenticationResult change_user_password(const std::string& username,
                                                  const std::string& old_password,
                                                  const std::string& new_password,
                                                  const std::string& provider_name = "");

        // Account management
        AuthenticationResult lock_user_account(const std::string& username);
        AuthenticationResult unlock_user_account(const std::string& username);

        // Configuration
        void set_session_timeout(std::chrono::minutes timeout)
        {
            session_timeout_ = timeout;
        }
        void set_failed_attempt_limit(int limit)
        {
            failed_attempt_limit_ = limit;
        }
        void set_lockout_duration(std::chrono::minutes duration)
        {
            lockout_duration_ = duration;
        }

        // Monitoring and auditing
        void enable_audit_logging(bool enable)
        {
            audit_logging_enabled_ = enable;
        }
        std::vector<std::string>
        get_audit_events(const std::string& username = "",
                         std::chrono::system_clock::time_point since = {}) const;

        // Statistics
        struct AuthenticationStats {
            std::uint32_t successful_authentications = 0;
            std::uint32_t failed_authentications = 0;
            std::uint32_t locked_accounts = 0;
            std::uint32_t active_sessions = 0;
            std::chrono::system_clock::time_point last_reset;
        };

        AuthenticationStats get_statistics() const;
        void reset_statistics();

      private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;

        std::chrono::minutes session_timeout_{60};  // 1 hour default
        int failed_attempt_limit_{5};               // 5 attempts before lockout
        std::chrono::minutes lockout_duration_{30}; // 30 minutes lockout
        bool audit_logging_enabled_{true};

        // Internal helper methods
        bool is_account_locked(const std::string& username);
        void record_failed_attempt(const std::string& username);
        void clear_failed_attempts(const std::string& username);
        void log_authentication_event(const std::string& event,
                                      const AuthenticationContext& context,
                                      AuthenticationResult result);
    };

    /**
     * Security context representing authenticated user state
     */
    class SecurityContext
    {
      public:
        SecurityContext(const std::string& username, const std::string& database,
                        const std::vector<std::string>& roles = {});

        // User identity
        const std::string& get_username() const
        {
            return username_;
        }
        const std::string& get_database() const
        {
            return database_;
        }

        // Role management
        const std::vector<std::string>& get_roles() const
        {
            return roles_;
        }
        bool has_role(const std::string& role) const;
        void add_role(const std::string& role);
        void remove_role(const std::string& role);

        // Permissions
        bool has_permission(const std::string& permission) const;
        void grant_permission(const std::string& permission);
        void revoke_permission(const std::string& permission);

        // Session information
        void set_session_id(const std::string& session_id)
        {
            session_id_ = session_id;
        }
        const std::string& get_session_id() const
        {
            return session_id_;
        }

        void set_authentication_method(AuthenticationMethod method)
        {
            auth_method_ = method;
        }
        AuthenticationMethod get_authentication_method() const
        {
            return auth_method_;
        }

        void set_authentication_time(std::chrono::system_clock::time_point time)
        {
            auth_time_ = time;
        }
        std::chrono::system_clock::time_point get_authentication_time() const
        {
            return auth_time_;
        }

        // Security attributes
        void set_security_definer(bool definer)
        {
            is_security_definer_ = definer;
        }
        bool is_security_definer() const
        {
            return is_security_definer_;
        }

        void set_superuser(bool superuser)
        {
            is_superuser_ = superuser;
        }
        bool is_superuser() const
        {
            return is_superuser_;
        }

      private:
        std::string username_;
        std::string database_;
        std::vector<std::string> roles_;
        std::vector<std::string> permissions_;
        std::string session_id_;
        AuthenticationMethod auth_method_ = AuthenticationMethod::Password;
        std::chrono::system_clock::time_point auth_time_;
        bool is_security_definer_ = false;
        bool is_superuser_ = false;
    };

    // Utility functions
    std::string to_string(AuthenticationResult result);
    std::string to_string(AuthenticationMethod method);
    AuthenticationMethod parse_authentication_method(const std::string& method_str);

} // namespace ScratchBird
