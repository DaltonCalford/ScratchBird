#include "gtest/gtest.h"

#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"

#include <filesystem>

using namespace scratchbird;
using namespace scratchbird::protocol;

namespace {
std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

template <typename T>
class AdapterHarness : public T {
public:
    using T::T;
    core::Status runCompile(const std::string& sql, std::vector<uint8_t>& bytecode, std::string& err) {
        return T::compileQuery(sql, bytecode, err);
    }
};

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}
} // namespace

TEST(ProtocolAdapterDialects, PostgreSQLSelectUsesPgCompiler) {
    cleanupDb("test_pg_adapter.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_adapter.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    auto status = adapter.runCompile("SELECT 1", bytecode, err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}

TEST(ProtocolAdapterDialects, MySQLSelectUsesMysqlParser) {
    cleanupDb("test_mysql_adapter.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_adapter.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    auto status = adapter.runCompile("SELECT 1 FROM dual", bytecode, err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}

TEST(ProtocolAdapterDialects, FirebirdSelectUsesFirebirdParser) {
    cleanupDb("test_fb_adapter.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_adapter.sbdb").string();

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    auto status = adapter.runCompile("SELECT 1 FROM RDB$DATABASE", bytecode, err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}
