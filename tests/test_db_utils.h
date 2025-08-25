#ifndef SCRATCHBIRD_TESTS_TEST_DB_UTILS_H
#define SCRATCHBIRD_TESTS_TEST_DB_UTILS_H

#include "scratchbird/capi.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"

#include <filesystem>
#include <string>
#include <unistd.h>

namespace scratchbird::tests
{

    /**
     * Shared database utilities for ScratchBird tests.
     * Implements the pattern where tests check if a database exists first,
     * and only create it if necessary.
     */
    class TestDatabase
    {
      public:
        /**
         * Get or create a test database for the current test process.
         * Uses a unique path per process to avoid conflicts.
         */
        static std::string get_or_create_test_db(const std::string& test_name)
        {
            std::string db_path = get_test_db_path(test_name);

            if (!database_exists(db_path)) {
                create_test_database(db_path);
            }

            // Set the executor to use this database
            scratchbird::engine::set_executor_db_path(db_path);

            return db_path;
        }

        /**
         * Create a fresh test database (always creates new, removes existing)
         */
        static std::string create_fresh_test_db(const std::string& test_name)
        {
            std::string db_path = get_test_db_path(test_name);

            // Remove existing database if present
            cleanup_test_database(db_path);

            // Create new database
            create_test_database(db_path);

            // Set the executor to use this database
            scratchbird::engine::set_executor_db_path(db_path);

            return db_path;
        }

        /**
         * Cleanup test database files
         */
        static void cleanup_test_database(const std::string& db_path)
        {
            // Best-effort: remove segment files db.seg0..db.seg15
            for (int i = 0; i < 16; ++i) {
                std::string seg = db_path + ".seg" + std::to_string(i);
                std::filesystem::remove(seg);
            }
            // Also try to remove any .bootstrap.sql file
            std::string bootstrap = db_path + ".bootstrap.sql";
            std::filesystem::remove(bootstrap);
        }

      private:
        static std::string get_test_db_path(const std::string& test_name)
        {
            return std::string("/tmp/scratchbird_test_") + test_name + "_" +
                   std::to_string(::getpid());
        }

        static bool database_exists(const std::string& db_path)
        {
            return std::filesystem::exists(db_path + ".seg0");
        }

        static void create_test_database(const std::string& db_path)
        {
            SB_CreateDbOptions options{};
            options.page_size = 4096;
            SB_Database* db = nullptr;

            auto status = sb_create_database(db_path.c_str(), &options, &db);
            if (db) {
                sb_close_database(db);
            }

            // Bootstrap catalog if needed
            scratchbird::engine::CatalogManager cm(db_path);
            cm.bootstrap_if_needed();
        }
    };

    /**
     * RAII helper for test database lifecycle management
     */
    class TestDatabaseRAII
    {
      public:
        explicit TestDatabaseRAII(const std::string& test_name, bool fresh = false)
            : db_path_(fresh ? TestDatabase::create_fresh_test_db(test_name)
                             : TestDatabase::get_or_create_test_db(test_name))
        {
        }

        ~TestDatabaseRAII()
        {
            TestDatabase::cleanup_test_database(db_path_);
        }

        const std::string& path() const
        {
            return db_path_;
        }

      private:
        std::string db_path_;
    };

} // namespace scratchbird::tests

#endif // SCRATCHBIRD_TESTS_TEST_DB_UTILS_H
