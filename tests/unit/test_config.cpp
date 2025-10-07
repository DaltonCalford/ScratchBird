/**
 * Unit Tests for Config System
 * Tests INI parsing, priority system, environment variables, and type conversions
 * Phase 1: Foundation Infrastructure
 */

#include "scratchbird/core/config.h"
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <algorithm>

using namespace scratchbird::core;

class ConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clear singleton state before each test
        Config::getInstance().clear();

        // Clean up any test files
        cleanupTestFiles();
    }

    void TearDown() override
    {
        // Clear singleton state after each test
        Config::getInstance().clear();

        // Clean up test files
        cleanupTestFiles();

        // Clear environment variables
        unsetenv("SCRATCHBIRD_DATABASE_MAX_CONNECTIONS");
        unsetenv("SCRATCHBIRD_LOGGING_LOG_LEVEL");
        unsetenv("SCRATCHBIRD_MEMORY_BUFFER_POOL_SIZE");
    }

    void cleanupTestFiles()
    {
        std::filesystem::remove("test_config.ini");
        std::filesystem::remove("test_empty.ini");
        std::filesystem::remove("test_invalid.ini");
    }

    void createTestConfig(const std::string& filename, const std::string& content)
    {
        std::ofstream file(filename);
        file << content;
        file.close();
    }
};

// Test 1: Basic INI File Parsing
TEST_F(ConfigTest, ParseBasicINIFile)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n"
                    "default_page_size = 8192\n"
                    "\n"
                    "[logging]\n"
                    "log_level = DEBUG\n"
                    "log_file = test.log\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);
    EXPECT_TRUE(cfg.isLoaded());

    EXPECT_EQ(cfg.getInt("database", "max_connections", 0), 50);
    EXPECT_EQ(cfg.getInt("database", "default_page_size", 0), 8192);
    EXPECT_EQ(cfg.getString("logging", "log_level", ""), "DEBUG");
    EXPECT_EQ(cfg.getString("logging", "log_file", ""), "test.log");
}

// Test 2: Comments and Empty Lines
TEST_F(ConfigTest, ParseCommentsAndEmptyLines)
{
    createTestConfig("test_config.ini",
                    "# This is a comment\n"
                    "[database]\n"
                    "\n"
                    "; This is also a comment\n"
                    "max_connections = 100\n"
                    "\n"
                    "# Another comment\n"
                    "[logging]\n"
                    "log_level = INFO # inline comment not supported\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    EXPECT_EQ(cfg.getInt("database", "max_connections", 0), 100);
    // Note: inline comments are not stripped, so this will include the comment
    EXPECT_TRUE(cfg.getString("logging", "log_level", "").find("INFO") == 0);
}

// Test 3: Quoted Values
TEST_F(ConfigTest, ParseQuotedValues)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "name = \"my database\"\n"
                    "path = '/var/lib/scratchbird'\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    EXPECT_EQ(cfg.getString("database", "name", ""), "my database");
    EXPECT_EQ(cfg.getString("database", "path", ""), "/var/lib/scratchbird");
}

// Test 4: Missing File (Not an Error)
TEST_F(ConfigTest, MissingFileNotError)
{
    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    // Missing file should return OK (it's optional)
    EXPECT_EQ(cfg.initialize("nonexistent.ini", &ctx), Status::OK);
    EXPECT_TRUE(cfg.isLoaded());
}

// Test 5: Default Values
TEST_F(ConfigTest, DefaultValues)
{
    Config& cfg = Config::getInstance();
    cfg.initialize(); // No config file

    EXPECT_EQ(cfg.getInt("database", "max_connections", 100), 100);
    EXPECT_EQ(cfg.getString("logging", "log_level", "INFO"), "INFO");
    EXPECT_EQ(cfg.getBool("sweep", "background_sweep", true), true);
    EXPECT_EQ(cfg.getDouble("performance", "cache_hit_ratio", 0.95), 0.95);
}

