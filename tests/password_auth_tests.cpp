#include "scratchbird/engine/password_auth.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <memory>

using namespace ScratchBird;

class PasswordAuthTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        temp_dir = std::filesystem::temp_directory_path() / "scratchbird_test";
        std::filesystem::create_directories(temp_dir);

        store = std::make_unique<FilePasswordStore>(temp_dir.string());
        hasher = std::make_unique<PasswordHasher>(PasswordHashAlgorithm::PBKDF2);

        // Create provider with default policy
        PasswordPolicy policy;
        policy.min_length = 8;
        policy.require_uppercase = true;
        policy.require_lowercase = true;
        policy.require_digits = true;
        policy.require_special_chars = true;

        provider = std::make_unique<PasswordAuthenticationProvider>(
            std::make_unique<FilePasswordStore>(temp_dir.string()), policy,
            PasswordHashAlgorithm::PBKDF2);
    }

    void TearDown() override
    {
        provider.reset();
        hasher.reset();
        store.reset();

        std::filesystem::remove_all(temp_dir);
    }

    std::filesystem::path temp_dir;
    std::unique_ptr<FilePasswordStore> store;
    std::unique_ptr<PasswordHasher> hasher;
    std::unique_ptr<PasswordAuthenticationProvider> provider;
};

// PasswordHasher Tests
TEST_F(PasswordAuthTest, PasswordHasher_BasicHashing)
{
    std::string password = "TestPassword123!";
    PasswordPolicy policy;

    // Test password hashing
    auto hash = hasher->hash_password(password, policy);
    EXPECT_EQ(hash.algorithm, PasswordHashAlgorithm::PBKDF2);
    EXPECT_FALSE(hash.hash.empty());
    EXPECT_FALSE(hash.salt.empty());
    EXPECT_FALSE(hash.parameters.empty());

    // Test password verification
    EXPECT_TRUE(hasher->verify_password(password, hash));
    EXPECT_FALSE(hasher->verify_password("WrongPassword", hash));
    EXPECT_FALSE(hasher->verify_password("", hash));
}

TEST_F(PasswordAuthTest, PasswordHasher_DifferentAlgorithms)
{
    std::string password = "TestPassword123!";
    PasswordPolicy policy;

    // Test PBKDF2
    PasswordHasher pbkdf2_hasher(PasswordHashAlgorithm::PBKDF2);
    auto pbkdf2_hash = pbkdf2_hasher.hash_password(password, policy);
    EXPECT_EQ(pbkdf2_hash.algorithm, PasswordHashAlgorithm::PBKDF2);
    EXPECT_TRUE(pbkdf2_hasher.verify_password(password, pbkdf2_hash));

    // Test bcrypt
    PasswordHasher bcrypt_hasher(PasswordHashAlgorithm::Bcrypt);
    auto bcrypt_hash = bcrypt_hasher.hash_password(password, policy);
    EXPECT_EQ(bcrypt_hash.algorithm, PasswordHashAlgorithm::Bcrypt);
    EXPECT_TRUE(bcrypt_hasher.verify_password(password, bcrypt_hash));

    // Verify algorithms produce different hashes
    EXPECT_NE(pbkdf2_hash.hash, bcrypt_hash.hash);

    // Verify cross-algorithm verification fails appropriately
    EXPECT_FALSE(pbkdf2_hasher.verify_password(password, bcrypt_hash));
    EXPECT_FALSE(bcrypt_hasher.verify_password(password, pbkdf2_hash));
}

TEST_F(PasswordAuthTest, PasswordHasher_PasswordValidation)
{
    PasswordPolicy policy;
    policy.min_length = 8;
    policy.max_length = 64;
    policy.require_uppercase = true;
    policy.require_lowercase = true;
    policy.require_digits = true;
    policy.require_special_chars = true;
    policy.forbidden_patterns = {"password", "123456"};

    // Test valid password
    auto result = hasher->validate_password("MySecure123!", "testuser", policy);
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.error_messages.empty());

    // Test password too short
    result = hasher->validate_password("Short1!", "testuser", policy);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());

    // Test missing uppercase
    result = hasher->validate_password("mysecure123!", "testuser", policy);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());

    // Test missing digits
    result = hasher->validate_password("MySecurePass!", "testuser", policy);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());

    // Test missing special characters
    result = hasher->validate_password("MySecure123", "testuser", policy);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());

    // Test forbidden pattern
    result = hasher->validate_password("MyPassword123!", "testuser", policy);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());

    // Test username in password
    policy.forbid_username_in_password = true;
    result = hasher->validate_password("MyTestuser123!", "testuser", policy);
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());
}

