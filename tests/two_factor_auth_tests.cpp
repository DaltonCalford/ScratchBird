#include "scratchbird/engine/two_factor_auth.h"

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace ScratchBird;

class TwoFactorAuthTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        totp_provider = std::make_unique<TOTPProvider>();
        sms_provider = std::make_unique<SMSProvider>("test_api_key", "http://test.example.com/sms");
        email_provider = std::make_unique<EmailProvider>("smtp.example.com", 587,
                                                         "test@example.com", "testpass");

        manager = std::make_unique<TwoFactorAuthenticationManager>();
    }

    void TearDown() override
    {
        manager.reset();
        email_provider.reset();
        sms_provider.reset();
        totp_provider.reset();
    }

    std::unique_ptr<TOTPProvider> totp_provider;
    std::unique_ptr<SMSProvider> sms_provider;
    std::unique_ptr<EmailProvider> email_provider;
    std::unique_ptr<TwoFactorAuthenticationManager> manager;
};

// Utility Functions Tests
TEST_F(TwoFactorAuthTest, UtilityFunctions_TwoFactorMethod)
{
    // Test TwoFactorMethod to_string
    EXPECT_EQ(to_string(TwoFactorMethod::TOTP), "TOTP");
    EXPECT_EQ(to_string(TwoFactorMethod::SMS), "SMS");
    EXPECT_EQ(to_string(TwoFactorMethod::Email), "Email");
    EXPECT_EQ(to_string(TwoFactorMethod::Hardware), "Hardware");
    EXPECT_EQ(to_string(TwoFactorMethod::Backup), "Backup");

    // Test parse_two_factor_method
    EXPECT_EQ(parse_two_factor_method("TOTP"), TwoFactorMethod::TOTP);
    EXPECT_EQ(parse_two_factor_method("SMS"), TwoFactorMethod::SMS);
    EXPECT_EQ(parse_two_factor_method("Email"), TwoFactorMethod::Email);
    EXPECT_EQ(parse_two_factor_method("Invalid"), TwoFactorMethod::TOTP); // Default
}

TEST_F(TwoFactorAuthTest, UtilityFunctions_TwoFactorStatus)
{
    // Test TwoFactorStatus to_string
    EXPECT_EQ(to_string(TwoFactorStatus::NotEnrolled), "NotEnrolled");
    EXPECT_EQ(to_string(TwoFactorStatus::Enrolled), "Enrolled");
    EXPECT_EQ(to_string(TwoFactorStatus::Pending), "Pending");
    EXPECT_EQ(to_string(TwoFactorStatus::Disabled), "Disabled");
    EXPECT_EQ(to_string(TwoFactorStatus::Locked), "Locked");

    // Test parse_two_factor_status
    EXPECT_EQ(parse_two_factor_status("NotEnrolled"), TwoFactorStatus::NotEnrolled);
    EXPECT_EQ(parse_two_factor_status("Enrolled"), TwoFactorStatus::Enrolled);
    EXPECT_EQ(parse_two_factor_status("Pending"), TwoFactorStatus::Pending);
    EXPECT_EQ(parse_two_factor_status("Invalid"), TwoFactorStatus::NotEnrolled); // Default
}

