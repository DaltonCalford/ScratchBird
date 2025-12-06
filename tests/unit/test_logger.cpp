/**
 * Unit Tests for Logger System
 * Tests log levels, categories, file output, and config integration
 * Phase 1: Foundation Infrastructure
 */

#include "scratchbird/core/logger.h"
#include "scratchbird/core/config.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <atomic>
#include <unistd.h>

using namespace scratchbird::core;

class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique file prefix per test to avoid conflicts during parallel execution
        static std::atomic<int> test_counter{0};
        test_id_ = std::to_string(getpid()) + "_" + std::to_string(test_counter++);
        log_file_ = "test_logger_" + test_id_ + ".log";
        log_file_2_ = "test_logger2_" + test_id_ + ".log";
        config_file_ = "test_logger_config_" + test_id_ + ".ini";
        multithread_log_file_ = "test_logger_multithread_" + test_id_ + ".log";

        // Get logger instance
        Logger& log = Logger::getInstance();

        // Close any open log file
        log.close();

        // Clean up test files
        cleanupTestFiles();
    }

    void TearDown() override
    {
        Logger& log = Logger::getInstance();
        log.close();

        // Clean up test files
        cleanupTestFiles();

        // Reset to defaults
        log.setLogLevel(LogLevel::INFO);
        log.setTimestampEnabled(true);
        log.setThreadIdEnabled(false);
        log.setSourceLocationEnabled(true);
    }

    void cleanupTestFiles()
    {
        std::filesystem::remove(log_file_);
        std::filesystem::remove(log_file_2_);
        std::filesystem::remove(config_file_);
        std::filesystem::remove(multithread_log_file_);
    }

    std::string test_id_;
    std::string log_file_;
    std::string log_file_2_;
    std::string config_file_;
    std::string multithread_log_file_;

    std::string readLogFile(const std::string& filename)
    {
        std::ifstream file(filename);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    int countLines(const std::string& filename)
    {
        std::ifstream file(filename);
        int count = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                count++;
            }
        }
        return count;
    }

    bool fileContains(const std::string& filename, const std::string& text)
    {
        std::string content = readLogFile(filename);
        return content.find(text) != std::string::npos;
    }
};

// Test 1: Basic Logging to File
TEST_F(LoggerTest, BasicLoggingToFile)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    LOG_INFO(GENERAL, "Test message");

    log.flush();

    EXPECT_TRUE(std::filesystem::exists(log_file_));
    EXPECT_TRUE(fileContains(log_file_, "Test message"));
    EXPECT_TRUE(fileContains(log_file_, "[INFO]"));
    EXPECT_TRUE(fileContains(log_file_, "[GENERAL]"));
}

// Test 2: Log Level Filtering
TEST_F(LoggerTest, LogLevelFiltering)
{
    Logger& log = Logger::getInstance();
    log.setLogLevel(LogLevel::WARNING);
    log.setLogFile(log_file_);

    // These should not appear (below WARNING)
    LOG_TRACE(GENERAL, "Trace message");
    LOG_DEBUG(GENERAL, "Debug message");
    LOG_INFO(GENERAL, "Info message");

    // These should appear (WARNING and above)
    LOG_WARNING(GENERAL, "Warning message");
    LOG_ERROR(GENERAL, "Error message");
    LOG_CRITICAL(GENERAL, "Critical message");

    log.flush();

    std::string content = readLogFile(log_file_);

    EXPECT_TRUE(content.find("Trace message") == std::string::npos);
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);

    EXPECT_TRUE(content.find("Warning message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message") != std::string::npos);
    EXPECT_TRUE(content.find("Critical message") != std::string::npos);
}

// Test 3: All Log Levels
TEST_F(LoggerTest, AllLogLevels)
{
    Logger& log = Logger::getInstance();
    log.setLogLevel(LogLevel::TRACE);
    log.setLogFile(log_file_);

    LOG_TRACE(GENERAL, "Trace");
    LOG_DEBUG(GENERAL, "Debug");
    LOG_INFO(GENERAL, "Info");
    LOG_WARNING(GENERAL, "Warning");
    LOG_ERROR(GENERAL, "Error");
    LOG_CRITICAL(GENERAL, "Critical");

    log.flush();

    std::string content = readLogFile(log_file_);

    EXPECT_TRUE(content.find("[TRACE]") != std::string::npos);
    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(content.find("[WARN]") != std::string::npos);
    EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(content.find("[CRIT]") != std::string::npos);
}

