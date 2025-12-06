#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/uuidv7.h"
#include <filesystem>
#include <cstring>

using namespace scratchbird::core;

class CharsetCatalogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Use unique path per test to avoid conflicts during parallel execution
        test_db_path_ = "/tmp/test_charset_catalog_" + std::to_string(getpid()) + "_" +
                        std::to_string(test_counter_++) + ".sbdb";
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
        // Also remove lock file
        if (std::filesystem::exists(test_db_path_ + "-lock"))
        {
            std::filesystem::remove(test_db_path_ + "-lock");
        }

        // First create the database file using the static method
        ErrorContext ctx;
        Status status = Database::create(test_db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create database: " << ctx.message;

        // Then open it
        db_ = std::make_unique<Database>();
        status = db_->open(test_db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open database: " << ctx.message;
    }

    void TearDown() override
    {
        db_.reset();
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
        if (std::filesystem::exists(test_db_path_ + "-lock"))
        {
            std::filesystem::remove(test_db_path_ + "-lock");
        }
    }

    std::string test_db_path_;
    std::unique_ptr<Database> db_;
    static inline std::atomic<int> test_counter_{0};
};

// ========== Character Set Tests ==========

TEST_F(CharsetCatalogTest, CreateCharset)
{
    ErrorContext ctx;

    CatalogManager::CharsetInfo cs_info;
    cs_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF8);
    cs_info.name = "utf8";
    cs_info.description = "UTF-8 Unicode";
    cs_info.min_bytes = 1;
    cs_info.max_bytes = 4;
    cs_info.variable_width = 1;
    cs_info.default_collation_id = 100;

    Status status = db_->catalog_manager()->createCharset(cs_info, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create charset: " << ctx.message;
}

