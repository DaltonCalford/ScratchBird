#include "scratchbird/engine/trusted_auth.h"

#include <gtest/gtest.h>
#include <memory>

using namespace ScratchBird;

class TrustedAuthTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Test setup will vary by platform
    }

    void TearDown() override
    {
        // Test cleanup
    }
};

// OSUserInfo Tests
TEST_F(TrustedAuthTest, OSUserInfo_BasicOperations)
{
    OSUserInfo user_info;

    user_info.username = "testuser";
    user_info.full_name = "Test User";
    user_info.domain = "testdomain";
    user_info.home_directory = "/home/testuser";
    user_info.shell = "/bin/bash";
    user_info.groups = {"users", "admin", "developers"};
    user_info.roles = {"user", "administrator"};
    user_info.is_admin = true;
    user_info.is_service_account = false;
    user_info.auth_method = "PAM";
    user_info.auth_time = std::chrono::system_clock::now();

    EXPECT_EQ(user_info.username, "testuser");
    EXPECT_EQ(user_info.full_name, "Test User");
    EXPECT_EQ(user_info.domain, "testdomain");
    EXPECT_EQ(user_info.home_directory, "/home/testuser");
    EXPECT_EQ(user_info.shell, "/bin/bash");
    EXPECT_EQ(user_info.groups.size(), 3);
    EXPECT_EQ(user_info.roles.size(), 2);
    EXPECT_TRUE(user_info.is_admin);
    EXPECT_FALSE(user_info.is_service_account);
    EXPECT_EQ(user_info.auth_method, "PAM");

    // Test group membership
    auto it = std::find(user_info.groups.begin(), user_info.groups.end(), "admin");
    EXPECT_NE(it, user_info.groups.end());

    it = std::find(user_info.groups.begin(), user_info.groups.end(), "nonexistent");
    EXPECT_EQ(it, user_info.groups.end());
}

// TrustedOSAuthenticatorFactory Tests
TEST_F(TrustedAuthTest, TrustedOSAuthenticatorFactory_PlatformDetection)
{
    // Test platform detection
    TrustedAuthType default_type = TrustedOSAuthenticatorFactory::get_default_auth_type();

#ifdef __linux__
    EXPECT_EQ(default_type, TrustedAuthType::PAM);
#elif defined(_WIN32)
    EXPECT_EQ(default_type, TrustedAuthType::SSPI);
#else
    EXPECT_EQ(default_type, TrustedAuthType::Auto);
#endif

    // Test available auth types
    auto available_types = TrustedOSAuthenticatorFactory::get_available_auth_types();
    EXPECT_FALSE(available_types.empty());

    // Verify platform-specific types are available
#ifdef __linux__
    auto it = std::find(available_types.begin(), available_types.end(), TrustedAuthType::PAM);
    EXPECT_NE(it, available_types.end());
#endif

#ifdef _WIN32
    auto it = std::find(available_types.begin(), available_types.end(), TrustedAuthType::SSPI);
    EXPECT_NE(it, available_types.end());
#endif

    // Kerberos and LDAP should always be in available types
    auto kerberos_it =
        std::find(available_types.begin(), available_types.end(), TrustedAuthType::Kerberos);
    EXPECT_NE(kerberos_it, available_types.end());

    auto ldap_it = std::find(available_types.begin(), available_types.end(), TrustedAuthType::LDAP);
    EXPECT_NE(ldap_it, available_types.end());
}

TEST_F(TrustedAuthTest, TrustedOSAuthenticatorFactory_AuthTypeAvailability)
{
    // Test individual auth type availability
#ifdef __linux__
    EXPECT_TRUE(TrustedOSAuthenticatorFactory::is_auth_type_available(TrustedAuthType::PAM));
#endif

#ifdef _WIN32
    EXPECT_TRUE(TrustedOSAuthenticatorFactory::is_auth_type_available(TrustedAuthType::SSPI));
#endif

    // These should always be available (even if not fully functional in tests)
    EXPECT_TRUE(TrustedOSAuthenticatorFactory::is_auth_type_available(TrustedAuthType::Kerberos));
    EXPECT_TRUE(TrustedOSAuthenticatorFactory::is_auth_type_available(TrustedAuthType::LDAP));
}