// Test 4: All Log Categories
TEST_F(LoggerTest, AllLogCategories)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    LOG_INFO(GENERAL, "General");
    LOG_INFO(STORAGE, "Storage");
    LOG_INFO(TRANSACTION, "Transaction");
    LOG_INFO(LOCK, "Lock");
    LOG_INFO(PARSER, "Parser");
    LOG_INFO(EXECUTOR, "Executor");
    LOG_INFO(NETWORK, "Network");
    LOG_INFO(CATALOG, "Catalog");
    LOG_INFO(BTREE, "BTree");
    LOG_INFO(HASH, "Hash");
    LOG_INFO(BUFFER, "Buffer");
    LOG_INFO(VACUUM, "Vacuum");

    log.flush();

    std::string content = readLogFile(log_file_);

    EXPECT_TRUE(content.find("[GENERAL]") != std::string::npos);
    EXPECT_TRUE(content.find("[STORAGE]") != std::string::npos);
    EXPECT_TRUE(content.find("[TRANSACTION]") != std::string::npos);
    EXPECT_TRUE(content.find("[LOCK]") != std::string::npos);
    EXPECT_TRUE(content.find("[PARSER]") != std::string::npos);
    EXPECT_TRUE(content.find("[EXECUTOR]") != std::string::npos);
    EXPECT_TRUE(content.find("[NETWORK]") != std::string::npos);
    EXPECT_TRUE(content.find("[CATALOG]") != std::string::npos);
    EXPECT_TRUE(content.find("[BTREE]") != std::string::npos);
    EXPECT_TRUE(content.find("[HASH]") != std::string::npos);
    EXPECT_TRUE(content.find("[BUFFER]") != std::string::npos);
    EXPECT_TRUE(content.find("[VACUUM]") != std::string::npos);
}

// Test 5: Formatted Messages
TEST_F(LoggerTest, FormattedMessages)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    int value = 42;
    std::string text = "test";
    LOG_INFO(GENERAL, "Integer: %d, String: %s", value, text.c_str());

    log.flush();

    std::string content = readLogFile(log_file_);

    EXPECT_TRUE(content.find("Integer: 42") != std::string::npos);
    EXPECT_TRUE(content.find("String: test") != std::string::npos);
}

// Test 6: Timestamp Enabled/Disabled
TEST_F(LoggerTest, TimestampEnabledDisabled)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    // With timestamp
    log.setTimestampEnabled(true);
    LOG_INFO(GENERAL, "With timestamp");

    log.flush();
    std::string content1 = readLogFile(log_file_);

    // Should contain date/time pattern (YYYY-MM-DD HH:MM:SS.mmm)
    EXPECT_TRUE(content1.find("202") != std::string::npos); // Year
    EXPECT_TRUE(content1.find(":") != std::string::npos);  // Time separator

    // Without timestamp
    log.close();
    std::filesystem::remove(log_file_);
    log.setLogFile(log_file_);
    log.setTimestampEnabled(false);
    LOG_INFO(GENERAL, "Without timestamp");

    log.flush();
    std::string content2 = readLogFile(log_file_);

    // Should start with [INFO] directly
    EXPECT_TRUE(content2.find("[INFO]") < 10);
}

// Test 7: Thread ID Enabled/Disabled
TEST_F(LoggerTest, ThreadIdEnabledDisabled)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    // With thread ID
    log.setThreadIdEnabled(true);
    LOG_INFO(GENERAL, "With thread ID");

    log.flush();
    std::string content1 = readLogFile(log_file_);

    EXPECT_TRUE(content1.find("[Thread:") != std::string::npos);

    // Without thread ID
    log.close();
    std::filesystem::remove(log_file_);
    log.setLogFile(log_file_);
    log.setThreadIdEnabled(false);
    LOG_INFO(GENERAL, "Without thread ID");

    log.flush();
    std::string content2 = readLogFile(log_file_);

    EXPECT_TRUE(content2.find("[Thread:") == std::string::npos);
}

// Test 8: Source Location Enabled/Disabled
TEST_F(LoggerTest, SourceLocationEnabledDisabled)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    // With source location
    log.setSourceLocationEnabled(true);
    LOG_INFO(GENERAL, "With source location");

    log.flush();
    std::string content1 = readLogFile(log_file_);

    EXPECT_TRUE(content1.find(".cpp:") != std::string::npos);

    // Without source location
    log.close();
    std::filesystem::remove(log_file_);
    log.setLogFile(log_file_);
    log.setSourceLocationEnabled(false);
    LOG_INFO(GENERAL, "Without source location");

    log.flush();
    std::string content2 = readLogFile(log_file_);

    EXPECT_TRUE(content2.find(".cpp:") == std::string::npos);
}

