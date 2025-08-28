#include "scratchbird/engine/authentication.h"
#include "scratchbird/engine/password_auth.h"
#include "scratchbird/engine/trusted_auth.h"
#include "scratchbird/engine/two_factor_auth.h"

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace ScratchBird;

class AuthenticationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        manager = std::make_unique<AuthenticationManager>();
    }

    void TearDown() override
    {
        manager.reset();
    }

    std::unique_ptr<AuthenticationManager> manager;
};

// Authentication Framework Tests
TEST_F(AuthenticationTest, AuthenticationContext_BasicOperations)
{
    AuthenticationContext context;

    // Test basic setters and getters
    context.set_username("testuser");
    context.set_database("testdb");
    context.set_remote_address("192.168.1.100");
    context.set_client_info("TestClient/1.0");

    EXPECT_EQ(context.get_username(), "testuser");
    EXPECT_EQ(context.get_database(), "testdb");
    EXPECT_EQ(context.get_remote_address(), "192.168.1.100");
    EXPECT_EQ(context.get_client_info(), "TestClient/1.0");

    // Test credentials
    context.set_credential("password", "secret123");
    context.set_credential("token", "abc123xyz");

    auto password = context.get_credential("password");
    auto token = context.get_credential("token");
    auto nonexistent = context.get_credential("nonexistent");

    ASSERT_TRUE(password.has_value());
    ASSERT_TRUE(token.has_value());
    ASSERT_FALSE(nonexistent.has_value());

    EXPECT_EQ(*password, "secret123");
    EXPECT_EQ(*token, "abc123xyz");

    // Test authentication state
    EXPECT_FALSE(context.is_authenticated());
    context.set_authenticated(true);
    EXPECT_TRUE(context.is_authenticated());

    EXPECT_FALSE(context.requires_2fa());
    context.set_requires_2fa(true);
    EXPECT_TRUE(context.requires_2fa());
}

TEST_F(AuthenticationTest, AuthenticationChallenge_Lifecycle)
{
    std::string challenge_id = "test_challenge_123";
    AuthenticationMethod method = AuthenticationMethod::TwoFactor;
    std::string challenge_data = "Please enter your verification code";

    AuthenticationChallenge challenge(challenge_id, method, challenge_data);

    EXPECT_EQ(challenge.get_challenge_id(), challenge_id);
    EXPECT_EQ(challenge.get_method(), method);
    EXPECT_EQ(challenge.get_challenge_data(), challenge_data);
    EXPECT_TRUE(challenge.get_response().empty());

    // Test response
    std::string response = "123456";
    challenge.set_response(response);
    EXPECT_EQ(challenge.get_response(), response);

    // Test expiry
    auto future_time = std::chrono::system_clock::now() + std::chrono::minutes(5);
    challenge.set_expiry(future_time);
    EXPECT_FALSE(challenge.is_expired());

    auto past_time = std::chrono::system_clock::now() - std::chrono::minutes(1);
    challenge.set_expiry(past_time);
    EXPECT_TRUE(challenge.is_expired());
}

TEST_F(AuthenticationTest, SecurityContext_RoleManagement)
{
    std::string username = "testuser";
    std::string database = "testdb";
    std::vector<std::string> initial_roles = {"user", "reader"};

    SecurityContext context(username, database, initial_roles);

    EXPECT_EQ(context.get_username(), username);
    EXPECT_EQ(context.get_database(), database);

    // Test initial roles
    auto roles = context.get_roles();
    EXPECT_EQ(roles.size(), 2);
    EXPECT_TRUE(context.has_role("user"));
    EXPECT_TRUE(context.has_role("reader"));
    EXPECT_FALSE(context.has_role("admin"));

    // Test role addition
    context.add_role("admin");
    EXPECT_TRUE(context.has_role("admin"));
    EXPECT_EQ(context.get_roles().size(), 3);

    // Test duplicate role addition (should not duplicate)
    context.add_role("user");
    EXPECT_EQ(context.get_roles().size(), 3);

    // Test role removal
    context.remove_role("reader");
    EXPECT_FALSE(context.has_role("reader"));
    EXPECT_EQ(context.get_roles().size(), 2);

    // Test permissions
    EXPECT_FALSE(context.has_permission("SELECT"));
    context.grant_permission("SELECT");
    EXPECT_TRUE(context.has_permission("SELECT"));

    context.grant_permission("INSERT");
    context.revoke_permission("SELECT");
    EXPECT_FALSE(context.has_permission("SELECT"));
    EXPECT_TRUE(context.has_permission("INSERT"));

    // Test security attributes
    EXPECT_FALSE(context.is_security_definer());
    context.set_security_definer(true);
    EXPECT_TRUE(context.is_security_definer());

    EXPECT_FALSE(context.is_superuser());
    context.set_superuser(true);
    EXPECT_TRUE(context.is_superuser());
}