TEST_F(TrustedAuthTest, TrustedOSAuthenticatorFactory_AutoCreation)
{
    // Test auto-creation of authenticator
    auto authenticator = TrustedOSAuthenticatorFactory::create_auto();

    if (authenticator) {
        EXPECT_FALSE(authenticator->get_provider_name().empty());
        EXPECT_FALSE(authenticator->get_auth_type_name().empty());

        // Basic interface tests
        TrustedAuthType auth_type = authenticator->get_auth_type();
        EXPECT_NE(auth_type, TrustedAuthType::Custom);

        // Test capabilities
        auto domains = authenticator->get_supported_domains();
        // Domains list can be empty or non-empty, both are valid

        bool supports_domains = authenticator->supports_domain_authentication();
        bool supports_service_accounts = authenticator->supports_service_accounts();
        // Both should be boolean values (no specific expectation)

        // Test availability check
        bool is_available = authenticator->is_available();
        // This may be true or false depending on system configuration

        auto config_errors = authenticator->get_configuration_errors();
        // Errors list can be empty or non-empty

        SUCCEED(); // If we get here without crashes, the factory works
    } else {
        // No authenticator available - this is valid on some systems
        SUCCEED();
    }
}

#ifdef __linux__
TEST_F(TrustedAuthTest, TrustedOSAuthenticatorFactory_PAMCreation)
{
    auto authenticator = TrustedOSAuthenticatorFactory::create_pam("scratchbird_test");
    ASSERT_NE(authenticator, nullptr);

    EXPECT_EQ(authenticator->get_auth_type(), TrustedAuthType::PAM);
    EXPECT_EQ(authenticator->get_auth_type_name(), "PAM");
    EXPECT_FALSE(authenticator->supports_domain_authentication());

    // Test PAM-specific functionality
    auto pam_auth = dynamic_cast<PAMAuthenticator*>(authenticator.get());
    ASSERT_NE(pam_auth, nullptr);

    EXPECT_EQ(pam_auth->get_service_name(), "scratchbird_test");

    // Test admin groups
    auto admin_groups = pam_auth->get_admin_groups();
    EXPECT_FALSE(admin_groups.empty());

    // Common admin groups should be present
    auto it = std::find(admin_groups.begin(), admin_groups.end(), "wheel");
    bool has_wheel = (it != admin_groups.end());

    it = std::find(admin_groups.begin(), admin_groups.end(), "sudo");
    bool has_sudo = (it != admin_groups.end());

    it = std::find(admin_groups.begin(), admin_groups.end(), "admin");
    bool has_admin = (it != admin_groups.end());

    // At least one common admin group should be configured
    EXPECT_TRUE(has_wheel || has_sudo || has_admin);
}

TEST_F(TrustedAuthTest, PAMAuthenticator_UserInformation)
{
    auto authenticator = TrustedOSAuthenticatorFactory::create_pam();
    ASSERT_NE(authenticator, nullptr);

    // Test with root user (should exist on all Unix systems)
    auto user_info = authenticator->get_user_info("root");
    if (user_info.has_value()) {
        EXPECT_EQ(user_info->username, "root");
        EXPECT_FALSE(user_info->home_directory.empty());

        // Root should typically be an admin
        bool is_admin = authenticator->is_user_admin("root");
        EXPECT_TRUE(is_admin);

        // Test group membership
        auto groups = authenticator->get_user_groups("root");
        EXPECT_FALSE(groups.empty());

        // Root should be in root group
        auto it = std::find(groups.begin(), groups.end(), "root");
        EXPECT_NE(it, groups.end());
    }

    // Test with non-existent user
    auto no_user_info = authenticator->get_user_info("nonexistentuser123456");
    EXPECT_FALSE(no_user_info.has_value());
}

TEST_F(TrustedAuthTest, PAMAuthenticator_GroupMembership)
{
    auto authenticator = TrustedOSAuthenticatorFactory::create_pam();
    ASSERT_NE(authenticator, nullptr);

    // Test root user group membership
    auto groups = authenticator->get_user_groups("root");
    if (!groups.empty()) {
        // Root should be in root group
        bool in_root_group = authenticator->is_user_in_group("root", "root");
        EXPECT_TRUE(in_root_group);

        // Root should not be in a non-existent group
        bool in_fake_group = authenticator->is_user_in_group("root", "nonexistentgroup123");
        EXPECT_FALSE(in_fake_group);
    }

    // Test non-existent user
    auto no_groups = authenticator->get_user_groups("nonexistentuser123");
    EXPECT_TRUE(no_groups.empty());
}
#endif

