#pragma once

#include "scratchbird/engine/authentication.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ScratchBird
{

    /**
     * Trusted OS authentication types
     */
    enum class TrustedAuthType {
        Auto,     // Automatically detect best method
        PAM,      // Linux/Unix PAM authentication
        SSPI,     // Windows SSPI authentication
        Kerberos, // Kerberos authentication
        LDAP,     // LDAP directory authentication
        Custom    // Custom OS integration
    };

    /**
     * OS user information
     */
    struct OSUserInfo {
        std::string username;
        std::string full_name;
        std::string domain; // Windows domain or Unix realm
        std::string home_directory;
        std::string shell; // Unix shell
        std::vector<std::string> groups;
        std::vector<std::string> roles;
        bool is_admin = false;
        bool is_service_account = false;

        // Authentication details
        std::string auth_method; // How user was authenticated
        std::string ticket_info; // Kerberos ticket or similar
        std::chrono::system_clock::time_point auth_time;
        std::chrono::system_clock::time_point ticket_expiry;
    };

    /**
     * Abstract base class for OS authentication
     */
    class TrustedOSAuthenticator
    {
      public:
        TrustedOSAuthenticator() = default;
        virtual ~TrustedOSAuthenticator() = default;

        // Core authentication
        virtual AuthenticationResult authenticate_user(const std::string& username,
                                                       const std::string& domain = "") = 0;

        // User information
        virtual std::optional<OSUserInfo> get_user_info(const std::string& username,
                                                        const std::string& domain = "") = 0;

        // Group membership
        virtual std::vector<std::string> get_user_groups(const std::string& username,
                                                         const std::string& domain = "") = 0;
        virtual bool is_user_in_group(const std::string& username, const std::string& group,
                                      const std::string& domain = "") = 0;

        // Administrative checks
        virtual bool is_user_admin(const std::string& username, const std::string& domain = "") = 0;
        virtual bool can_user_login(const std::string& username,
                                    const std::string& domain = "") = 0;

        // Configuration and capabilities
        virtual TrustedAuthType get_auth_type() const = 0;
        virtual std::string get_auth_type_name() const = 0;
        virtual std::vector<std::string> get_supported_domains() const
        {
            return {};
        }
        virtual bool supports_domain_authentication() const
        {
            return false;
        }
        virtual bool supports_service_accounts() const
        {
            return false;
        }

        // Validation
        virtual bool is_available() const = 0;
        virtual std::vector<std::string> get_configuration_errors() const
        {
            return {};
        }
    };

#ifdef __linux__
    /**
     * PAM (Pluggable Authentication Modules) authenticator for Linux/Unix
     */
    class PAMAuthenticator : public TrustedOSAuthenticator
    {
      public:
        explicit PAMAuthenticator(const std::string& service_name = "scratchbird");
        ~PAMAuthenticator() override;

        AuthenticationResult authenticate_user(const std::string& username,
                                               const std::string& domain = "") override;

        std::optional<OSUserInfo> get_user_info(const std::string& username,
                                                const std::string& domain = "") override;

        std::vector<std::string> get_user_groups(const std::string& username,
                                                 const std::string& domain = "") override;
        bool is_user_in_group(const std::string& username, const std::string& group,
                              const std::string& domain = "") override;

        bool is_user_admin(const std::string& username, const std::string& domain = "") override;
        bool can_user_login(const std::string& username, const std::string& domain = "") override;

        TrustedAuthType get_auth_type() const override
        {
            return TrustedAuthType::PAM;
        }
        std::string get_auth_type_name() const override
        {
            return "PAM";
        }

        bool is_available() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // PAM-specific configuration
        void set_service_name(const std::string& service_name)
        {
            service_name_ = service_name;
        }
        const std::string& get_service_name() const
        {
            return service_name_;
        }

        void set_admin_groups(const std::vector<std::string>& groups)
        {
            admin_groups_ = groups;
        }
        const std::vector<std::string>& get_admin_groups() const
        {
            return admin_groups_;
        }

      private:
        std::string service_name_;
        std::vector<std::string> admin_groups_{"wheel", "sudo", "admin"};

        // Helper methods
        bool authenticate_with_pam(const std::string& username, const std::string& password);
        OSUserInfo get_unix_user_info(const std::string& username);
        std::vector<std::string> get_unix_user_groups(const std::string& username);
    };
#endif

#ifdef _WIN32
    /**
     * SSPI (Security Support Provider Interface) authenticator for Windows
     */
    class SSPIAuthenticator : public TrustedOSAuthenticator
    {
      public:
        SSPIAuthenticator();
        ~SSPIAuthenticator() override;

        AuthenticationResult authenticate_user(const std::string& username,
                                               const std::string& domain = "") override;

        std::optional<OSUserInfo> get_user_info(const std::string& username,
                                                const std::string& domain = "") override;

        std::vector<std::string> get_user_groups(const std::string& username,
                                                 const std::string& domain = "") override;
        bool is_user_in_group(const std::string& username, const std::string& group,
                              const std::string& domain = "") override;

        bool is_user_admin(const std::string& username, const std::string& domain = "") override;
        bool can_user_login(const std::string& username, const std::string& domain = "") override;

        TrustedAuthType get_auth_type() const override
        {
            return TrustedAuthType::SSPI;
        }
        std::string get_auth_type_name() const override
        {
            return "SSPI";
        }
        std::vector<std::string> get_supported_domains() const override;
        bool supports_domain_authentication() const override
        {
            return true;
        }
        bool supports_service_accounts() const override
        {
            return true;
        }

        bool is_available() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // Windows-specific methods
        bool authenticate_with_kerberos(const std::string& username, const std::string& domain);
        bool authenticate_with_ntlm(const std::string& username, const std::string& domain);
        std::vector<std::string> get_domain_controllers(const std::string& domain);

      private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };
#endif

    /**
     * Kerberos authenticator (cross-platform)
     */
    class KerberosAuthenticator : public TrustedOSAuthenticator
    {
      public:
        KerberosAuthenticator(const std::string& realm = "", const std::string& kdc = "");
        ~KerberosAuthenticator() override;

        AuthenticationResult authenticate_user(const std::string& username,
                                               const std::string& domain = "") override;

        std::optional<OSUserInfo> get_user_info(const std::string& username,
                                                const std::string& domain = "") override;

        std::vector<std::string> get_user_groups(const std::string& username,
                                                 const std::string& domain = "") override;
        bool is_user_in_group(const std::string& username, const std::string& group,
                              const std::string& domain = "") override;

        bool is_user_admin(const std::string& username, const std::string& domain = "") override;
        bool can_user_login(const std::string& username, const std::string& domain = "") override;

        TrustedAuthType get_auth_type() const override
        {
            return TrustedAuthType::Kerberos;
        }
        std::string get_auth_type_name() const override
        {
            return "Kerberos";
        }
        std::vector<std::string> get_supported_domains() const override
        {
            return {realm_};
        }
        bool supports_domain_authentication() const override
        {
            return true;
        }

        bool is_available() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // Kerberos-specific methods
        void set_realm(const std::string& realm)
        {
            realm_ = realm;
        }
        const std::string& get_realm() const
        {
            return realm_;
        }

        void set_kdc(const std::string& kdc)
        {
            kdc_ = kdc;
        }
        const std::string& get_kdc() const
        {
            return kdc_;
        }

        bool validate_ticket(const std::string& ticket);
        std::string get_ticket_info(const std::string& username, const std::string& domain);

      private:
        std::string realm_;
        std::string kdc_;

        // Helper methods
        bool initialize_krb5();
        void cleanup_krb5();
        AuthenticationResult authenticate_with_ticket(const std::string& username,
                                                      const std::string& ticket);
    };

    /**
     * LDAP authenticator for directory services
     */
    class LDAPAuthenticator : public TrustedOSAuthenticator
    {
      public:
        struct LDAPConfig {
            std::string server_url;
            std::string base_dn;
            std::string bind_dn;
            std::string bind_password;
            std::string user_search_base;
            std::string user_search_filter = "(uid={username})";
            std::string group_search_base;
            std::string group_search_filter = "(member={user_dn})";
            bool use_tls = true;
            bool verify_certificate = true;
            std::uint32_t timeout_seconds = 30;
        };

        explicit LDAPAuthenticator(const LDAPConfig& config);
        ~LDAPAuthenticator() override;

        AuthenticationResult authenticate_user(const std::string& username,
                                               const std::string& domain = "") override;

        std::optional<OSUserInfo> get_user_info(const std::string& username,
                                                const std::string& domain = "") override;

        std::vector<std::string> get_user_groups(const std::string& username,
                                                 const std::string& domain = "") override;
        bool is_user_in_group(const std::string& username, const std::string& group,
                              const std::string& domain = "") override;

        bool is_user_admin(const std::string& username, const std::string& domain = "") override;
        bool can_user_login(const std::string& username, const std::string& domain = "") override;

        TrustedAuthType get_auth_type() const override
        {
            return TrustedAuthType::LDAP;
        }
        std::string get_auth_type_name() const override
        {
            return "LDAP";
        }

        bool is_available() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // LDAP-specific methods
        void set_config(const LDAPConfig& config)
        {
            config_ = config;
        }
        const LDAPConfig& get_config() const
        {
            return config_;
        }

        bool test_connection();
        std::vector<std::string> search_users(const std::string& pattern);

      private:
        LDAPConfig config_;

        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

    /**
     * Trusted OS authentication provider
     */
    class TrustedOSAuthenticationProvider : public AuthenticationProvider
    {
      public:
        TrustedOSAuthenticationProvider(std::unique_ptr<TrustedOSAuthenticator> authenticator);
        ~TrustedOSAuthenticationProvider() override;

        // AuthenticationProvider interface
        std::string get_provider_name() const override
        {
            return "TrustedOS";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::TrustedOS;
        }
        std::vector<std::string> get_supported_capabilities() const override;

        AuthenticationResult authenticate(AuthenticationContext& context) override;

        bool supports_challenge_response() const override
        {
            return false;
        }

        // Account management (read-only for OS accounts)
        bool supports_account_management() const override
        {
            return false;
        }

        bool validate_configuration() const override;
        std::vector<std::string> get_configuration_errors() const override;

        // OS-specific operations
        std::optional<OSUserInfo> get_os_user_info(const std::string& username,
                                                   const std::string& domain = "");
        std::vector<std::string> get_user_groups(const std::string& username,
                                                 const std::string& domain = "");
        bool is_user_admin(const std::string& username, const std::string& domain = "");

        // Configuration
        TrustedOSAuthenticator* get_authenticator()
        {
            return authenticator_.get();
        }
        TrustedAuthType get_auth_type() const;
        std::string get_auth_type_name() const;

        // Domain support
        bool supports_domain_authentication() const;
        std::vector<std::string> get_supported_domains() const;

      private:
        std::unique_ptr<TrustedOSAuthenticator> authenticator_;

        // Helper methods
        void populate_security_context(AuthenticationContext& context, const OSUserInfo& user_info);
    };

    /**
     * Factory for creating appropriate trusted OS authenticators
     */
    class TrustedOSAuthenticatorFactory
    {
      public:
        // Auto-detect best authenticator for current platform
        static std::unique_ptr<TrustedOSAuthenticator> create_auto();

        // Create specific authenticator types
        static std::unique_ptr<TrustedOSAuthenticator>
        create_pam(const std::string& service_name = "scratchbird");
        static std::unique_ptr<TrustedOSAuthenticator> create_sspi();
        static std::unique_ptr<TrustedOSAuthenticator>
        create_kerberos(const std::string& realm = "", const std::string& kdc = "");
        static std::unique_ptr<TrustedOSAuthenticator>
        create_ldap(const LDAPAuthenticator::LDAPConfig& config);

        // Platform detection
        static TrustedAuthType get_default_auth_type();
        static std::vector<TrustedAuthType> get_available_auth_types();
        static bool is_auth_type_available(TrustedAuthType type);
    };

} // namespace ScratchBird