TEST_F(AuthenticationTest, AuthenticationManager_ProviderManagement)
{
    // Create a simple test provider
    class TestAuthProvider : public AuthenticationProvider
    {
      public:
        std::string get_provider_name() const override
        {
            return "TestProvider";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::Password;
        }
        std::vector<std::string> get_supported_capabilities() const override
        {
            return {"test_authentication"};
        }
        AuthenticationResult authenticate(AuthenticationContext& context) override
        {
            return AuthenticationResult::Success;
        }
        bool validate_configuration() const override
        {
            return true;
        }
    };

    // Test provider registration
    auto provider = std::make_unique<TestAuthProvider>();
    std::string provider_name = provider->get_provider_name();
    AuthenticationMethod method = provider->get_authentication_method();

    EXPECT_TRUE(manager->register_provider(std::move(provider)));

    // Test provider retrieval
    auto* retrieved_provider = manager->get_provider(provider_name);
    ASSERT_NE(retrieved_provider, nullptr);
    EXPECT_EQ(retrieved_provider->get_provider_name(), provider_name);

    auto* method_provider = manager->get_provider(method);
    ASSERT_NE(method_provider, nullptr);
    EXPECT_EQ(method_provider->get_provider_name(), provider_name);

    // Test registered providers list
    auto providers = manager->get_registered_providers();
    EXPECT_EQ(providers.size(), 1);
    EXPECT_EQ(providers[0], provider_name);

    // Test provider unregistration
    EXPECT_TRUE(manager->unregister_provider(provider_name));
    EXPECT_EQ(manager->get_registered_providers().size(), 0);
    EXPECT_EQ(manager->get_provider(provider_name), nullptr);
}

TEST_F(AuthenticationTest, AuthenticationManager_SessionManagement)
{
    // Create and register a test provider
    class TestSessionProvider : public AuthenticationProvider
    {
      public:
        std::string get_provider_name() const override
        {
            return "SessionProvider";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::Password;
        }
        std::vector<std::string> get_supported_capabilities() const override
        {
            return {"session_management"};
        }
        AuthenticationResult authenticate(AuthenticationContext& context) override
        {
            context.set_authenticated(true);
            return AuthenticationResult::Success;
        }
        bool validate_configuration() const override
        {
            return true;
        }
    };

    manager->register_provider(std::make_unique<TestSessionProvider>());

    // Test user authentication
    AuthenticationContext context;
    context.set_username("sessionuser");
    context.set_database("testdb");
    context.set_credential("password", "testpass");

    AuthenticationResult result = manager->authenticate_user(context);
    EXPECT_EQ(result, AuthenticationResult::Success);
    EXPECT_TRUE(context.is_authenticated());

    // Check if session was created
    auto session_id = context.get_credential("session_id");
    ASSERT_TRUE(session_id.has_value());
    EXPECT_FALSE(session_id->empty());

    // Test session validation
    EXPECT_TRUE(manager->validate_session(*session_id));

    // Test session invalidation
    manager->invalidate_session(*session_id);
    EXPECT_FALSE(manager->validate_session(*session_id));
}

