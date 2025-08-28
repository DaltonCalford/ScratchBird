#include "scratchbird/engine/trusted_auth.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef __linux__
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
// PAM headers are optional - may not be available on all systems
#ifdef HAVE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#endif
#endif

#ifdef _WIN32
#include <dsgetdc.h>
#include <lm.h>
#include <security.h>
#include <windows.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "secur32.lib")
#endif

namespace ScratchBird
{

#ifdef __linux__
// PAM Authenticator Implementation
#ifdef HAVE_PAM
    static int pam_conv_function(int num_msg, const struct pam_message** msg,
                                 struct pam_response** resp, void* appdata_ptr)
    {
        // For now, we don't support interactive authentication
        // This would need to be enhanced for production use
        return PAM_CONV_ERR;
    }
#endif

    PAMAuthenticator::PAMAuthenticator(const std::string& service_name)
        : service_name_(service_name)
    {
    }

    PAMAuthenticator::~PAMAuthenticator() = default;

    AuthenticationResult PAMAuthenticator::authenticate_user(const std::string& username,
                                                             const std::string& domain)
    {
        // For PAM authentication, we typically need interactive password prompt
        // This is a simplified version - full implementation would need proper PAM conversation

        // Check if user exists in system
        struct passwd* pwd = getpwnam(username.c_str());
        if (!pwd) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Check if user can login
        if (!can_user_login(username, domain)) {
            return AuthenticationResult::AccessDenied;
        }

        // For this implementation, we'll assume successful authentication
        // In a real implementation, this would do proper PAM authentication
        return AuthenticationResult::Success;
    }

    std::optional<OSUserInfo> PAMAuthenticator::get_user_info(const std::string& username,
                                                              const std::string& domain)
    {
        struct passwd* pwd = getpwnam(username.c_str());
        if (!pwd) {
            return std::nullopt;
        }

        OSUserInfo user_info;
        user_info.username = username;
        user_info.full_name = pwd->pw_gecos ? pwd->pw_gecos : "";
        user_info.home_directory = pwd->pw_dir ? pwd->pw_dir : "";
        user_info.shell = pwd->pw_shell ? pwd->pw_shell : "";
        user_info.domain = domain.empty() ? "local" : domain;
        user_info.groups = get_user_groups(username, domain);
        user_info.is_admin = is_user_admin(username, domain);
        user_info.auth_method = "PAM";
        user_info.auth_time = std::chrono::system_clock::now();

        return user_info;
    }

    std::vector<std::string> PAMAuthenticator::get_user_groups(const std::string& username,
                                                               const std::string& domain)
    {
        std::vector<std::string> groups;

        struct passwd* pwd = getpwnam(username.c_str());
        if (!pwd)
            return groups;

        // Get primary group
        struct group* grp = getgrgid(pwd->pw_gid);
        if (grp && grp->gr_name) {
            groups.push_back(grp->gr_name);
        }

        // Get supplementary groups
        gid_t user_groups[256];
        int ngroups = 256;

        if (getgrouplist(username.c_str(), pwd->pw_gid, user_groups, &ngroups) >= 0) {
            for (int i = 0; i < ngroups; ++i) {
                struct group* sup_grp = getgrgid(user_groups[i]);
                if (sup_grp && sup_grp->gr_name) {
                    std::string group_name = sup_grp->gr_name;
                    if (std::find(groups.begin(), groups.end(), group_name) == groups.end()) {
                        groups.push_back(group_name);
                    }
                }
            }
        }

        return groups;
    }

    bool PAMAuthenticator::is_user_in_group(const std::string& username, const std::string& group,
                                            const std::string& domain)
    {
        auto user_groups = get_user_groups(username, domain);
        return std::find(user_groups.begin(), user_groups.end(), group) != user_groups.end();
    }

    bool PAMAuthenticator::is_user_admin(const std::string& username, const std::string& domain)
    {
        for (const auto& admin_group : admin_groups_) {
            if (is_user_in_group(username, admin_group, domain)) {
                return true;
            }
        }
        return false;
    }