// TOTP Utility Functions Tests
TEST_F(TwoFactorAuthTest, TOTPUtilities_SecretKeyGeneration)
{
    std::string secret1 = generate_totp_secret_key();
    std::string secret2 = generate_totp_secret_key();

    EXPECT_FALSE(secret1.empty());
    EXPECT_FALSE(secret2.empty());
    EXPECT_NE(secret1, secret2); // Should be unique

    // Base32 encoded secrets should have specific characteristics
    EXPECT_GE(secret1.length(), 16); // At least 80 bits encoded as Base32

    // Should contain only Base32 characters
    for (char c : secret1) {
        EXPECT_TRUE((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7') || c == '=');
    }
}

TEST_F(TwoFactorAuthTest, TOTPUtilities_CodeGeneration)
{
    std::string secret = "JBSWY3DPEHPK3PXP"; // Base32 encoded "Hello!"
    std::uint64_t timestamp = 1000;          // Fixed timestamp for predictable results

    // Test code generation
    std::string code1 = generate_totp_code(secret, timestamp, 6, "SHA1");
    EXPECT_EQ(code1.length(), 6);

    // Should contain only digits
    for (char c : code1) {
        EXPECT_TRUE(c >= '0' && c <= '9');
    }

    // Same parameters should produce same code
    std::string code2 = generate_totp_code(secret, timestamp, 6, "SHA1");
    EXPECT_EQ(code1, code2);

    // Different timestamp should produce different code
    std::string code3 = generate_totp_code(secret, timestamp + 1, 6, "SHA1");
    EXPECT_NE(code1, code3);

    // Test 8-digit codes
    std::string code8 = generate_totp_code(secret, timestamp, 8, "SHA1");
    EXPECT_EQ(code8.length(), 8);
}

TEST_F(TwoFactorAuthTest, TOTPUtilities_CodeVerification)
{
    std::string secret = "JBSWY3DPEHPK3PXP";
    std::uint64_t timestamp = 1000;

    // Generate a code
    std::string code = generate_totp_code(secret, timestamp, 6, "SHA1");

    // Verify the code at the same timestamp
    EXPECT_TRUE(verify_totp_code(secret, code, timestamp, 0, 6, "SHA1"));

    // Verify with wrong code
    EXPECT_FALSE(verify_totp_code(secret, "000000", timestamp, 0, 6, "SHA1"));
    EXPECT_FALSE(verify_totp_code(secret, "123456", timestamp, 0, 6, "SHA1"));

    // Test with time window
    EXPECT_TRUE(verify_totp_code(secret, code, timestamp + 1, 1, 6, "SHA1"));
    EXPECT_TRUE(verify_totp_code(secret, code, timestamp - 1, 1, 6, "SHA1"));
    EXPECT_FALSE(verify_totp_code(secret, code, timestamp + 2, 1, 6, "SHA1"));
}

TEST_F(TwoFactorAuthTest, TOTPUtilities_QRCodeURL)
{
    std::string secret = "JBSWY3DPEHPK3PXP";
    std::string account = "user@example.com";
    std::string issuer = "TestService";

    std::string qr_url = generate_totp_qr_url(secret, account, issuer);

    EXPECT_FALSE(qr_url.empty());
    EXPECT_TRUE(qr_url.starts_with("otpauth://totp/"));
    EXPECT_TRUE(qr_url.find(secret) != std::string::npos);
    EXPECT_TRUE(qr_url.find(account) != std::string::npos);
    EXPECT_TRUE(qr_url.find(issuer) != std::string::npos);
    EXPECT_TRUE(qr_url.find("algorithm=SHA1") != std::string::npos);
    EXPECT_TRUE(qr_url.find("digits=6") != std::string::npos);
    EXPECT_TRUE(qr_url.find("period=30") != std::string::npos);
}

// TOTPConfig Tests
TEST_F(TwoFactorAuthTest, TOTPConfig_BasicOperations)
{
    TOTPConfig config;
    config.secret_key = generate_totp_secret_key();
    config.account_name = "testuser@example.com";
    config.issuer = "TestService";
    config.time_step = 30;
    config.digits = 6;
    config.algorithm = "SHA1";

    // Test QR code URL generation
    std::string qr_url = config.generate_qr_code_url();
    EXPECT_FALSE(qr_url.empty());
    EXPECT_TRUE(qr_url.find(config.secret_key) != std::string::npos);
    EXPECT_TRUE(qr_url.find(config.account_name) != std::string::npos);

    // Test code generation
    std::string code = config.generate_code();
    EXPECT_EQ(code.length(), 6);

    // Test code verification (should verify its own generated code)
    EXPECT_TRUE(config.verify_code(code, 1)); // Allow 1 time window tolerance
    EXPECT_FALSE(config.verify_code("000000", 1));
}

// TOTPProvider Tests
TEST_F(TwoFactorAuthTest, TOTPProvider_Configuration)
{
    EXPECT_EQ(totp_provider->get_provider_name(), "TOTP");
    EXPECT_EQ(totp_provider->get_method(), TwoFactorMethod::TOTP);

    auto capabilities = totp_provider->get_supported_capabilities();
    EXPECT_FALSE(capabilities.empty());

    auto it = std::find(capabilities.begin(), capabilities.end(), "totp_authentication");
    EXPECT_NE(it, capabilities.end());

    EXPECT_TRUE(totp_provider->validate_configuration());
    EXPECT_TRUE(totp_provider->get_configuration_errors().empty());

    // Test configuration parameters
    EXPECT_EQ(totp_provider->get_issuer(), "ScratchBird");
    EXPECT_EQ(totp_provider->get_time_step(), 30);
    EXPECT_EQ(totp_provider->get_digits(), 6);

    // Test parameter changes
    totp_provider->set_issuer("TestService");
    EXPECT_EQ(totp_provider->get_issuer(), "TestService");

    totp_provider->set_time_step(60);
    EXPECT_EQ(totp_provider->get_time_step(), 60);

    totp_provider->set_digits(8);
    EXPECT_EQ(totp_provider->get_digits(), 8);
}

TEST_F(TwoFactorAuthTest, TOTPProvider_EnrollmentFlow)
{
    std::string username = "testuser";
    AuthenticationContext context;
    context.set_username(username);

    // Initial status should be NotEnrolled
    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::NotEnrolled);

    // Begin enrollment
    AuthenticationResult result = totp_provider->begin_enrollment(username, context);
    EXPECT_EQ(result, AuthenticationResult::Success);

    // Should have QR code URL and secret key in context
    auto qr_url = context.get_credential("qr_code_url");
    auto secret_key = context.get_credential("secret_key");
    ASSERT_TRUE(qr_url.has_value());
    ASSERT_TRUE(secret_key.has_value());
    EXPECT_FALSE(qr_url->empty());
    EXPECT_FALSE(secret_key->empty());

    // Generate a verification code using the secret
    TOTPConfig temp_config;
    temp_config.secret_key = *secret_key;
    temp_config.time_step = totp_provider->get_time_step();
    temp_config.digits = totp_provider->get_digits();
    std::string verification_code = temp_config.generate_code();

    // Complete enrollment
    result = totp_provider->complete_enrollment(username, verification_code, context);
    EXPECT_EQ(result, AuthenticationResult::Success);

    // Should now be enrolled
    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::Enrolled);

    // Should have backup codes
    auto backup_codes = context.get_credential("backup_codes");
    ASSERT_TRUE(backup_codes.has_value());
    EXPECT_FALSE(backup_codes->empty());
}

