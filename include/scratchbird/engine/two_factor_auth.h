#pragma once

#include "scratchbird/engine/authentication.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ScratchBird
{

    /**
     * Two-factor authentication methods
     */
    enum class TwoFactorMethod {
        TOTP,     // Time-based One-Time Password (Google Authenticator, Authy, etc.)
        SMS,      // SMS text message
        Email,    // Email verification
        Hardware, // Hardware security keys (YubiKey, etc.)
        Backup,   // Backup/recovery codes
        Push,     // Push notifications to mobile app
        Voice,    // Voice call verification
        Custom    // Custom 2FA method
    };

    /**
     * 2FA enrollment status
     */
    enum class TwoFactorStatus {
        NotEnrolled, // User has not set up 2FA
        Enrolled,    // User has 2FA enabled
        Pending,     // Enrollment process in progress
        Disabled,    // 2FA temporarily disabled
        Locked       // 2FA locked due to too many failures
    };

    /**
     * TOTP (Time-based One-Time Password) configuration
     */
    struct TOTPConfig {
        std::string secret_key;         // Base32-encoded secret
        std::uint32_t time_step = 30;   // Time step in seconds (usually 30)
        std::uint32_t digits = 6;       // Number of digits (6 or 8)
        std::string algorithm = "SHA1"; // Hash algorithm (SHA1, SHA256, SHA512)
        std::string issuer = "ScratchBird";
        std::string account_name; // Usually username or email

        // Generate QR code URL for enrollment
        std::string generate_qr_code_url() const;

        // Generate current TOTP code
        std::string generate_code() const;
        std::string generate_code(std::uint64_t timestamp) const;

        // Verify TOTP code with time window tolerance
        bool verify_code(const std::string& code, std::uint32_t window = 1) const;
    };

    /**
     * SMS/Email configuration
     */
    struct MessageConfig {
        std::string recipient; // Phone number or email address
        std::string template_text = "Your verification code is: {code}";
        std::uint32_t code_length = 6;  // Length of verification code
        std::chrono::minutes expiry{5}; // Code expiry time
        std::uint32_t max_attempts = 3; // Max verification attempts
    };

    /**
     * Hardware token configuration
     */
    struct HardwareTokenConfig {
        std::string device_id;                   // Hardware device identifier
        std::string public_key;                  // Device public key
        std::string challenge_type = "webauthn"; // Challenge protocol
        std::chrono::minutes timeout{2};         // Response timeout
    };

    /**
     * Backup codes configuration
     */
    struct BackupCodesConfig {
        std::uint32_t code_count = 10;       // Number of backup codes to generate
        std::uint32_t code_length = 8;       // Length of each backup code
        bool single_use = true;              // Codes are single-use only
        std::chrono::hours expiry{24 * 365}; // Backup codes expire after 1 year
    };

    /**
     * Two-factor authentication provider interface
     */
    class TwoFactorProvider
    {
      public:
        TwoFactorProvider() = default;
        virtual ~TwoFactorProvider() = default;

        // Provider identification
        virtual std::string get_provider_name() const = 0;
        virtual TwoFactorMethod get_method() const = 0;
        virtual std::vector<std::string> get_supported_capabilities() const = 0;

        // Enrollment process
        virtual AuthenticationResult begin_enrollment(const std::string& username,
                                                      AuthenticationContext& context) = 0;
        virtual AuthenticationResult complete_enrollment(const std::string& username,
                                                         const std::string& verification_data,
                                                         AuthenticationContext& context) = 0;
        virtual AuthenticationResult cancel_enrollment(const std::string& username) = 0;

        // Authentication process
        virtual std::unique_ptr<AuthenticationChallenge>
        create_challenge(const std::string& username, AuthenticationContext& context) = 0;
        virtual AuthenticationResult verify_response(const AuthenticationChallenge& challenge,
                                                     const std::string& response,
                                                     AuthenticationContext& context) = 0;

        // Status and management
        virtual TwoFactorStatus get_status(const std::string& username) const = 0;
        virtual AuthenticationResult disable_2fa(const std::string& username) = 0;
        virtual AuthenticationResult enable_2fa(const std::string& username) = 0;

        // Recovery and backup
        virtual std::vector<std::string> generate_backup_codes(const std::string& username) = 0;
        virtual bool verify_backup_code(const std::string& username, const std::string& code) = 0;
        virtual AuthenticationResult reset_2fa(const std::string& username) = 0;

        // Configuration validation
        virtual bool validate_configuration() const = 0;
        virtual std::vector<std::string> get_configuration_errors() const
        {
            return {};
        }
    };

    /**
     * TOTP (Time-based One-Time Password) provider
     */
    class TOTPProvider : public TwoFactorProvider
    {
      public:
        TOTPProvider();
        ~TOTPProvider() override;

        std::string get_provider_name() const override
        {
            return "TOTP";
        }
        TwoFactorMethod get_method() const override
        {
            return TwoFactorMethod::TOTP;
        }
        std::vector<std::string> get_supported_capabilities() const override;

        AuthenticationResult begin_enrollment(const std::string& username,
                                              AuthenticationContext& context) override;
        AuthenticationResult complete_enrollment(const std::string& username,
                                                 const std::string& verification_code,
                                                 AuthenticationContext& context) override;
        AuthenticationResult cancel_enrollment(const std::string& username) override;

        std::unique_ptr<AuthenticationChallenge>
        create_challenge(const std::string& username, AuthenticationContext& context) override;
        AuthenticationResult verify_response(const AuthenticationChallenge& challenge,
                                             const std::string& response,
                                             AuthenticationContext& context) override;

        TwoFactorStatus get_status(const std::string& username) const override;
        AuthenticationResult disable_2fa(const std::string& username) override;
        AuthenticationResult enable_2fa(const std::string& username) override;

        std::vector<std::string> generate_backup_codes(const std::string& username) override;
        bool verify_backup_code(const std::string& username, const std::string& code) override;
        AuthenticationResult reset_2fa(const std::string& username) override;

        bool validate_configuration() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // TOTP-specific methods
        std::string generate_secret_key() const;
        TOTPConfig get_totp_config(const std::string& username) const;
        std::string get_qr_code_url(const std::string& username) const;

        // Configuration
        void set_issuer(const std::string& issuer)
        {
            issuer_ = issuer;
        }
        const std::string& get_issuer() const
        {
            return issuer_;
        }

        void set_time_step(std::uint32_t time_step)
        {
            time_step_ = time_step;
        }
        std::uint32_t get_time_step() const
        {
            return time_step_;
        }

        void set_digits(std::uint32_t digits)
        {
            digits_ = digits;
        }
        std::uint32_t get_digits() const
        {
            return digits_;
        }

      private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;

        std::string issuer_ = "ScratchBird";
        std::uint32_t time_step_ = 30;
        std::uint32_t digits_ = 6;
        std::string algorithm_ = "SHA1";
    };

    /**
     * SMS-based 2FA provider
     */
    class SMSProvider : public TwoFactorProvider
    {
      public:
        explicit SMSProvider(const std::string& api_key = "", const std::string& service_url = "");
        ~SMSProvider() override;

        std::string get_provider_name() const override
        {
            return "SMS";
        }
        TwoFactorMethod get_method() const override
        {
            return TwoFactorMethod::SMS;
        }
        std::vector<std::string> get_supported_capabilities() const override;

        AuthenticationResult begin_enrollment(const std::string& username,
                                              AuthenticationContext& context) override;
        AuthenticationResult complete_enrollment(const std::string& username,
                                                 const std::string& phone_number,
                                                 AuthenticationContext& context) override;
        AuthenticationResult cancel_enrollment(const std::string& username) override;

        std::unique_ptr<AuthenticationChallenge>
        create_challenge(const std::string& username, AuthenticationContext& context) override;
        AuthenticationResult verify_response(const AuthenticationChallenge& challenge,
                                             const std::string& response,
                                             AuthenticationContext& context) override;

        TwoFactorStatus get_status(const std::string& username) const override;
        AuthenticationResult disable_2fa(const std::string& username) override;
        AuthenticationResult enable_2fa(const std::string& username) override;

        std::vector<std::string> generate_backup_codes(const std::string& username) override;
        bool verify_backup_code(const std::string& username, const std::string& code) override;
        AuthenticationResult reset_2fa(const std::string& username) override;

        bool validate_configuration() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // SMS-specific configuration
        void set_api_key(const std::string& api_key)
        {
            api_key_ = api_key;
        }
        void set_service_url(const std::string& service_url)
        {
            service_url_ = service_url;
        }
        void set_message_template(const std::string& template_text)
        {
            message_template_ = template_text;
        }

      private:
        std::string api_key_;
        std::string service_url_;
        std::string message_template_ = "Your ScratchBird verification code is: {code}";

        class Impl;
        std::unique_ptr<Impl> pimpl_;

        // Helper methods
        bool send_sms(const std::string& phone_number, const std::string& message);
        std::string generate_verification_code() const;
    };

    /**
     * Email-based 2FA provider
     */
    class EmailProvider : public TwoFactorProvider
    {
      public:
        explicit EmailProvider(const std::string& smtp_server = "", std::uint16_t smtp_port = 587,
                               const std::string& smtp_username = "",
                               const std::string& smtp_password = "");
        ~EmailProvider() override;

        std::string get_provider_name() const override
        {
            return "Email";
        }
        TwoFactorMethod get_method() const override
        {
            return TwoFactorMethod::Email;
        }
        std::vector<std::string> get_supported_capabilities() const override;

        AuthenticationResult begin_enrollment(const std::string& username,
                                              AuthenticationContext& context) override;
        AuthenticationResult complete_enrollment(const std::string& username,
                                                 const std::string& email_address,
                                                 AuthenticationContext& context) override;
        AuthenticationResult cancel_enrollment(const std::string& username) override;

        std::unique_ptr<AuthenticationChallenge>
        create_challenge(const std::string& username, AuthenticationContext& context) override;
        AuthenticationResult verify_response(const AuthenticationChallenge& challenge,
                                             const std::string& response,
                                             AuthenticationContext& context) override;

        TwoFactorStatus get_status(const std::string& username) const override;
        AuthenticationResult disable_2fa(const std::string& username) override;
        AuthenticationResult enable_2fa(const std::string& username) override;

        std::vector<std::string> generate_backup_codes(const std::string& username) override;
        bool verify_backup_code(const std::string& username, const std::string& code) override;
        AuthenticationResult reset_2fa(const std::string& username) override;

        bool validate_configuration() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // Email-specific configuration
        void set_smtp_config(const std::string& server, std::uint16_t port,
                             const std::string& username, const std::string& password);
        void set_email_template(const std::string& subject, const std::string& body);

      private:
        std::string smtp_server_;
        std::uint16_t smtp_port_;
        std::string smtp_username_;
        std::string smtp_password_;
        std::string email_subject_ = "ScratchBird Verification Code";
        std::string email_template_ = "Your verification code is: {code}";

        class Impl;
        std::unique_ptr<Impl> pimpl_;

        // Helper methods
        bool send_email(const std::string& to_address, const std::string& subject,
                        const std::string& body);
        std::string generate_verification_code() const;
    };

    /**
     * Two-factor authentication manager
     */
    class TwoFactorAuthenticationManager
    {
      public:
        TwoFactorAuthenticationManager();
        ~TwoFactorAuthenticationManager();

        // Provider management
        bool register_provider(std::unique_ptr<TwoFactorProvider> provider);
        bool unregister_provider(const std::string& provider_name);
        std::vector<std::string> get_registered_providers() const;
        TwoFactorProvider* get_provider(const std::string& provider_name);
        TwoFactorProvider* get_provider(TwoFactorMethod method);

        // User 2FA management
        std::vector<TwoFactorMethod> get_user_enrolled_methods(const std::string& username) const;
        TwoFactorStatus get_user_2fa_status(const std::string& username) const;
        bool is_user_enrolled(const std::string& username) const;
        bool is_2fa_required(const std::string& username) const;

        // Enrollment process
        AuthenticationResult begin_enrollment(const std::string& username, TwoFactorMethod method,
                                              AuthenticationContext& context);
        AuthenticationResult complete_enrollment(const std::string& username,
                                                 TwoFactorMethod method,
                                                 const std::string& verification_data,
                                                 AuthenticationContext& context);
        AuthenticationResult cancel_enrollment(const std::string& username, TwoFactorMethod method);

        // Authentication process
        std::unique_ptr<AuthenticationChallenge>
        initiate_2fa_challenge(const std::string& username, AuthenticationContext& context,
                               TwoFactorMethod preferred_method = TwoFactorMethod::TOTP);
        AuthenticationResult verify_2fa_response(const AuthenticationChallenge& challenge,
                                                 const std::string& response,
                                                 AuthenticationContext& context);

        // Recovery and backup
        std::vector<std::string> generate_backup_codes(const std::string& username,
                                                       TwoFactorMethod method);
        bool verify_backup_code(const std::string& username, const std::string& code);
        AuthenticationResult reset_user_2fa(const std::string& username);

        // Administration
        AuthenticationResult disable_user_2fa(const std::string& username,
                                              TwoFactorMethod method = TwoFactorMethod::TOTP);
        AuthenticationResult enable_user_2fa(const std::string& username,
                                             TwoFactorMethod method = TwoFactorMethod::TOTP);
        AuthenticationResult lock_user_2fa(const std::string& username);
        AuthenticationResult unlock_user_2fa(const std::string& username);

        // Policy configuration
        void set_2fa_required_for_all(bool required)
        {
            require_2fa_for_all_ = required;
        }
        bool is_2fa_required_for_all() const
        {
            return require_2fa_for_all_;
        }

        void set_2fa_required_for_admins(bool required)
        {
            require_2fa_for_admins_ = required;
        }
        bool is_2fa_required_for_admins() const
        {
            return require_2fa_for_admins_;
        }

        void set_backup_codes_required(bool required)
        {
            require_backup_codes_ = required;
        }
        bool are_backup_codes_required() const
        {
            return require_backup_codes_;
        }

        void set_max_failed_attempts(std::uint32_t max_attempts)
        {
            max_failed_attempts_ = max_attempts;
        }
        std::uint32_t get_max_failed_attempts() const
        {
            return max_failed_attempts_;
        }

        void set_lockout_duration(std::chrono::minutes duration)
        {
            lockout_duration_ = duration;
        }
        std::chrono::minutes get_lockout_duration() const
        {
            return lockout_duration_;
        }

        // Statistics and monitoring
        struct TwoFactorStats {
            std::uint32_t enrolled_users = 0;
            std::uint32_t successful_verifications = 0;
            std::uint32_t failed_verifications = 0;
            std::uint32_t backup_codes_used = 0;
            std::uint32_t locked_users = 0;
            std::chrono::system_clock::time_point last_reset;
        };

        TwoFactorStats get_statistics() const;
        void reset_statistics();

      private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;

        // Policy configuration
        bool require_2fa_for_all_ = false;
        bool require_2fa_for_admins_ = true;
        bool require_backup_codes_ = true;
        std::uint32_t max_failed_attempts_ = 3;
        std::chrono::minutes lockout_duration_{30};

        // Helper methods
        void record_verification_attempt(const std::string& username, bool success);
        bool is_user_locked(const std::string& username) const;
        void lock_user(const std::string& username);
        void unlock_user(const std::string& username);
    };

    /**
     * Two-factor authentication provider for integration with main auth system
     */
    class TwoFactorAuthenticationProvider : public AuthenticationProvider
    {
      public:
        TwoFactorAuthenticationProvider(std::unique_ptr<TwoFactorAuthenticationManager> manager);
        ~TwoFactorAuthenticationProvider() override;

        // AuthenticationProvider interface
        std::string get_provider_name() const override
        {
            return "TwoFactor";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::TwoFactor;
        }
        std::vector<std::string> get_supported_capabilities() const override;

        AuthenticationResult authenticate(AuthenticationContext& context) override;

        bool supports_challenge_response() const override
        {
            return true;
        }
        std::unique_ptr<AuthenticationChallenge>
        create_challenge(const AuthenticationContext& context) override;
        AuthenticationResult validate_challenge_response(const AuthenticationChallenge& challenge,
                                                         AuthenticationContext& context) override;

        bool validate_configuration() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // 2FA-specific access
        TwoFactorAuthenticationManager* get_manager()
        {
            return manager_.get();
        }

      private:
        std::unique_ptr<TwoFactorAuthenticationManager> manager_;
    };

    // Utility functions
    std::string to_string(TwoFactorMethod method);
    std::string to_string(TwoFactorStatus status);
    TwoFactorMethod parse_two_factor_method(const std::string& method_str);
    TwoFactorStatus parse_two_factor_status(const std::string& status_str);

    // TOTP utility functions
    std::string generate_totp_secret_key();
    std::string generate_totp_code(const std::string& secret_key, std::uint64_t timestamp,
                                   std::uint32_t digits = 6, const std::string& algorithm = "SHA1");
    bool verify_totp_code(const std::string& secret_key, const std::string& code,
                          std::uint64_t timestamp, std::uint32_t window = 1,
                          std::uint32_t digits = 6, const std::string& algorithm = "SHA1");
    std::string generate_totp_qr_url(const std::string& secret_key, const std::string& account_name,
                                     const std::string& issuer = "ScratchBird");

} // namespace ScratchBird