TEST_F(AuthenticationTest, AuthenticationManager_AccountLockout)
{
    class FailingAuthProvider : public AuthenticationProvider
    {
      public:
        std::string get_provider_name() const override
        {
            return "FailingProvider";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::Password;
        }
        std::vector<std::string> get_supported_capabilities() const override
        {
            return {"account_lockout_testing"};
        }
        AuthenticationResult authenticate(AuthenticationContext& context) override
        {
            return AuthenticationResult::InvalidCredentials; // Always fail
        }
        bool validate_configuration() const override
        {
            return true;
        }
    };

    manager->register_provider(std::make_unique<FailingAuthProvider>());
    manager->set_failed_attempt_limit(3);
    manager->set_lockout_duration(std::chrono::minutes(1));

    AuthenticationContext context;
    context.set_username("lockoutuser");
    context.set_database("testdb");

    // Perform multiple failed authentication attempts
    for (int i = 0; i < 3; ++i) {
        AuthenticationResult result = manager->authenticate_user(context);
        EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);
    }

    // Next attempt should result in account lockout
    AuthenticationResult result = manager->authenticate_user(context);
    EXPECT_EQ(result, AuthenticationResult::AccountLocked);

    // Unlock the account
    EXPECT_EQ(manager->unlock_user_account("lockoutuser"), AuthenticationResult::Success);

    // Should be able to attempt authentication again
    result = manager->authenticate_user(context);
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);
}

TEST_F(AuthenticationTest, AuthenticationManager_Statistics)
{
    class StatsTestProvider : public AuthenticationProvider
    {
      public:
        mutable bool should_succeed = true;

        std::string get_provider_name() const override
        {
            return "StatsProvider";
        }
        AuthenticationMethod get_authentication_method() const override
        {
            return AuthenticationMethod::Password;
        }
        std::vector<std::string> get_supported_capabilities() const override
        {
            return {"statistics_testing"};
        }
        AuthenticationResult authenticate(AuthenticationContext& context) override
        {
            if (should_succeed) {
                context.set_authenticated(true);
                return AuthenticationResult::Success;
            }
            return AuthenticationResult::InvalidCredentials;
        }
        bool validate_configuration() const override
        {
            return true;
        }
    };

    auto provider = std::make_unique<StatsTestProvider>();
    auto* provider_ptr = provider.get();
    manager->register_provider(std::move(provider));

    // Initial statistics
    auto stats = manager->get_statistics();
    EXPECT_EQ(stats.successful_authentications, 0);
    EXPECT_EQ(stats.failed_authentications, 0);

    AuthenticationContext context;
    context.set_username("statsuser");

    // Successful authentication
    provider_ptr->should_succeed = true;
    AuthenticationResult result = manager->authenticate_user(context);
    EXPECT_EQ(result, AuthenticationResult::Success);

    stats = manager->get_statistics();
    EXPECT_EQ(stats.successful_authentications, 1);
    EXPECT_EQ(stats.failed_authentications, 0);

    // Failed authentication
    provider_ptr->should_succeed = false;
    result = manager->authenticate_user(context);
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);

    stats = manager->get_statistics();
    EXPECT_EQ(stats.successful_authentications, 1);
    EXPECT_EQ(stats.failed_authentications, 1);

    // Reset statistics
    manager->reset_statistics();
    stats = manager->get_statistics();
    EXPECT_EQ(stats.successful_authentications, 0);
    EXPECT_EQ(stats.failed_authentications, 0);
}

// Utility Functions Tests
TEST_F(AuthenticationTest, UtilityFunctions)
{
    // Test AuthenticationResult to_string
    EXPECT_EQ(to_string(AuthenticationResult::Success), "Success");
    EXPECT_EQ(to_string(AuthenticationResult::InvalidCredentials), "Invalid credentials");
    EXPECT_EQ(to_string(AuthenticationResult::AccountLocked), "Account locked");

    // Test AuthenticationMethod to_string and parsing
    EXPECT_EQ(to_string(AuthenticationMethod::Password), "Password");
    EXPECT_EQ(to_string(AuthenticationMethod::TrustedOS), "TrustedOS");
    EXPECT_EQ(to_string(AuthenticationMethod::TwoFactor), "TwoFactor");

    EXPECT_EQ(parse_authentication_method("Password"), AuthenticationMethod::Password);
    EXPECT_EQ(parse_authentication_method("TrustedOS"), AuthenticationMethod::TrustedOS);
    EXPECT_EQ(parse_authentication_method("TwoFactor"), AuthenticationMethod::TwoFactor);
    EXPECT_EQ(parse_authentication_method("Invalid"), AuthenticationMethod::Password); // Default
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