TEST_F(TwoFactorAuthTest, TOTPProvider_Authentication)
{
    std::string username = "authuser";

    // First enroll the user (simplified)
    AuthenticationContext enrollment_context;
    enrollment_context.set_username(username);

    totp_provider->begin_enrollment(username, enrollment_context);

    auto secret_key = enrollment_context.get_credential("secret_key");
    ASSERT_TRUE(secret_key.has_value());

    TOTPConfig config;
    config.secret_key = *secret_key;
    config.time_step = totp_provider->get_time_step();
    config.digits = totp_provider->get_digits();

    std::string verification_code = config.generate_code();
    totp_provider->complete_enrollment(username, verification_code, enrollment_context);

    // Now test authentication
    AuthenticationContext auth_context;
    auth_context.set_username(username);

    // Create challenge
    auto challenge = totp_provider->create_challenge(username, auth_context);
    ASSERT_NE(challenge, nullptr);

    EXPECT_EQ(challenge->get_method(), AuthenticationMethod::TwoFactor);
    EXPECT_FALSE(challenge->get_challenge_data().empty());
    EXPECT_FALSE(challenge->is_expired());

    // Generate current code
    std::string current_code = config.generate_code();

    // Verify response
    AuthenticationResult result =
        totp_provider->verify_response(*challenge, current_code, auth_context);
    EXPECT_EQ(result, AuthenticationResult::Success);

    auto auth_method = auth_context.get_credential("auth_method");
    ASSERT_TRUE(auth_method.has_value());
    EXPECT_EQ(*auth_method, "totp");

    // Test wrong code
    result = totp_provider->verify_response(*challenge, "000000", auth_context);
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);
}