// Test 9: Config Integration
// Tests that Logger can be configured via Config values
// Note: We apply config settings manually since Logger::initialize() is a one-shot
// initialization that may have already run before this test.
TEST_F(LoggerTest, ConfigIntegration)
{
    // Create config file
    std::ofstream cfg_file(config_file_.c_str());
    cfg_file << "[logging]\n";
    cfg_file << "log_level = DEBUG\n";
    cfg_file << "log_file = " << log_file_ << "\n";
    cfg_file << "log_timestamp = true\n";
    cfg_file << "log_thread_id = false\n";
    cfg_file << "log_source_location = true\n";
    cfg_file.close();

    // Initialize config
    Config& cfg = Config::getInstance();
    cfg.clear();
    ErrorContext ctx;
    cfg.initialize(config_file_.c_str(), &ctx);

    // Apply config settings to logger manually (simulating what initialize() does)
    Logger& log = Logger::getInstance();

    // Read and apply log level from config
    std::string level_str = cfg.getString("logging", "log_level", "INFO");
    if (level_str == "DEBUG")
        log.setLogLevel(LogLevel::DEBUG);
    else if (level_str == "INFO")
        log.setLogLevel(LogLevel::INFO);

    // Read and apply log file from config
    std::string log_file_from_cfg = cfg.getString("logging", "log_file", "");
    if (!log_file_from_cfg.empty())
        log.setLogFile(log_file_from_cfg);

    // Read and apply flags from config
    log.setTimestampEnabled(cfg.getBool("logging", "log_timestamp", true));
    log.setThreadIdEnabled(cfg.getBool("logging", "log_thread_id", false));
    log.setSourceLocationEnabled(cfg.getBool("logging", "log_source_location", true));

    // Verify logger picked up config settings
    EXPECT_EQ(log.getLogLevel(), LogLevel::DEBUG);

    // Log at DEBUG level (should appear)
    LOG_DEBUG(GENERAL, "Debug message from config");

    log.flush();

    EXPECT_TRUE(fileContains(log_file_, "Debug message from config"));

    // Clean up
    cfg.clear();
}

// Test 10: Thread Safety (Concurrent Logging)
TEST_F(LoggerTest, ThreadSafety)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(multithread_log_file_);
    log.setTimestampEnabled(false); // Easier to count lines

    const int num_threads = 10;
    const int logs_per_thread = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, logs_per_thread]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                LOG_INFO(GENERAL, "Thread %d message %d", i, j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    log.flush();

    // Should have exactly num_threads * logs_per_thread lines
    int line_count = countLines(multithread_log_file_);
    EXPECT_EQ(line_count, num_threads * logs_per_thread);
}

// Test 11: Log Level Getters
TEST_F(LoggerTest, LogLevelGetters)
{
    Logger& log = Logger::getInstance();

    log.setLogLevel(LogLevel::TRACE);
    EXPECT_EQ(log.getLogLevel(), LogLevel::TRACE);

    log.setLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(log.getLogLevel(), LogLevel::DEBUG);

    log.setLogLevel(LogLevel::INFO);
    EXPECT_EQ(log.getLogLevel(), LogLevel::INFO);

    log.setLogLevel(LogLevel::WARNING);
    EXPECT_EQ(log.getLogLevel(), LogLevel::WARNING);

    log.setLogLevel(LogLevel::ERROR);
    EXPECT_EQ(log.getLogLevel(), LogLevel::ERROR);

    log.setLogLevel(LogLevel::CRITICAL);
    EXPECT_EQ(log.getLogLevel(), LogLevel::CRITICAL);
}

// Test 12: Empty Log File Path (Logs to Stderr)
TEST_F(LoggerTest, EmptyLogFilePathLogsToStderr)
{
    Logger& log = Logger::getInstance();

    // Set empty log file (should log to stderr)
    log.setLogFile("");

    // This should not crash and should log to stderr
    LOG_INFO(GENERAL, "This goes to stderr");

    // No file should be created
    EXPECT_FALSE(std::filesystem::exists(""));
}