    bool PAMAuthenticator::can_user_login(const std::string& username, const std::string& domain)
    {
        struct passwd* pwd = getpwnam(username.c_str());
        if (!pwd)
            return false;

        // Check if shell allows login (not /bin/false, /usr/sbin/nologin, etc.)
        if (pwd->pw_shell) {
            std::string shell = pwd->pw_shell;
            if (shell.find("false") != std::string::npos ||
                shell.find("nologin") != std::string::npos) {
                return false;
            }
        }

        return true;
    }

    bool PAMAuthenticator::is_available() const
    {
        // Check if PAM is available on this system
        return true; // PAM should be available on most Linux systems
    }

    std::vector<std::string> PAMAuthenticator::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        // Check if service configuration exists
        std::string config_path = "/etc/pam.d/" + service_name_;
        std::ifstream config_file(config_path);
        if (!config_file.good()) {
            errors.push_back("PAM service configuration not found: " + config_path);
        }

        return errors;
    }

    OSUserInfo PAMAuthenticator::get_unix_user_info(const std::string& username)
    {
        OSUserInfo user_info;

        struct passwd* pwd = getpwnam(username.c_str());
        if (pwd) {
            user_info.username = username;
            user_info.full_name = pwd->pw_gecos ? pwd->pw_gecos : "";
            user_info.home_directory = pwd->pw_dir ? pwd->pw_dir : "";
            user_info.shell = pwd->pw_shell ? pwd->pw_shell : "";
            user_info.groups = get_unix_user_groups(username);
        }

        return user_info;
    }

    std::vector<std::string> PAMAuthenticator::get_unix_user_groups(const std::string& username)
    {
        return get_user_groups(username, "");
    }
#endif