TEST_F(TrustedAuthTest, KerberosAuthenticator_Configuration)
{
    auto authenticator =
        TrustedOSAuthenticatorFactory::create_kerberos("EXAMPLE.COM", "kdc.example.com");
    ASSERT_NE(authenticator, nullptr);

    EXPECT_EQ(authenticator->get_auth_type(), TrustedAuthType::Kerberos);
    EXPECT_EQ(authenticator->get_auth_type_name(), "Kerberos");
    EXPECT_TRUE(authenticator->supports_domain_authentication());

    auto kerberos_auth = dynamic_cast<KerberosAuthenticator*>(authenticator.get());
    ASSERT_NE(kerberos_auth, nullptr);

    EXPECT_EQ(kerberos_auth->get_realm(), "EXAMPLE.COM");
    EXPECT_EQ(kerberos_auth->get_kdc(), "kdc.example.com");

    auto domains = kerberos_auth->get_supported_domains();
    EXPECT_EQ(domains.size(), 1);
    EXPECT_EQ(domains[0], "EXAMPLE.COM");

    // Test configuration validation
    auto config_errors = kerberos_auth->get_configuration_errors();
    // Should have no errors since we provided realm and kdc
    EXPECT_TRUE(config_errors.empty());

    // Test availability (will be false since we don't have actual Kerberos)
    EXPECT_FALSE(kerberos_auth->is_available());
}