TEST_F(TwoFactorAuthTest, TOTPProvider_BackupCodes)
{
    std::string username = "backupuser";

    // Enroll user first
    AuthenticationContext context;
    context.set_username(username);

    totp_provider->begin_enrollment(username, context);
    auto secret_key = context.get_credential("secret_key");
    ASSERT_TRUE(secret_key.has_value());

    TOTPConfig config;
    config.secret_key = *secret_key;
    std::string verification_code = config.generate_code();
    totp_provider->complete_enrollment(username, verification_code, context);

    // Generate new backup codes
    auto backup_codes = totp_provider->generate_backup_codes(username);
    EXPECT_EQ(backup_codes.size(), 10); // Default count

    for (const auto& code : backup_codes) {
        EXPECT_EQ(code.length(), 8); // 8-digit backup codes
        // Should contain only digits
        for (char c : code) {
            EXPECT_TRUE(c >= '0' && c <= '9');
        }
    }

    // Test backup code verification
    if (!backup_codes.empty()) {
        std::string test_code = backup_codes[0];

        // First use should succeed
        EXPECT_TRUE(totp_provider->verify_backup_code(username, test_code));

        // Second use should fail (single-use)
        EXPECT_FALSE(totp_provider->verify_backup_code(username, test_code));

        // Non-existent code should fail
        EXPECT_FALSE(totp_provider->verify_backup_code(username, "99999999"));
    }
}

TEST_F(TwoFactorAuthTest, TOTPProvider_StatusManagement)
{
    std::string username = "statususer";

    // Initial status
    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::NotEnrolled);

    // Enroll user
    AuthenticationContext context;
    context.set_username(username);
    totp_provider->begin_enrollment(username, context);

    auto secret_key = context.get_credential("secret_key");
    ASSERT_TRUE(secret_key.has_value());

    TOTPConfig config;
    config.secret_key = *secret_key;
    std::string verification_code = config.generate_code();
    totp_provider->complete_enrollment(username, verification_code, context);

    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::Enrolled);

    // Disable 2FA
    AuthenticationResult result = totp_provider->disable_2fa(username);
    EXPECT_EQ(result, AuthenticationResult::Success);
    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::Disabled);

    // Re-enable 2FA
    result = totp_provider->enable_2fa(username);
    EXPECT_EQ(result, AuthenticationResult::Success);
    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::Enrolled);

    // Reset 2FA
    result = totp_provider->reset_2fa(username);
    EXPECT_EQ(result, AuthenticationResult::Success);
    EXPECT_EQ(totp_provider->get_status(username), TwoFactorStatus::NotEnrolled);
}

// SMS Provider Tests
TEST_F(TwoFactorAuthTest, SMSProvider_Configuration)
{
    EXPECT_EQ(sms_provider->get_provider_name(), "SMS");
    EXPECT_EQ(sms_provider->get_method(), TwoFactorMethod::SMS);

    auto capabilities = sms_provider->get_supported_capabilities();
    EXPECT_FALSE(capabilities.empty());

    auto it = std::find(capabilities.begin(), capabilities.end(), "sms_authentication");
    EXPECT_NE(it, capabilities.end());

    // Should be valid with API key and service URL
    EXPECT_TRUE(sms_provider->validate_configuration());
    EXPECT_TRUE(sms_provider->get_configuration_errors().empty());

    // Test configuration changes
    sms_provider->set_api_key("new_key");
    sms_provider->set_service_url("http://new.example.com");
    sms_provider->set_message_template("Code: {code}");

    EXPECT_TRUE(sms_provider->validate_configuration());
}

