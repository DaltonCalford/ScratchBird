/**
 * COPY executor tests (CSV/DELIMITER/NULL/HEADER)
 */

#include <gtest/gtest.h>
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

static std::string makeUniquePath(const std::string& prefix, const std::string& suffix) {
    std::ostringstream oss;
    oss << "/tmp/" << prefix << "_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << suffix;
    return oss.str();
}

class CopyExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = makeUniquePath("test_copy_executor", ".sbdb");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        auto status = Database::create(db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema";
        public_schema_id_ = public_schema_info.schema_id;

        EnsureUser(catalog_, "test_user");

        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        executor_ = std::make_unique<Executor>(&db_);

        status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create connection";
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        auto system_user_id = catalog_->getSystemUserId(&ctx);
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
    }

    void TearDown() override {
        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(CopyExecutorTest, CopyCsvWithHeaderAndNulls) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_src (id INT, name VARCHAR(10), note VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO copy_src VALUES (1, 'alpha', NULL), "
        "(2, NULL, 'beta'), (3, 'a,b', 'c\"d')").success());

    std::string path = makeUniquePath("copy_out", ".csv");
    std::string sql = "COPY copy_src TO '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', NULL 'NULL', HEADER true)";
    auto copy_out = compileAndExecute(sql);
    ASSERT_TRUE(copy_out.success()) << copy_out.error();

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());

    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "id,name,note");
    EXPECT_EQ(lines[1], "1,alpha,NULL");
    EXPECT_EQ(lines[2], "2,NULL,beta");
    EXPECT_EQ(lines[3], "3,\"a,b\",\"c\"\"d\"");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyCsvFromWithHeaderAndDelimiter) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_dst (id INT, name VARCHAR(10), note VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id|name|note\n";
        out << "1|alpha|NULL\n";
        out << "2|NULL|beta\n";
        out << "3|\"a|b\"|\"c\"\"d\"\n";
    }

    std::string sql = "COPY copy_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER '|', NULL 'NULL', HEADER true)";
    auto copy_in = compileAndExecute(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    auto select = compileAndExecute("SELECT id, name, note FROM copy_dst ORDER BY id");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());

    auto* results = select.resultSet();
    ASSERT_EQ(results->rowCount(), 3u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "1");
    EXPECT_EQ(results->getValue(0, 1).toString(), "alpha");
    EXPECT_TRUE(results->getValue(0, 2).isNull());

    EXPECT_EQ(results->getValue(1, 0).toString(), "2");
    EXPECT_TRUE(results->getValue(1, 1).isNull());
    EXPECT_EQ(results->getValue(1, 2).toString(), "beta");

    EXPECT_EQ(results->getValue(2, 0).toString(), "3");
    EXPECT_EQ(results->getValue(2, 1).toString(), "a|b");
    EXPECT_EQ(results->getValue(2, 2).toString(), "c\"d");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyEncodingUtf8Accepted) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_enc (id INT, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO copy_enc VALUES (1, 'alpha')").success());

    std::string path = makeUniquePath("copy_enc_utf8", ".csv");
    std::string sql = "COPY copy_enc TO '" + path +
                      "' WITH (FORMAT csv, ENCODING 'UTF8')";
    auto copy_out = compileAndExecute(sql);
    ASSERT_TRUE(copy_out.success()) << copy_out.error();

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    std::string line;
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_FALSE(line.empty());

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyEncodingUnsupportedRejected) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_enc_bad (id INT, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_enc_bad", ".csv");
    std::string sql = "COPY copy_enc_bad TO '" + path +
                      "' WITH (FORMAT csv, ENCODING 'LATIN1')";
    auto copy_out = compileAndExecute(sql);
    ASSERT_FALSE(copy_out.success());
    EXPECT_NE(copy_out.error().find("COPY ENCODING"), std::string::npos);

    std::filesystem::remove(path);
}