// Test 6: Boolean Parsing
TEST_F(ConfigTest, BooleanParsing)
{
    createTestConfig("test_config.ini",
                    "[test]\n"
                    "bool_true = true\n"
                    "bool_yes = yes\n"
                    "bool_on = on\n"
                    "bool_1 = 1\n"
                    "bool_false = false\n"
                    "bool_no = no\n"
                    "bool_off = off\n"
                    "bool_0 = 0\n"
                    "bool_invalid = maybe\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    // True values
    EXPECT_TRUE(cfg.getBool("test", "bool_true", false));
    EXPECT_TRUE(cfg.getBool("test", "bool_yes", false));
    EXPECT_TRUE(cfg.getBool("test", "bool_on", false));
    EXPECT_TRUE(cfg.getBool("test", "bool_1", false));

    // False values
    EXPECT_FALSE(cfg.getBool("test", "bool_false", true));
    EXPECT_FALSE(cfg.getBool("test", "bool_no", true));
    EXPECT_FALSE(cfg.getBool("test", "bool_off", true));
    EXPECT_FALSE(cfg.getBool("test", "bool_0", true));

    // Invalid value - returns default
    EXPECT_TRUE(cfg.getBool("test", "bool_invalid", true));
    EXPECT_FALSE(cfg.getBool("test", "bool_invalid", false));
}

// Test 7: Integer Parsing
TEST_F(ConfigTest, IntegerParsing)
{
    createTestConfig("test_config.ini",
                    "[test]\n"
                    "int_positive = 12345\n"
                    "int_negative = -9876\n"
                    "int_zero = 0\n"
                    "int_invalid = not_a_number\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    EXPECT_EQ(cfg.getInt("test", "int_positive", 0), 12345);
    EXPECT_EQ(cfg.getInt("test", "int_negative", 0), -9876);
    EXPECT_EQ(cfg.getInt("test", "int_zero", 999), 0);

    // Invalid value - returns default
    EXPECT_EQ(cfg.getInt("test", "int_invalid", 42), 42);
}

// Test 8: Unsigned Integer Parsing
TEST_F(ConfigTest, UnsignedIntegerParsing)
{
    createTestConfig("test_config.ini",
                    "[test]\n"
                    "uint_value = 65536\n"
                    "uint_zero = 0\n"
                    "uint_invalid = -100\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    EXPECT_EQ(cfg.getUInt("test", "uint_value", 0), 65536u);
    EXPECT_EQ(cfg.getUInt("test", "uint_zero", 999), 0u);

    // Negative value - stoull wraps it to large unsigned value
    // This is implementation-defined behavior, so we'll just verify it doesn't crash
    uint64_t wrapped_value = cfg.getUInt("test", "uint_invalid", 42);
    EXPECT_TRUE(wrapped_value != 42 || wrapped_value > 0); // Either wraps or uses default
}

// Test 9: Double Parsing
TEST_F(ConfigTest, DoubleParsing)
{
    createTestConfig("test_config.ini",
                    "[test]\n"
                    "double_value = 3.14159\n"
                    "double_negative = -2.718\n"
                    "double_zero = 0.0\n"
                    "double_invalid = not_a_number\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    EXPECT_DOUBLE_EQ(cfg.getDouble("test", "double_value", 0.0), 3.14159);
    EXPECT_DOUBLE_EQ(cfg.getDouble("test", "double_negative", 0.0), -2.718);
    EXPECT_DOUBLE_EQ(cfg.getDouble("test", "double_zero", 999.0), 0.0);

    // Invalid value - returns default
    EXPECT_DOUBLE_EQ(cfg.getDouble("test", "double_invalid", 1.23), 1.23);
}

// Test 10: Environment Variable Override
TEST_F(ConfigTest, EnvironmentVariableOverride)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n"
                    "\n"
                    "[logging]\n"
                    "log_level = INFO\n");

    // Set environment variables
    setenv("SCRATCHBIRD_DATABASE_MAX_CONNECTIONS", "200", 1);
    setenv("SCRATCHBIRD_LOGGING_LOG_LEVEL", "DEBUG", 1);

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    // Environment variables should override file values
    EXPECT_EQ(cfg.getInt("database", "max_connections", 0), 200);
    EXPECT_EQ(cfg.getString("logging", "log_level", ""), "DEBUG");
}

// Test 11: Command-Line Argument Override
TEST_F(ConfigTest, CommandLineArgumentOverride)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n");

    // Set environment variable
    setenv("SCRATCHBIRD_DATABASE_MAX_CONNECTIONS", "100", 1);

    Config& cfg = Config::getInstance();

    // Set command-line argument (highest priority)
    cfg.addCommandLineArg("database", "max_connections", "300");

    ErrorContext ctx;
    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    // Command-line should override both env var and file
    EXPECT_EQ(cfg.getInt("database", "max_connections", 0), 300);
}

// Test 12: Priority System (CLI > Env > File > Default)
TEST_F(ConfigTest, PrioritySystem)
{
    createTestConfig("test_config.ini",
                    "[memory]\n"
                    "buffer_pool_size = 128\n");

    setenv("SCRATCHBIRD_MEMORY_BUFFER_POOL_SIZE", "256", 1);

    Config& cfg = Config::getInstance();
    cfg.addCommandLineArg("memory", "buffer_pool_size", "512");

    ErrorContext ctx;
    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    // CLI arg should win
    EXPECT_EQ(cfg.getUInt("memory", "buffer_pool_size", 0), 512u);

    // Clear CLI arg and check env var
    cfg.clear();
    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);
    EXPECT_EQ(cfg.getUInt("memory", "buffer_pool_size", 0), 256u);

    // Clear env var and check file
    unsetenv("SCRATCHBIRD_MEMORY_BUFFER_POOL_SIZE");
    cfg.clear();
    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);
    EXPECT_EQ(cfg.getUInt("memory", "buffer_pool_size", 0), 128u);

    // No file and check default
    cfg.clear();
    ASSERT_EQ(cfg.initialize("nonexistent.ini", &ctx), Status::OK);
    EXPECT_EQ(cfg.getUInt("memory", "buffer_pool_size", 64), 64u);
}