TEST_F(TwoFactorAuthTest, SMSProvider_InvalidConfiguration)
{
    // Create SMS provider with empty configuration
    SMSProvider invalid_sms("", "");

    EXPECT_FALSE(invalid_sms.validate_configuration());

    auto errors = invalid_sms.get_configuration_errors();
    EXPECT_FALSE(errors.empty());

    bool has_api_error = false;
    bool has_url_error = false;

    for (const auto& error : errors) {
        if (error.find("API key") != std::string::npos) {
            has_api_error = true;
        }
        if (error.find("service URL") != std::string::npos) {
            has_url_error = true;
        }
    }

    EXPECT_TRUE(has_api_error);
    EXPECT_TRUE(has_url_error);
}

// Email Provider Tests
TEST_F(TwoFactorAuthTest, EmailProvider_Configuration)
{
    EXPECT_EQ(email_provider->get_provider_name(), "Email");
    EXPECT_EQ(email_provider->get_method(), TwoFactorMethod::Email);

    auto capabilities = email_provider->get_supported_capabilities();
    EXPECT_FALSE(capabilities.empty());

    auto it = std::find(capabilities.begin(), capabilities.end(), "email_authentication");
    EXPECT_NE(it, capabilities.end());

    // Should be valid with SMTP configuration
    EXPECT_TRUE(email_provider->validate_configuration());
    EXPECT_TRUE(email_provider->get_configuration_errors().empty());

    // Test configuration changes
    email_provider->set_smtp_config("new.smtp.com", 25, "newuser", "newpass");
    email_provider->set_email_template("New Subject", "Code is: {code}");

    EXPECT_TRUE(email_provider->validate_configuration());
}

TEST_F(TwoFactorAuthTest, EmailProvider_InvalidConfiguration)
{
    // Create email provider with empty configuration
    EmailProvider invalid_email("", 0, "", "");

    EXPECT_FALSE(invalid_email.validate_configuration());

    auto errors = invalid_email.get_configuration_errors();
    EXPECT_FALSE(errors.empty());

    bool has_server_error = false;
    bool has_port_error = false;

    for (const auto& error : errors) {
        if (error.find("SMTP server") != std::string::npos) {
            has_server_error = true;
        }
        if (error.find("SMTP port") != std::string::npos) {
            has_port_error = true;
        }
    }

    EXPECT_TRUE(has_server_error);
    EXPECT_TRUE(has_port_error);
}

// TwoFactorAuthenticationManager Tests
TEST_F(TwoFactorAuthTest, TwoFactorManager_ProviderManagement)
{
    // Register TOTP provider
    auto totp_ptr = std::make_unique<TOTPProvider>();
    std::string totp_name = totp_ptr->get_provider_name();
    TwoFactorMethod totp_method = totp_ptr->get_method();

    EXPECT_TRUE(manager->register_provider(std::move(totp_ptr)));

    // Test provider retrieval
    auto* retrieved_provider = manager->get_provider(totp_name);
    ASSERT_NE(retrieved_provider, nullptr);
    EXPECT_EQ(retrieved_provider->get_provider_name(), totp_name);

    auto* method_provider = manager->get_provider(totp_method);
    ASSERT_NE(method_provider, nullptr);
    EXPECT_EQ(method_provider->get_method(), totp_method);

    // Test registered providers list
    auto providers = manager->get_registered_providers();
    EXPECT_EQ(providers.size(), 1);
    EXPECT_EQ(providers[0], totp_name);

    // Register SMS provider
    auto sms_ptr = std::make_unique<SMSProvider>("api_key", "http://example.com");
    std::string sms_name = sms_ptr->get_provider_name();

    EXPECT_TRUE(manager->register_provider(std::move(sms_ptr)));
    EXPECT_EQ(manager->get_registered_providers().size(), 2);

    // Test unregistration
    EXPECT_TRUE(manager->unregister_provider(sms_name));
    EXPECT_EQ(manager->get_registered_providers().size(), 1);
    EXPECT_EQ(manager->get_provider(sms_name), nullptr);
}