TEST_F(TrustedAuthTest, KerberosAuthenticator_EmptyConfiguration)
{
    auto authenticator = TrustedOSAuthenticatorFactory::create_kerberos("", "");
    ASSERT_NE(authenticator, nullptr);

    auto kerberos_auth = dynamic_cast<KerberosAuthenticator*>(authenticator.get());
    ASSERT_NE(kerberos_auth, nullptr);

    // Test configuration errors with empty realm
    auto config_errors = kerberos_auth->get_configuration_errors();
    EXPECT_FALSE(config_errors.empty());

    // Should have error about missing realm
    bool has_realm_error = false;
    for (const auto& error : config_errors) {
        if (error.find("realm") != std::string::npos) {
            has_realm_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_realm_error);
}

TEST_F(TrustedAuthTest, LDAPAuthenticator_Configuration)
{
    LDAPAuthenticator::LDAPConfig config;
    config.server_url = "ldap://ldap.example.com:389";
    config.base_dn = "dc=example,dc=com";
    config.bind_dn = "cn=admin,dc=example,dc=com";
    config.bind_password = "adminpass";
    config.user_search_base = "ou=users,dc=example,dc=com";
    config.group_search_base = "ou=groups,dc=example,dc=com";

    auto authenticator = TrustedOSAuthenticatorFactory::create_ldap(config);
    ASSERT_NE(authenticator, nullptr);

    EXPECT_EQ(authenticator->get_auth_type(), TrustedAuthType::LDAP);
    EXPECT_EQ(authenticator->get_auth_type_name(), "LDAP");

    auto ldap_auth = dynamic_cast<LDAPAuthenticator*>(authenticator.get());
    ASSERT_NE(ldap_auth, nullptr);

    auto ldap_config = ldap_auth->get_config();
    EXPECT_EQ(ldap_config.server_url, config.server_url);
    EXPECT_EQ(ldap_config.base_dn, config.base_dn);
    EXPECT_EQ(ldap_config.bind_dn, config.bind_dn);

    // Test configuration validation
    EXPECT_TRUE(ldap_auth->is_available());

    auto config_errors = ldap_auth->get_configuration_errors();
    EXPECT_TRUE(config_errors.empty());
}

TEST_F(TrustedAuthTest, LDAPAuthenticator_InvalidConfiguration)
{
    LDAPAuthenticator::LDAPConfig config; // Empty configuration

    auto authenticator = TrustedOSAuthenticatorFactory::create_ldap(config);
    ASSERT_NE(authenticator, nullptr);

    auto ldap_auth = dynamic_cast<LDAPAuthenticator*>(authenticator.get());
    ASSERT_NE(ldap_auth, nullptr);

    // Test configuration validation with empty config
    EXPECT_FALSE(ldap_auth->is_available());

    auto config_errors = ldap_auth->get_configuration_errors();
    EXPECT_FALSE(config_errors.empty());

    // Should have errors about missing server URL and base DN
    bool has_server_error = false;
    bool has_base_dn_error = false;

    for (const auto& error : config_errors) {
        if (error.find("server URL") != std::string::npos) {
            has_server_error = true;
        }
        if (error.find("base DN") != std::string::npos) {
            has_base_dn_error = true;
        }
    }

    EXPECT_TRUE(has_server_error);
    EXPECT_TRUE(has_base_dn_error);
}

TEST_F(TrustedAuthTest, TrustedOSAuthenticationProvider_Configuration)
{
    auto authenticator = TrustedOSAuthenticatorFactory::create_auto();
    if (!authenticator) {
        GTEST_SKIP() << "No trusted OS authenticator available on this platform";
    }

    auto provider = std::make_unique<TrustedOSAuthenticationProvider>(std::move(authenticator));

    EXPECT_EQ(provider->get_provider_name(), "TrustedOS");
    EXPECT_EQ(provider->get_authentication_method(), AuthenticationMethod::TrustedOS);

    auto capabilities = provider->get_supported_capabilities();
    EXPECT_FALSE(capabilities.empty());

    // Should have basic trusted OS capabilities
    auto it = std::find(capabilities.begin(), capabilities.end(), "trusted_os_authentication");
    EXPECT_NE(it, capabilities.end());

    it = std::find(capabilities.begin(), capabilities.end(), "user_info_retrieval");
    EXPECT_NE(it, capabilities.end());

    // Test configuration validation
    bool is_valid = provider->validate_configuration();
    // This may be true or false depending on system configuration

    auto config_errors = provider->get_configuration_errors();
    // Errors list can be empty or non-empty depending on configuration
}

TEST_F(TrustedAuthTest, TrustedOSAuthenticationProvider_UserInfo)
{
    auto authenticator = TrustedOSAuthenticatorFactory::create_auto();
    if (!authenticator) {
        GTEST_SKIP() << "No trusted OS authenticator available on this platform";
    }

    auto provider = std::make_unique<TrustedOSAuthenticationProvider>(std::move(authenticator));

    // Test OS user info retrieval for root user (should exist on Unix systems)
#ifdef __linux__
    auto user_info = provider->get_os_user_info("root");
    if (user_info.has_value()) {
        EXPECT_EQ(user_info->username, "root");
        EXPECT_FALSE(user_info->auth_method.empty());

        // Test group information
        auto groups = provider->get_user_groups("root");
        EXPECT_FALSE(groups.empty());

        // Test admin check
        bool is_admin = provider->is_user_admin("root");
        EXPECT_TRUE(is_admin);
    }
#endif

    // Test with non-existent user
    auto no_user_info = provider->get_os_user_info("nonexistentuser123456");
    EXPECT_FALSE(no_user_info.has_value());

    auto no_groups = provider->get_user_groups("nonexistentuser123456");
    EXPECT_TRUE(no_groups.empty());

    bool not_admin = provider->is_user_admin("nonexistentuser123456");
    EXPECT_FALSE(not_admin);
}

// Utility Functions Tests
TEST_F(TrustedAuthTest, UtilityFunctions)
{
    // Test TrustedAuthType to_string (we don't have these implemented, but we can test the concept)
    // This would be implemented in the actual code

    // Test enum values
    EXPECT_NE(TrustedAuthType::Auto, TrustedAuthType::PAM);
    EXPECT_NE(TrustedAuthType::PAM, TrustedAuthType::SSPI);
    EXPECT_NE(TrustedAuthType::SSPI, TrustedAuthType::Kerberos);
    EXPECT_NE(TrustedAuthType::Kerberos, TrustedAuthType::LDAP);
    EXPECT_NE(TrustedAuthType::LDAP, TrustedAuthType::Custom);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
