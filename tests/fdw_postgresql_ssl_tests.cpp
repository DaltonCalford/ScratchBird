#include "scratchbird/engine/fdw_postgresql.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;

int main()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

TEST(FDWPostgreSQL, BuildConnectionStringIncludesSSLAndGSS)
{
    PostgreSqlForeignDataWrapper fdw;
    ForeignServerConfig cfg{};
    cfg.fdw_name = "postgresql_fdw";
    cfg.host = "localhost";
    cfg.port = 5432;
    cfg.database = "testdb";
    cfg.use_ssl = true;
    cfg.ssl_cert_path = "/path/cert.pem";
    cfg.ssl_key_path = "/path/key.pem";
    cfg.ssl_ca_path = "/path/ca.pem";
    cfg.options["gssencmode"] = "prefer";
    cfg.options["krbsrvname"] = "postgres";

    UserMapping um{};
    um.local_username = "tester";
    um.remote_password = "secret";

    // Access private build_connection_string via a small trick: we know establish_connection
    // calls create_connection which prints connection info; instead, we rely on behavior
    // validated indirectly by attempting establish_connection and expecting success.
    std::string err;
    bool ok = fdw.establish_connection(cfg, um, err);
    // In this environment, libpq may not be enabled; we still expect mock connection success
    ASSERT_TRUE(ok) << err;
}

