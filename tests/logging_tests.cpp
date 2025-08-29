#include "scratchbird/telemetry/logging.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <thread>

using scratchbird::telemetry::FileSink;
using scratchbird::telemetry::Logger;
using scratchbird::telemetry::LogLevel;
using scratchbird::telemetry::StdoutSink;

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(LoggingTests, LevelFiltering)
{
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::Warn);
    logger.set_json_format(false);
    logger.clear_sinks();
    logger.add_sink(std::make_shared<StdoutSink>());

    // Should not crash and should filter below WARN
    logger.debug("test", "debug suppressed", __FILE__, __LINE__);
    logger.info("test", "info suppressed", __FILE__, __LINE__);
    logger.warn("test", "warn prints", __FILE__, __LINE__);
    logger.error("test", "error prints", __FILE__, __LINE__);
}

TEST(LoggingTests, FileRotation)
{
    std::string log_path = "/tmp/sb_logging_test.log";
    std::filesystem::remove(log_path);
    std::filesystem::remove(log_path + ".1");
    std::filesystem::remove(log_path + ".2");

    auto& logger = Logger::instance();
    logger.set_level(LogLevel::Info);
    logger.set_json_format(true);
    logger.clear_sinks();
    logger.add_sink(std::make_shared<FileSink>(log_path, 256, 2));

    for (int i = 0; i < 200; ++i) {
        logger.info("rot", "message-" + std::to_string(i), __FILE__, __LINE__);
    }
    logger.flush();

    // Expect current file exists and at least one rotated file exists
    EXPECT_TRUE(std::filesystem::exists(log_path));
    bool rotated_exists =
        std::filesystem::exists(log_path + ".1") || std::filesystem::exists(log_path + ".2");
    EXPECT_TRUE(rotated_exists);
}