#ifdef _WIN32
    // SSPI Authenticator Implementation
    class SSPIAuthenticator::Impl
    {
      public:
        Impl()
        {
            // Initialize SSPI
        }

        ~Impl()
        {
            // Cleanup SSPI
        }

        bool initialize()
        {
            return true;
        }

        AuthenticationResult authenticate_with_sspi(const std::string& username,
                                                    const std::string& domain)
        {
            // Implement SSPI authentication
            return AuthenticationResult::Success;
        }

        OSUserInfo get_windows_user_info(const std::string& username, const std::string& domain)
        {
            OSUserInfo user_info;
            user_info.username = username;
            user_info.domain = domain;
            user_info.auth_method = "SSPI";

            // Get Windows user information using NetUserGetInfo
            std::wstring wide_username(username.begin(), username.end());
            std::wstring wide_domain(domain.begin(), domain.end());

            LPUSER_INFO_3 user_info_3 = nullptr;
            NET_API_STATUS status = NetUserGetInfo(domain.empty() ? nullptr : wide_domain.c_str(),
                                                   wide_username.c_str(), 3, (LPBYTE*)&user_info_3);

            if (status == NERR_Success && user_info_3) {
                // Convert wide strings to regular strings
                if (user_info_3->usri3_full_name) {
                    std::wstring wide_full_name = user_info_3->usri3_full_name;
                    user_info.full_name = std::string(wide_full_name.begin(), wide_full_name.end());
                }

                if (user_info_3->usri3_home_dir) {
                    std::wstring wide_home = user_info_3->usri3_home_dir;
                    user_info.home_directory = std::string(wide_home.begin(), wide_home.end());
                }

                user_info.is_admin = (user_info_3->usri3_priv == USER_PRIV_ADMIN);

                NetApiBufferFree(user_info_3);
            }

            return user_info;
        }

        std::vector<std::string> get_user_groups_windows(const std::string& username,
                                                         const std::string& domain)
        {
            std::vector<std::string> groups;

            std::wstring wide_username(username.begin(), username.end());
            std::wstring wide_domain(domain.begin(), domain.end());

            LPGROUP_USERS_INFO_0 group_info = nullptr;
            DWORD entries_read = 0, total_entries = 0;

            NET_API_STATUS status = NetUserGetGroups(
                domain.empty() ? nullptr : wide_domain.c_str(), wide_username.c_str(), 0,
                (LPBYTE*)&group_info, MAX_PREFERRED_LENGTH, &entries_read, &total_entries);

            if (status == NERR_Success && group_info) {
                for (DWORD i = 0; i < entries_read; ++i) {
                    if (group_info[i].grui0_name) {
                        std::wstring wide_group = group_info[i].grui0_name;
                        groups.push_back(std::string(wide_group.begin(), wide_group.end()));
                    }
                }

                NetApiBufferFree(group_info);
            }

            return groups;
        }

        std::vector<std::string> get_domain_controllers(const std::string& domain)
        {
            std::vector<std::string> dcs;

            std::wstring wide_domain(domain.begin(), domain.end());
            PDOMAIN_CONTROLLER_INFO dc_info = nullptr;

            DWORD result = DsGetDcName(nullptr, wide_domain.c_str(), nullptr, nullptr,
                                       DS_RETURN_DNS_NAME, &dc_info);

            if (result == ERROR_SUCCESS && dc_info) {
                if (dc_info->DomainControllerName) {
                    std::wstring wide_dc = dc_info->DomainControllerName;
                    dcs.push_back(std::string(wide_dc.begin(), wide_dc.end()));
                }

                NetApiBufferFree(dc_info);
            }

            return dcs;
        }
    };

    SSPIAuthenticator::SSPIAuthenticator() : pimpl_(std::make_unique<Impl>())
    {
        pimpl_->initialize();
    }

    SSPIAuthenticator::~SSPIAuthenticator() = default;

    AuthenticationResult SSPIAuthenticator::authenticate_user(const std::string& username,
                                                              const std::string& domain)
    {
        return pimpl_->authenticate_with_sspi(username, domain);
    }

    std::optional<OSUserInfo> SSPIAuthenticator::get_user_info(const std::string& username,
                                                               const std::string& domain)
    {
        return pimpl_->get_windows_user_info(username, domain);
    }

    std::vector<std::string> SSPIAuthenticator::get_user_groups(const std::string& username,
                                                                const std::string& domain)
    {
        return pimpl_->get_user_groups_windows(username, domain);
    }

    bool SSPIAuthenticator::is_user_in_group(const std::string& username, const std::string& group,
                                             const std::string& domain)
    {
        auto user_groups = get_user_groups(username, domain);
        return std::find(user_groups.begin(), user_groups.end(), group) != user_groups.end();
    }

    bool SSPIAuthenticator::is_user_admin(const std::string& username, const std::string& domain)
    {
        // Check for Windows admin groups
        return is_user_in_group(username, "Administrators", domain) ||
               is_user_in_group(username, "Domain Admins", domain) ||
               is_user_in_group(username, "Enterprise Admins", domain);
    }

    bool SSPIAuthenticator::can_user_login(const std::string& username, const std::string& domain)
    {
        // Check Windows user account flags
        auto user_info = get_user_info(username, domain);
        return user_info.has_value(); // Simplified check
    }

    std::vector<std::string> SSPIAuthenticator::get_supported_domains() const
    {
        // This would enumerate available domains
        return {"."}; // Local computer
    }

    bool SSPIAuthenticator::is_available() const
    {
        return true; // SSPI should be available on Windows
    }

    std::vector<std::string> SSPIAuthenticator::get_configuration_errors() const
    {
        // Check SSPI availability and configuration
        return {};
    }

    std::vector<std::string> SSPIAuthenticator::get_domain_controllers(const std::string& domain)
    {
        return pimpl_->get_domain_controllers(domain);
    }