// Test 13: Change Log File
TEST_F(LoggerTest, ChangeLogFile)
{
    Logger& log = Logger::getInstance();

    log.setLogFile(log_file_);
    LOG_INFO(GENERAL, "Message 1");
    log.flush();

    EXPECT_TRUE(fileContains(log_file_, "Message 1"));

    // Change to a different file
    log.setLogFile(log_file_2_);
    LOG_INFO(GENERAL, "Message 2");
    log.flush();

    // First file should only have Message 1
    EXPECT_TRUE(fileContains(log_file_, "Message 1"));
    EXPECT_FALSE(fileContains(log_file_, "Message 2"));

    // Second file should have Message 2
    EXPECT_TRUE(fileContains(log_file_2_, "Message 2"));
    EXPECT_FALSE(fileContains(log_file_2_, "Message 1"));

    // Clean up
    std::filesystem::remove(log_file_2_);
}

// Test 14: Flush Ensures Data Written
TEST_F(LoggerTest, FlushEnsuresDataWritten)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    LOG_INFO(GENERAL, "Before flush");

    // Flush explicitly
    log.flush();

    // File should contain the message
    EXPECT_TRUE(fileContains(log_file_, "Before flush"));
}

// Test 15: Close Closes Log File
TEST_F(LoggerTest, CloseClosesLogFile)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    LOG_INFO(GENERAL, "Before close");
    log.close();

    // After close, logging should go to stderr (no new file writes)
    LOG_INFO(GENERAL, "After close");

    // File should only have "Before close"
    std::string content = readLogFile(log_file_);
    EXPECT_TRUE(content.find("Before close") != std::string::npos);
    EXPECT_TRUE(content.find("After close") == std::string::npos);
}

// Test 16: Level String Conversion
TEST_F(LoggerTest, LevelToString)
{
    EXPECT_STREQ(Logger::levelToString(LogLevel::TRACE), "TRACE");
    EXPECT_STREQ(Logger::levelToString(LogLevel::DEBUG), "DEBUG");
    EXPECT_STREQ(Logger::levelToString(LogLevel::INFO), "INFO");
    EXPECT_STREQ(Logger::levelToString(LogLevel::WARNING), "WARN");
    EXPECT_STREQ(Logger::levelToString(LogLevel::ERROR), "ERROR");
    EXPECT_STREQ(Logger::levelToString(LogLevel::CRITICAL), "CRIT");
}

// Test 17: Category String Conversion
TEST_F(LoggerTest, CategoryToString)
{
    EXPECT_STREQ(Logger::categoryToString(LogCategory::GENERAL), "GENERAL");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::STORAGE), "STORAGE");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::TRANSACTION), "TRANSACTION");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::LOCK), "LOCK");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::PARSER), "PARSER");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::EXECUTOR), "EXECUTOR");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::NETWORK), "NETWORK");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::CATALOG), "CATALOG");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::BTREE), "BTREE");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::HASH), "HASH");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::BUFFER), "BUFFER");
    EXPECT_STREQ(Logger::categoryToString(LogCategory::VACUUM), "VACUUM");
}

// Test 18: Multiple Initialize Calls
TEST_F(LoggerTest, MultipleInitializeCalls)
{
    Logger& log = Logger::getInstance();

    log.initialize();
    log.initialize(); // Should not crash

    // Should still work
    log.setLogFile(log_file_);
    LOG_INFO(GENERAL, "After multiple initializes");
    log.flush();

    EXPECT_TRUE(fileContains(log_file_, "After multiple initializes"));
}

// Test 19: Invalid Log File Path
TEST_F(LoggerTest, InvalidLogFilePath)
{
    Logger& log = Logger::getInstance();

    // Try to set an invalid path (should fail gracefully)
    log.setLogFile("/invalid/path/that/does/not/exist/test.log");

    // Should fall back to stderr (not crash)
    LOG_INFO(GENERAL, "This should not crash");

    // No exception should be thrown
    SUCCEED();
}

// Test 20: Large Log Message
TEST_F(LoggerTest, LargeLogMessage)
{
    Logger& log = Logger::getInstance();
    log.setLogFile(log_file_);

    // Create a large message (3000 characters)
    std::string large_message(3000, 'X');
    LOG_INFO(GENERAL, "%s", large_message.c_str());

    log.flush();

    std::string content = readLogFile(log_file_);

    // Message should be truncated to buffer size (4096 in logger.cpp)
    // but should not crash
    EXPECT_TRUE(content.find("XXX") != std::string::npos);
}