TEST_F(TwoFactorAuthTest, TwoFactorManager_UserEnrollment)
{
    // Register TOTP provider
    manager->register_provider(std::make_unique<TOTPProvider>());

    std::string username = "enrollmentuser";

    // Initially not enrolled
    EXPECT_FALSE(manager->is_user_enrolled(username));
    EXPECT_EQ(manager->get_user_2fa_status(username), TwoFactorStatus::NotEnrolled);

    auto enrolled_methods = manager->get_user_enrolled_methods(username);
    EXPECT_TRUE(enrolled_methods.empty());
}

TEST_F(TwoFactorAuthTest, TwoFactorManager_Statistics)
{
    // Test initial statistics
    auto stats = manager->get_statistics();
    EXPECT_EQ(stats.enrolled_users, 0);
    EXPECT_EQ(stats.successful_verifications, 0);
    EXPECT_EQ(stats.failed_verifications, 0);

    // Reset statistics
    manager->reset_statistics();
    stats = manager->get_statistics();
    EXPECT_EQ(stats.enrolled_users, 0);
    EXPECT_EQ(stats.successful_verifications, 0);
    EXPECT_EQ(stats.failed_verifications, 0);
}

TEST_F(TwoFactorAuthTest, TwoFactorManager_PolicyConfiguration)
{
    // Test default policy settings
    EXPECT_FALSE(manager->is_2fa_required_for_all());
    EXPECT_TRUE(manager->is_2fa_required_for_admins());
    EXPECT_TRUE(manager->are_backup_codes_required());
    EXPECT_EQ(manager->get_max_failed_attempts(), 3);
    EXPECT_EQ(manager->get_lockout_duration(), std::chrono::minutes(30));

    // Test policy changes
    manager->set_2fa_required_for_all(true);
    EXPECT_TRUE(manager->is_2fa_required_for_all());

    manager->set_2fa_required_for_admins(false);
    EXPECT_FALSE(manager->is_2fa_required_for_admins());

    manager->set_backup_codes_required(false);
    EXPECT_FALSE(manager->are_backup_codes_required());

    manager->set_max_failed_attempts(5);
    EXPECT_EQ(manager->get_max_failed_attempts(), 5);

    manager->set_lockout_duration(std::chrono::minutes(60));
    EXPECT_EQ(manager->get_lockout_duration(), std::chrono::minutes(60));
}

// TwoFactorAuthenticationProvider Tests
TEST_F(TwoFactorAuthTest, TwoFactorAuthProvider_Configuration)
{
    auto tf_manager = std::make_unique<TwoFactorAuthenticationManager>();
    tf_manager->register_provider(std::make_unique<TOTPProvider>());

    auto provider = std::make_unique<TwoFactorAuthenticationProvider>(std::move(tf_manager));

    EXPECT_EQ(provider->get_provider_name(), "TwoFactor");
    EXPECT_EQ(provider->get_authentication_method(), AuthenticationMethod::TwoFactor);
    EXPECT_TRUE(provider->supports_challenge_response());

    auto capabilities = provider->get_supported_capabilities();
    EXPECT_FALSE(capabilities.empty());

    auto it = std::find(capabilities.begin(), capabilities.end(), "two_factor_authentication");
    EXPECT_NE(it, capabilities.end());

    // Should be valid with registered providers
    EXPECT_TRUE(provider->validate_configuration());
    EXPECT_TRUE(provider->get_configuration_errors().empty());
}

TEST_F(TwoFactorAuthTest, TwoFactorAuthProvider_InvalidConfiguration)
{
    // Create provider with no registered providers
    auto empty_manager = std::make_unique<TwoFactorAuthenticationManager>();
    auto provider = std::make_unique<TwoFactorAuthenticationProvider>(std::move(empty_manager));

    EXPECT_FALSE(provider->validate_configuration());

    auto errors = provider->get_configuration_errors();
    EXPECT_FALSE(errors.empty());

    bool has_provider_error = false;
    for (const auto& error : errors) {
        if (error.find("providers") != std::string::npos) {
            has_provider_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_provider_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