#endif

    // Kerberos Authenticator Implementation
    KerberosAuthenticator::KerberosAuthenticator(const std::string& realm, const std::string& kdc)
        : realm_(realm), kdc_(kdc)
    {
    }

    KerberosAuthenticator::~KerberosAuthenticator() = default;

    AuthenticationResult KerberosAuthenticator::authenticate_user(const std::string& username,
                                                                  const std::string& domain)
    {
        // Kerberos authentication would require krb5 library
        // For now, this is a placeholder implementation
        if (!is_available()) {
            return AuthenticationResult::InternalError;
        }

        // In a real implementation, this would:
        // 1. Acquire Kerberos credentials
        // 2. Validate the ticket
        // 3. Extract user information

        return AuthenticationResult::Success;
    }

    std::optional<OSUserInfo> KerberosAuthenticator::get_user_info(const std::string& username,
                                                                   const std::string& domain)
    {
        OSUserInfo user_info;
        user_info.username = username;
        user_info.domain = domain.empty() ? realm_ : domain;
        user_info.auth_method = "Kerberos";
        user_info.auth_time = std::chrono::system_clock::now();

        // Get ticket info
        user_info.ticket_info = get_ticket_info(username, user_info.domain);

        return user_info;
    }

    std::vector<std::string> KerberosAuthenticator::get_user_groups(const std::string& username,
                                                                    const std::string& domain)
    {
        // Kerberos groups would be retrieved from tickets or LDAP
        return {};
    }

    bool KerberosAuthenticator::is_user_in_group(const std::string& username,
                                                 const std::string& group,
                                                 const std::string& domain)
    {
        auto user_groups = get_user_groups(username, domain);
        return std::find(user_groups.begin(), user_groups.end(), group) != user_groups.end();
    }

    bool KerberosAuthenticator::is_user_admin(const std::string& username,
                                              const std::string& domain)
    {
        // Check for admin groups in Kerberos realm
        return false; // Placeholder
    }

    bool KerberosAuthenticator::can_user_login(const std::string& username,
                                               const std::string& domain)
    {
        // Check if user has valid Kerberos credentials
        return true; // Placeholder
    }

    bool KerberosAuthenticator::is_available() const
    {
        // Check if Kerberos libraries are available
        return false; // For now, not implemented
    }

    std::vector<std::string> KerberosAuthenticator::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (realm_.empty()) {
            errors.push_back("Kerberos realm not configured");
        }

        if (!is_available()) {
            errors.push_back("Kerberos libraries not available");
        }

        return errors;
    }

    std::string KerberosAuthenticator::get_ticket_info(const std::string& username,
                                                       const std::string& domain)
    {
        // Get Kerberos ticket information
        return ""; // Placeholder
    }

    // LDAP Authenticator Implementation
    class LDAPAuthenticator::Impl
    {
      public:
        explicit Impl(const LDAPConfig& config) : config_(config) {}

        AuthenticationResult authenticate_ldap(const std::string& username,
                                               const std::string& password)
        {
            // LDAP authentication would require ldap library
            // For now, this is a placeholder implementation
            return AuthenticationResult::Success;
        }

        OSUserInfo get_ldap_user_info(const std::string& username)
        {
            OSUserInfo user_info;
            user_info.username = username;
            user_info.auth_method = "LDAP";
            user_info.auth_time = std::chrono::system_clock::now();

            // LDAP search for user information
            // This would use ldap_search_ext_s and related functions

            return user_info;
        }

        std::vector<std::string> get_ldap_user_groups(const std::string& username)
        {
            std::vector<std::string> groups;

            // LDAP search for user group memberships
            // This would search for groups containing the user DN

            return groups;
        }

        bool test_ldap_connection()
        {
            // Test LDAP server connectivity
            return false; // Placeholder
        }

      private:
        LDAPConfig config_;
    };

    LDAPAuthenticator::LDAPAuthenticator(const LDAPConfig& config)
        : config_(config), pimpl_(std::make_unique<Impl>(config))
    {
    }

    LDAPAuthenticator::~LDAPAuthenticator() = default;

    AuthenticationResult LDAPAuthenticator::authenticate_user(const std::string& username,
                                                              const std::string& domain)
    {
        return pimpl_->authenticate_ldap(username, ""); // Would need password from context
    }

    std::optional<OSUserInfo> LDAPAuthenticator::get_user_info(const std::string& username,
                                                               const std::string& domain)
    {
        return pimpl_->get_ldap_user_info(username);
    }

    std::vector<std::string> LDAPAuthenticator::get_user_groups(const std::string& username,
                                                                const std::string& domain)
    {
        return pimpl_->get_ldap_user_groups(username);
    }

    bool LDAPAuthenticator::is_user_in_group(const std::string& username, const std::string& group,
                                             const std::string& domain)
    {
        auto user_groups = get_user_groups(username, domain);
        return std::find(user_groups.begin(), user_groups.end(), group) != user_groups.end();
    }

    bool LDAPAuthenticator::is_user_admin(const std::string& username, const std::string& domain)
    {
        // Check for admin groups in LDAP
        return is_user_in_group(username, "Administrators", domain) ||
               is_user_in_group(username, "Domain Admins", domain);
    }

    bool LDAPAuthenticator::can_user_login(const std::string& username, const std::string& domain)
    {
        // Check LDAP user account status
        return true; // Placeholder
    }

    bool LDAPAuthenticator::is_available() const
    {
        return !config_.server_url.empty();
    }

    std::vector<std::string> LDAPAuthenticator::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (config_.server_url.empty()) {
            errors.push_back("LDAP server URL not configured");
        }

        if (config_.base_dn.empty()) {
            errors.push_back("LDAP base DN not configured");
        }

        return errors;
    }

    bool LDAPAuthenticator::test_connection()
    {
        return pimpl_->test_ldap_connection();
    }

    // TrustedOSAuthenticationProvider Implementation
    TrustedOSAuthenticationProvider::TrustedOSAuthenticationProvider(
        std::unique_ptr<TrustedOSAuthenticator> authenticator)
        : authenticator_(std::move(authenticator))
    {
    }

    TrustedOSAuthenticationProvider::~TrustedOSAuthenticationProvider() = default;

    std::vector<std::string> TrustedOSAuthenticationProvider::get_supported_capabilities() const
    {
        std::vector<std::string> capabilities = {"trusted_os_authentication", "user_info_retrieval",
                                                 "group_membership", "admin_check"};

        if (authenticator_->supports_domain_authentication()) {
            capabilities.push_back("domain_authentication");
        }

        if (authenticator_->supports_service_accounts()) {
            capabilities.push_back("service_account_support");
        }

        return capabilities;
    }

    AuthenticationResult
    TrustedOSAuthenticationProvider::authenticate(AuthenticationContext& context)
    {
        const std::string& username = context.get_username();

        // Extract domain from username if present (domain\username or username@domain)
        std::string user = username;
        std::string domain;

        size_t backslash_pos = username.find('\\');
        size_t at_pos = username.find('@');

        if (backslash_pos != std::string::npos) {
            domain = username.substr(0, backslash_pos);
            user = username.substr(backslash_pos + 1);
        } else if (at_pos != std::string::npos) {
            user = username.substr(0, at_pos);
            domain = username.substr(at_pos + 1);
        }

        // Authenticate with OS
        AuthenticationResult result = authenticator_->authenticate_user(user, domain);

        if (result == AuthenticationResult::Success) {
            // Get user information and populate context
            auto user_info_opt = authenticator_->get_user_info(user, domain);
            if (user_info_opt) {
                populate_security_context(context, *user_info_opt);
            }
        }

        return result;
    }

    bool TrustedOSAuthenticationProvider::validate_configuration() const
    {
        return authenticator_ && authenticator_->is_available();
    }

    std::vector<std::string> TrustedOSAuthenticationProvider::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (!authenticator_) {
            errors.push_back("No OS authenticator configured");
            return errors;
        }

        if (!authenticator_->is_available()) {
            errors.push_back("OS authenticator not available: " +
                             authenticator_->get_auth_type_name());
        }

        auto auth_errors = authenticator_->get_configuration_errors();
        errors.insert(errors.end(), auth_errors.begin(), auth_errors.end());

        return errors;
    }

    std::optional<OSUserInfo>
    TrustedOSAuthenticationProvider::get_os_user_info(const std::string& username,
                                                      const std::string& domain)
    {
        return authenticator_->get_user_info(username, domain);
    }

    std::vector<std::string>
    TrustedOSAuthenticationProvider::get_user_groups(const std::string& username,
                                                     const std::string& domain)
    {
        return authenticator_->get_user_groups(username, domain);
    }

    bool TrustedOSAuthenticationProvider::is_user_admin(const std::string& username,
                                                        const std::string& domain)
    {
        return authenticator_->is_user_admin(username, domain);
    }

    TrustedAuthType TrustedOSAuthenticationProvider::get_auth_type() const
    {
        return authenticator_->get_auth_type();
    }

    std::string TrustedOSAuthenticationProvider::get_auth_type_name() const
    {
        return authenticator_->get_auth_type_name();
    }

    bool TrustedOSAuthenticationProvider::supports_domain_authentication() const
    {
        return authenticator_->supports_domain_authentication();
    }

    std::vector<std::string> TrustedOSAuthenticationProvider::get_supported_domains() const
    {
        return authenticator_->get_supported_domains();
    }

    void TrustedOSAuthenticationProvider::populate_security_context(AuthenticationContext& context,
                                                                    const OSUserInfo& user_info)
    {

        context.set_credential("full_name", user_info.full_name);
        context.set_credential("domain", user_info.domain);
        context.set_credential("home_directory", user_info.home_directory);
        context.set_credential("auth_method", user_info.auth_method);
        context.set_credential("is_admin", user_info.is_admin ? "true" : "false");

        // Set groups as comma-separated list
        std::stringstream groups_ss;
        for (size_t i = 0; i < user_info.groups.size(); ++i) {
            if (i > 0)
                groups_ss << ",";
            groups_ss << user_info.groups[i];
        }
        context.set_credential("groups", groups_ss.str());

        if (!user_info.ticket_info.empty()) {
            context.set_credential("ticket_info", user_info.ticket_info);
        }
    }

    // TrustedOSAuthenticatorFactory Implementation
    std::unique_ptr<TrustedOSAuthenticator> TrustedOSAuthenticatorFactory::create_auto()
    {
        TrustedAuthType default_type = get_default_auth_type();

        switch (default_type) {
#ifdef __linux__
        case TrustedAuthType::PAM:
            return create_pam();
#endif
#ifdef _WIN32
        case TrustedAuthType::SSPI:
            return create_sspi();
#endif
        default:
            return nullptr;
        }
    }

    std::unique_ptr<TrustedOSAuthenticator>
    TrustedOSAuthenticatorFactory::create_pam(const std::string& service_name)
    {
#ifdef __linux__
        return std::make_unique<PAMAuthenticator>(service_name);
#else
        return nullptr;
#endif
    }

    std::unique_ptr<TrustedOSAuthenticator> TrustedOSAuthenticatorFactory::create_sspi()
    {
#ifdef _WIN32
        return std::make_unique<SSPIAuthenticator>();
#else
        return nullptr;
#endif
    }

    std::unique_ptr<TrustedOSAuthenticator>
    TrustedOSAuthenticatorFactory::create_kerberos(const std::string& realm, const std::string& kdc)
    {
        return std::make_unique<KerberosAuthenticator>(realm, kdc);
    }

    std::unique_ptr<TrustedOSAuthenticator>
    TrustedOSAuthenticatorFactory::create_ldap(const LDAPAuthenticator::LDAPConfig& config)
    {
        return std::make_unique<LDAPAuthenticator>(config);
    }

    TrustedAuthType TrustedOSAuthenticatorFactory::get_default_auth_type()
    {
#ifdef __linux__
        return TrustedAuthType::PAM;
#elif defined(_WIN32)
        return TrustedAuthType::SSPI;
#else
        return TrustedAuthType::Auto;
#endif
    }

    std::vector<TrustedAuthType> TrustedOSAuthenticatorFactory::get_available_auth_types()
    {
        std::vector<TrustedAuthType> types;

#ifdef __linux__
        types.push_back(TrustedAuthType::PAM);
#endif
#ifdef _WIN32
        types.push_back(TrustedAuthType::SSPI);
#endif

        types.push_back(TrustedAuthType::Kerberos);
        types.push_back(TrustedAuthType::LDAP);

        return types;
    }

    bool TrustedOSAuthenticatorFactory::is_auth_type_available(TrustedAuthType type)
    {
        auto available_types = get_available_auth_types();
        return std::find(available_types.begin(), available_types.end(), type) !=
               available_types.end();
    }

} // namespace ScratchBird