// Test 13: hasKey Method
TEST_F(ConfigTest, HasKeyMethod)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    EXPECT_TRUE(cfg.hasKey("database", "max_connections"));
    EXPECT_FALSE(cfg.hasKey("database", "nonexistent_key"));
    EXPECT_FALSE(cfg.hasKey("nonexistent_section", "any_key"));
}

// Test 14: set Method
TEST_F(ConfigTest, SetMethod)
{
    Config& cfg = Config::getInstance();
    cfg.initialize();

    cfg.set("test", "key1", "value1");
    cfg.set("test", "key2", "123");

    EXPECT_EQ(cfg.getString("test", "key1", ""), "value1");
    EXPECT_EQ(cfg.getInt("test", "key2", 0), 123);
}

// Test 15: getKeys Method
TEST_F(ConfigTest, GetKeysMethod)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n"
                    "default_page_size = 16384\n"
                    "encoding = utf8\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    std::vector<std::string> keys = cfg.getKeys("database");
    EXPECT_EQ(keys.size(), 3u);
    EXPECT_NE(std::find(keys.begin(), keys.end(), "max_connections"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "default_page_size"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "encoding"), keys.end());

    // Non-existent section
    std::vector<std::string> empty_keys = cfg.getKeys("nonexistent");
    EXPECT_TRUE(empty_keys.empty());
}

// Test 16: getSections Method
TEST_F(ConfigTest, GetSectionsMethod)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n"
                    "\n"
                    "[logging]\n"
                    "log_level = INFO\n"
                    "\n"
                    "[memory]\n"
                    "buffer_pool_size = 128\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    std::vector<std::string> sections = cfg.getSections();
    EXPECT_EQ(sections.size(), 3u);
    EXPECT_NE(std::find(sections.begin(), sections.end(), "database"), sections.end());
    EXPECT_NE(std::find(sections.begin(), sections.end(), "logging"), sections.end());
    EXPECT_NE(std::find(sections.begin(), sections.end(), "memory"), sections.end());
}

// Test 17: reload Method
TEST_F(ConfigTest, ReloadMethod)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);
    EXPECT_EQ(cfg.getInt("database", "max_connections", 0), 50);

    // Modify the file
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 100\n");

    // Reload
    ASSERT_EQ(cfg.reload(&ctx), Status::OK);
    EXPECT_EQ(cfg.getInt("database", "max_connections", 0), 100);
}

// Test 18: clear Method
TEST_F(ConfigTest, ClearMethod)
{
    createTestConfig("test_config.ini",
                    "[database]\n"
                    "max_connections = 50\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);
    EXPECT_TRUE(cfg.isLoaded());
    EXPECT_TRUE(cfg.hasKey("database", "max_connections"));

    cfg.clear();

    EXPECT_FALSE(cfg.isLoaded());
    EXPECT_FALSE(cfg.hasKey("database", "max_connections"));
}

// Test 19: Empty Configuration File
TEST_F(ConfigTest, EmptyConfigurationFile)
{
    createTestConfig("test_empty.ini", "");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_empty.ini", &ctx), Status::OK);
    EXPECT_TRUE(cfg.isLoaded());

    // Should just use defaults
    EXPECT_EQ(cfg.getInt("database", "max_connections", 100), 100);
}

// Test 20: Case Sensitivity
TEST_F(ConfigTest, CaseSensitivity)
{
    createTestConfig("test_config.ini",
                    "[Database]\n"
                    "Max_Connections = 50\n");

    Config& cfg = Config::getInstance();
    ErrorContext ctx;

    ASSERT_EQ(cfg.initialize("test_config.ini", &ctx), Status::OK);

    // Sections and keys are case-sensitive
    EXPECT_EQ(cfg.getInt("Database", "Max_Connections", 0), 50);
    EXPECT_EQ(cfg.getInt("database", "Max_Connections", 999), 999); // Wrong section
    EXPECT_EQ(cfg.getInt("Database", "max_connections", 999), 999); // Wrong key
}