TEST_F(PasswordAuthTest, PasswordHasher_StrengthEstimation)
{
    // Test weak password
    double strength = hasher->estimate_password_strength("123456");
    EXPECT_LT(strength, 50.0);

    // Test moderate password
    strength = hasher->estimate_password_strength("Password123");
    EXPECT_GE(strength, 50.0);
    EXPECT_LT(strength, 85.0);

    // Test strong password
    strength = hasher->estimate_password_strength("MyV3ryStr0ng!P@ssw0rd#2024");
    EXPECT_GE(strength, 85.0);

    // Test empty password
    strength = hasher->estimate_password_strength("");
    EXPECT_EQ(strength, 0.0);
}

TEST_F(PasswordAuthTest, PasswordHasher_SaltGeneration)
{
    // Test salt generation
    std::string salt1 = hasher->generate_salt();
    std::string salt2 = hasher->generate_salt();

    EXPECT_FALSE(salt1.empty());
    EXPECT_FALSE(salt2.empty());
    EXPECT_NE(salt1, salt2); // Should be unique

    // Test salt with specific length
    std::string salt16 = hasher->generate_salt(16);
    std::string salt32 = hasher->generate_salt(32);

    EXPECT_EQ(salt16.length(), 32); // Hex encoded, so 16 bytes = 32 chars
    EXPECT_EQ(salt32.length(), 64); // Hex encoded, so 32 bytes = 64 chars
}

// PasswordHash Tests
TEST_F(PasswordAuthTest, PasswordHash_Serialization)
{
    std::string password = "TestPassword123!";
    auto hash = hasher->hash_password(password);

    // Test serialization
    std::string serialized = hash.serialize();
    EXPECT_FALSE(serialized.empty());

    // Test deserialization
    auto deserialized = PasswordHash::deserialize(serialized);
    EXPECT_EQ(deserialized.algorithm, hash.algorithm);
    EXPECT_EQ(deserialized.hash, hash.hash);
    EXPECT_EQ(deserialized.salt, hash.salt);
    EXPECT_EQ(deserialized.parameters, hash.parameters);

    // Verify deserialized hash works
    EXPECT_TRUE(hasher->verify_password(password, deserialized));
}

TEST_F(PasswordAuthTest, PasswordHash_UpgradeDetection)
{
    PasswordPolicy policy;
    auto hash = hasher->hash_password("TestPassword123!", policy);

    // Fresh hash should not need upgrade
    EXPECT_FALSE(hash.needs_upgrade(policy));

    // Simulate old hash (set created time to past)
    hash.created = std::chrono::system_clock::now() - std::chrono::hours(24 * 400);
    EXPECT_TRUE(hash.needs_upgrade(policy));
}

// FilePasswordStore Tests
TEST_F(PasswordAuthTest, FilePasswordStore_BasicOperations)
{
    std::string username = "testuser";
    std::string password = "TestPassword123!";

    auto hash = hasher->hash_password(password);

    // Test storing password hash
    EXPECT_TRUE(store->store_password_hash(username, hash));

    // Test retrieving password hash
    auto retrieved_hash = store->get_password_hash(username);
    ASSERT_TRUE(retrieved_hash.has_value());
    EXPECT_EQ(retrieved_hash->algorithm, hash.algorithm);
    EXPECT_EQ(retrieved_hash->hash, hash.hash);
    EXPECT_EQ(retrieved_hash->salt, hash.salt);

    // Test user existence check
    EXPECT_TRUE(store->user_exists(username));
    EXPECT_FALSE(store->user_exists("nonexistentuser"));

    // Test getting all usernames
    auto usernames = store->get_all_usernames();
    EXPECT_EQ(usernames.size(), 1);
    EXPECT_EQ(usernames[0], username);

    // Test deleting password hash
    EXPECT_TRUE(store->delete_password_hash(username));
    EXPECT_FALSE(store->user_exists(username));
    EXPECT_FALSE(store->get_password_hash(username).has_value());
}

TEST_F(PasswordAuthTest, FilePasswordStore_PasswordHistory)
{
    std::string username = "testuser";

    // Store multiple passwords in history
    for (int i = 1; i <= 5; ++i) {
        std::string password = "TestPassword" + std::to_string(i) + "!";
        auto hash = hasher->hash_password(password);
        EXPECT_TRUE(store->store_password_history(username, hash));
    }

    // Test retrieving password history
    auto history = store->get_password_history(username, 10);
    EXPECT_EQ(history.size(), 5);

    // Test limited history retrieval
    auto limited_history = store->get_password_history(username, 3);
    EXPECT_EQ(limited_history.size(), 3);

    // Test history cleanup
    store->cleanup_password_history(username, 2);
    auto cleaned_history = store->get_password_history(username, 10);
    EXPECT_EQ(cleaned_history.size(), 2);
}