TEST_F(CharsetCatalogTest, GetCharsetById)
{
    ErrorContext ctx;

    // Create charset
    CatalogManager::CharsetInfo cs_info;
    cs_info.charset_id = static_cast<uint16_t>(CharacterSet::LATIN1);
    cs_info.name = "latin1";
    cs_info.description = "ISO-8859-1 Western European";
    cs_info.min_bytes = 1;
    cs_info.max_bytes = 1;
    cs_info.variable_width = 0;
    cs_info.default_collation_id = 10;

    Status status = db_->catalog_manager()->createCharset(cs_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Retrieve charset
    CatalogManager::CharsetInfo retrieved;
    status = db_->catalog_manager()->getCharset(cs_info.charset_id, retrieved, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(retrieved.charset_id, cs_info.charset_id);
    EXPECT_EQ(retrieved.name, cs_info.name);
    EXPECT_EQ(retrieved.description, cs_info.description);
    EXPECT_EQ(retrieved.min_bytes, cs_info.min_bytes);
    EXPECT_EQ(retrieved.max_bytes, cs_info.max_bytes);
    EXPECT_EQ(retrieved.variable_width, cs_info.variable_width);
    EXPECT_EQ(retrieved.default_collation_id, cs_info.default_collation_id);
}

TEST_F(CharsetCatalogTest, GetCharsetByName)
{
    ErrorContext ctx;

    // Create charset
    CatalogManager::CharsetInfo cs_info;
    cs_info.charset_id = static_cast<uint16_t>(CharacterSet::ASCII);
    cs_info.name = "ascii";
    cs_info.description = "7-bit ASCII";
    cs_info.min_bytes = 1;
    cs_info.max_bytes = 1;
    cs_info.variable_width = 0;
    cs_info.default_collation_id = 1;

    Status status = db_->catalog_manager()->createCharset(cs_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Retrieve by name
    CatalogManager::CharsetInfo retrieved;
    status = db_->catalog_manager()->getCharsetByName("ascii", retrieved, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(retrieved.charset_id, cs_info.charset_id);
    EXPECT_EQ(retrieved.name, "ascii");
}

TEST_F(CharsetCatalogTest, ListCharsets)
{
    ErrorContext ctx;

    // Create multiple charsets
    std::vector<CatalogManager::CharsetInfo> test_charsets = {
        {static_cast<uint16_t>(CharacterSet::ASCII), "ascii", "7-bit ASCII", 1, 1, 0, 0, 1, 0, 0},
        {static_cast<uint16_t>(CharacterSet::UTF8), "utf8", "UTF-8 Unicode", 1, 4, 1, 0, 100, 0, 0},
        {static_cast<uint16_t>(CharacterSet::LATIN1), "latin1", "ISO-8859-1", 1, 1, 0, 0, 10, 0, 0}
    };

    for (const auto& cs : test_charsets)
    {
        Status status = db_->catalog_manager()->createCharset(cs, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // List all charsets
    std::vector<CatalogManager::CharsetInfo> charsets;
    Status status = db_->catalog_manager()->listCharsets(charsets, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(charsets.size(), 3);
}

TEST_F(CharsetCatalogTest, UpdateCharset)
{
    ErrorContext ctx;

    // Create charset
    CatalogManager::CharsetInfo cs_info;
    cs_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF8);
    cs_info.name = "utf8";
    cs_info.description = "Original description";
    cs_info.min_bytes = 1;
    cs_info.max_bytes = 4;
    cs_info.variable_width = 1;
    cs_info.default_collation_id = 100;

    Status status = db_->catalog_manager()->createCharset(cs_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Update charset
    cs_info.description = "Updated UTF-8 Unicode description";
    cs_info.default_collation_id = 101;

    status = db_->catalog_manager()->updateCharset(cs_info.charset_id, cs_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify update
    CatalogManager::CharsetInfo retrieved;
    status = db_->catalog_manager()->getCharset(cs_info.charset_id, retrieved, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(retrieved.description, "Updated UTF-8 Unicode description");
    EXPECT_EQ(retrieved.default_collation_id, 101);
}

TEST_F(CharsetCatalogTest, DeleteCharset)
{
    ErrorContext ctx;

    // Create charset
    CatalogManager::CharsetInfo cs_info;
    cs_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF16);
    cs_info.name = "utf16";
    cs_info.description = "UTF-16 Unicode";
    cs_info.min_bytes = 2;
    cs_info.max_bytes = 4;
    cs_info.variable_width = 1;
    cs_info.default_collation_id = 200;

    Status status = db_->catalog_manager()->createCharset(cs_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Delete charset
    status = db_->catalog_manager()->deleteCharset(cs_info.charset_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify deletion (should not be found)
    CatalogManager::CharsetInfo retrieved;
    status = db_->catalog_manager()->getCharset(cs_info.charset_id, retrieved, &ctx);
    EXPECT_NE(status, Status::OK);
}

// ========== Collation Tests ==========

TEST_F(CharsetCatalogTest, CreateCollation)
{
    ErrorContext ctx;

    CatalogManager::CollationCatalogInfo col_info;
    col_info.collation_id = 100;
    col_info.name = "utf8_general_ci";
    col_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF8);
    col_info.collation_type = static_cast<uint8_t>(CollationType::CASE_INSENSITIVE);
    col_info.strength = static_cast<uint8_t>(CollationStrength::TERTIARY);
    col_info.pad_space = 1;
    col_info.is_default = 1;
    std::strncpy(col_info.locale, "en_US", sizeof(col_info.locale) - 1);

    Status status = db_->catalog_manager()->createCollation(col_info, &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to create collation: " << ctx.message;
}

TEST_F(CharsetCatalogTest, GetCollationById)
{
    ErrorContext ctx;

    // Create collation
    CatalogManager::CollationCatalogInfo col_info;
    col_info.collation_id = 101;
    col_info.name = "utf8_unicode_ci";
    col_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF8);
    col_info.collation_type = static_cast<uint8_t>(CollationType::UNICODE);
    col_info.strength = static_cast<uint8_t>(CollationStrength::TERTIARY);
    col_info.pad_space = 1;
    col_info.is_default = 0;
    std::strncpy(col_info.locale, "", sizeof(col_info.locale) - 1);

    Status status = db_->catalog_manager()->createCollation(col_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Retrieve collation
    CatalogManager::CollationCatalogInfo retrieved;
    status = db_->catalog_manager()->getCollation(col_info.collation_id, retrieved, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(retrieved.collation_id, col_info.collation_id);
    EXPECT_EQ(retrieved.name, col_info.name);
    EXPECT_EQ(retrieved.charset_id, col_info.charset_id);
    EXPECT_EQ(retrieved.collation_type, col_info.collation_type);
    EXPECT_EQ(retrieved.strength, col_info.strength);
}

TEST_F(CharsetCatalogTest, GetCollationByName)
{
    ErrorContext ctx;

    // Create collation
    CatalogManager::CollationCatalogInfo col_info;
    col_info.collation_id = 10;
    col_info.name = "latin1_general_ci";
    col_info.charset_id = static_cast<uint16_t>(CharacterSet::LATIN1);
    col_info.collation_type = static_cast<uint8_t>(CollationType::CASE_INSENSITIVE);
    col_info.strength = static_cast<uint8_t>(CollationStrength::TERTIARY);
    col_info.pad_space = 1;
    col_info.is_default = 1;

    Status status = db_->catalog_manager()->createCollation(col_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Retrieve by name
    CatalogManager::CollationCatalogInfo retrieved;
    status = db_->catalog_manager()->getCollationByName("latin1_general_ci", retrieved, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(retrieved.collation_id, col_info.collation_id);
    EXPECT_EQ(retrieved.name, "latin1_general_ci");
}

TEST_F(CharsetCatalogTest, ListCollations)
{
    ErrorContext ctx;

    // Create multiple collations
    std::vector<CatalogManager::CollationCatalogInfo> test_collations = {
        {100, "utf8_bin", static_cast<uint16_t>(CharacterSet::UTF8),
         static_cast<uint8_t>(CollationType::BINARY), static_cast<uint8_t>(CollationStrength::IDENTICAL),
         1, 0, 0, {0}, 0, 0},
        {101, "utf8_general_ci", static_cast<uint16_t>(CharacterSet::UTF8),
         static_cast<uint8_t>(CollationType::CASE_INSENSITIVE), static_cast<uint8_t>(CollationStrength::TERTIARY),
         1, 1, 0, {0}, 0, 0},
        {10, "latin1_bin", static_cast<uint16_t>(CharacterSet::LATIN1),
         static_cast<uint8_t>(CollationType::BINARY), static_cast<uint8_t>(CollationStrength::IDENTICAL),
         1, 0, 0, {0}, 0, 0}
    };

    for (const auto& col : test_collations)
    {
        Status status = db_->catalog_manager()->createCollation(col, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // List all collations
    std::vector<CatalogManager::CollationCatalogInfo> collations;
    Status status = db_->catalog_manager()->listCollations(collations, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(collations.size(), 3);
}

TEST_F(CharsetCatalogTest, ListCollationsForCharset)
{
    ErrorContext ctx;

    // Create collations for different charsets
    std::vector<CatalogManager::CollationCatalogInfo> test_collations = {
        {100, "utf8_bin", static_cast<uint16_t>(CharacterSet::UTF8),
         static_cast<uint8_t>(CollationType::BINARY), static_cast<uint8_t>(CollationStrength::IDENTICAL),
         1, 0, 0, {0}, 0, 0},
        {101, "utf8_general_ci", static_cast<uint16_t>(CharacterSet::UTF8),
         static_cast<uint8_t>(CollationType::CASE_INSENSITIVE), static_cast<uint8_t>(CollationStrength::TERTIARY),
         1, 1, 0, {0}, 0, 0},
        {10, "latin1_bin", static_cast<uint16_t>(CharacterSet::LATIN1),
         static_cast<uint8_t>(CollationType::BINARY), static_cast<uint8_t>(CollationStrength::IDENTICAL),
         1, 0, 0, {0}, 0, 0}
    };

    for (const auto& col : test_collations)
    {
        Status status = db_->catalog_manager()->createCollation(col, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // List collations for UTF8 only
    std::vector<CatalogManager::CollationCatalogInfo> utf8_collations;
    Status status = db_->catalog_manager()->listCollationsForCharset(
        static_cast<uint16_t>(CharacterSet::UTF8), utf8_collations, &ctx);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(utf8_collations.size(), 2);

    // Verify all returned collations are for UTF8
    for (const auto& col : utf8_collations)
    {
        EXPECT_EQ(col.charset_id, static_cast<uint16_t>(CharacterSet::UTF8));
    }
}

// P0-8: Enabled after implementing updateCollation
TEST_F(CharsetCatalogTest, UpdateCollation)
{
    ErrorContext ctx;

    // Create collation
    CatalogManager::CollationCatalogInfo col_info;
    col_info.collation_id = 102;
    col_info.name = "utf8_test_ci";
    col_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF8);
    col_info.collation_type = static_cast<uint8_t>(CollationType::CASE_INSENSITIVE);
    col_info.strength = static_cast<uint8_t>(CollationStrength::TERTIARY);
    col_info.pad_space = 1;
    col_info.is_default = 0;

    Status status = db_->catalog_manager()->createCollation(col_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Update collation
    col_info.strength = static_cast<uint8_t>(CollationStrength::SECONDARY);
    col_info.is_default = 1;

    status = db_->catalog_manager()->updateCollation(col_info.collation_id, col_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify update
    CatalogManager::CollationCatalogInfo retrieved;
    status = db_->catalog_manager()->getCollation(col_info.collation_id, retrieved, &ctx);
    ASSERT_EQ(status, Status::OK);

    EXPECT_EQ(retrieved.strength, static_cast<uint8_t>(CollationStrength::SECONDARY));
    EXPECT_EQ(retrieved.is_default, 1);
}

TEST_F(CharsetCatalogTest, DeleteCollation)
{
    ErrorContext ctx;

    // Create collation
    CatalogManager::CollationCatalogInfo col_info;
    col_info.collation_id = 999;
    col_info.name = "test_to_delete";
    col_info.charset_id = static_cast<uint16_t>(CharacterSet::UTF8);
    col_info.collation_type = static_cast<uint8_t>(CollationType::BINARY);
    col_info.strength = static_cast<uint8_t>(CollationStrength::IDENTICAL);
    col_info.pad_space = 1;
    col_info.is_default = 0;

    Status status = db_->catalog_manager()->createCollation(col_info, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Delete collation
    status = db_->catalog_manager()->deleteCollation(col_info.collation_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify deletion
    CatalogManager::CollationCatalogInfo retrieved;
    status = db_->catalog_manager()->getCollation(col_info.collation_id, retrieved, &ctx);
    EXPECT_NE(status, Status::OK);
}

// ========== CharsetManager Integration Tests ==========

TEST_F(CharsetCatalogTest, CharsetManagerLoadFromCatalog)
{
    ErrorContext ctx;

    // Create charsets in catalog
    std::vector<CatalogManager::CharsetInfo> test_charsets = {
        {static_cast<uint16_t>(CharacterSet::UTF8), "utf8", "UTF-8 Unicode", 1, 4, 1, 0, 100, 0, 0},
        {static_cast<uint16_t>(CharacterSet::LATIN1), "latin1", "ISO-8859-1", 1, 1, 0, 0, 10, 0, 0}
    };

    for (const auto& cs : test_charsets)
    {
        Status status = db_->catalog_manager()->createCharset(cs, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Create collations in catalog
    std::vector<CatalogManager::CollationCatalogInfo> test_collations = {
        {100, "utf8_general_ci", static_cast<uint16_t>(CharacterSet::UTF8),
         static_cast<uint8_t>(CollationType::CASE_INSENSITIVE), static_cast<uint8_t>(CollationStrength::TERTIARY),
         1, 1, 0, {0}, 0, 0},
        {10, "latin1_general_ci", static_cast<uint16_t>(CharacterSet::LATIN1),
         static_cast<uint8_t>(CollationType::CASE_INSENSITIVE), static_cast<uint8_t>(CollationStrength::TERTIARY),
         1, 1, 0, {0}, 0, 0}
    };

    for (const auto& col : test_collations)
    {
        Status status = db_->catalog_manager()->createCollation(col, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Load into CharsetManager
    CharsetManager charset_mgr;
    Status status = charset_mgr.loadFromCatalog(db_.get(), &ctx);
    ASSERT_EQ(status, Status::OK) << "Failed to load from catalog: " << ctx.message;

    // Verify charsets loaded
    const auto* utf8_info = charset_mgr.getCharsetInfo(CharacterSet::UTF8);
    ASSERT_NE(utf8_info, nullptr);
    EXPECT_EQ(utf8_info->name, "utf8");

    const auto* latin1_info = charset_mgr.getCharsetInfo(CharacterSet::LATIN1);
    ASSERT_NE(latin1_info, nullptr);
    EXPECT_EQ(latin1_info->name, "latin1");

    // Verify collations loaded
    const auto* utf8_collation = charset_mgr.getCollationInfo(100);
    ASSERT_NE(utf8_collation, nullptr);
    EXPECT_EQ(utf8_collation->name, "utf8_general_ci");

    const auto* latin1_collation = charset_mgr.getCollationInfo(10);
    ASSERT_NE(latin1_collation, nullptr);
    EXPECT_EQ(latin1_collation->name, "latin1_general_ci");
}

TEST_F(CharsetCatalogTest, CharsetManagerEmptyCatalogUsesDefaults)
{
    ErrorContext ctx;

    // Don't create any charsets/collations in catalog
    // CharsetManager should fall back to hardcoded defaults

    CharsetManager charset_mgr;
    Status status = charset_mgr.loadFromCatalog(db_.get(), &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify defaults are available
    const auto* utf8_info = charset_mgr.getCharsetInfo(CharacterSet::UTF8);
    ASSERT_NE(utf8_info, nullptr);
    EXPECT_EQ(utf8_info->name, "utf8");
}