TEST_F(PasswordAuthTest, FilePasswordStore_PasswordExpiry)
{
    std::string username = "testuser";
    auto future_time = std::chrono::system_clock::now() + std::chrono::hours(24);

    // Test setting password expiry
    store->set_password_expiry(username, future_time);

    // Test getting password expiry
    auto expiry = store->get_password_expiry(username);
    ASSERT_TRUE(expiry.has_value());

    // Should be within a reasonable time range (account for precision loss)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(*expiry - future_time);
    EXPECT_LT(std::abs(diff.count()), 2); // Within 2 seconds

    // Test non-existent user expiry
    auto no_expiry = store->get_password_expiry("nonexistent");
    EXPECT_FALSE(no_expiry.has_value());
}

// PasswordAuthenticationProvider Tests
TEST_F(PasswordAuthTest, PasswordAuthProvider_Configuration)
{
    EXPECT_EQ(provider->get_provider_name(), "Password");
    EXPECT_EQ(provider->get_authentication_method(), AuthenticationMethod::Password);
    EXPECT_TRUE(provider->supports_password_management());
    EXPECT_TRUE(provider->supports_account_management());
    EXPECT_TRUE(provider->validate_configuration());

    auto capabilities = provider->get_supported_capabilities();
    EXPECT_FALSE(capabilities.empty());

    auto errors = provider->get_configuration_errors();
    EXPECT_TRUE(errors.empty());
}

TEST_F(PasswordAuthTest, PasswordAuthProvider_UserManagement)
{
    std::string username = "testuser";
    std::string password = "TestPassword123!";

    // Test user creation
    EXPECT_TRUE(provider->create_user(username, password));
    EXPECT_TRUE(provider->user_exists(username));

    // Test duplicate user creation
    EXPECT_FALSE(provider->create_user(username, password));

    // Test getting all users
    auto users = provider->get_all_users();
    EXPECT_EQ(users.size(), 1);
    EXPECT_EQ(users[0], username);

    // Test user deletion
    EXPECT_TRUE(provider->delete_user(username));
    EXPECT_FALSE(provider->user_exists(username));
}

TEST_F(PasswordAuthTest, PasswordAuthProvider_Authentication)
{
    std::string username = "authuser";
    std::string password = "AuthPassword123!";

    // Create user first
    EXPECT_TRUE(provider->create_user(username, password));

    // Test successful authentication
    AuthenticationContext context;
    context.set_username(username);
    context.set_credential("password", password);

    AuthenticationResult result = provider->authenticate(context);
    EXPECT_EQ(result, AuthenticationResult::Success);

    // Test failed authentication with wrong password
    context.set_credential("password", "WrongPassword");
    result = provider->authenticate(context);
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);

    // Test authentication with non-existent user
    context.set_username("nonexistent");
    context.set_credential("password", password);
    result = provider->authenticate(context);
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);
}

TEST_F(PasswordAuthTest, PasswordAuthProvider_PasswordChange)
{
    std::string username = "changeuser";
    std::string old_password = "OldPassword123!";
    std::string new_password = "NewPassword456!";

    // Create user
    EXPECT_TRUE(provider->create_user(username, old_password));

    // Test password change
    AuthenticationResult result = provider->change_password(username, old_password, new_password);
    EXPECT_EQ(result, AuthenticationResult::Success);

    // Test authentication with new password
    AuthenticationContext context;
    context.set_username(username);
    context.set_credential("password", new_password);
    result = provider->authenticate(context);
    EXPECT_EQ(result, AuthenticationResult::Success);

    // Test authentication with old password should fail
    context.set_credential("password", old_password);
    result = provider->authenticate(context);
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);

    // Test password change with wrong old password
    result = provider->change_password(username, "WrongOldPassword", "AnotherNewPassword123!");
    EXPECT_EQ(result, AuthenticationResult::InvalidCredentials);
}

TEST_F(PasswordAuthTest, PasswordAuthProvider_PasswordValidation)
{
    std::string username = "validationuser";

    // Test password validation
    auto result = provider->validate_new_password(username, "ValidPassword123!");
    EXPECT_TRUE(result.is_valid);

    result = provider->validate_new_password(username, "weak");
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_messages.empty());

    std::string error_summary = result.get_error_summary();
    EXPECT_FALSE(error_summary.empty());
}

TEST_F(PasswordAuthTest, PasswordAuthProvider_PasswordExpiry)
{
    std::string username = "expiryuser";
    std::string password = "ExpiryPassword123!";

    // Create user
    EXPECT_TRUE(provider->create_user(username, password));

    // Test password expiry check (should not be expired initially)
    EXPECT_FALSE(provider->is_password_expired(username));

    // Force password change
    EXPECT_TRUE(provider->force_password_change(username));

    // Now password should be expired
    EXPECT_TRUE(provider->is_password_expired(username));

    // Test authentication with expired password
    AuthenticationContext context;
    context.set_username(username);
    context.set_credential("password", password);

    AuthenticationResult result = provider->authenticate(context);
    EXPECT_EQ(result, AuthenticationResult::PasswordExpired);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
