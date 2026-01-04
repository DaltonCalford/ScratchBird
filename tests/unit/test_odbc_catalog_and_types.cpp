#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define private public
#include "scratchbird/odbc/odbc_handles.h"
#include "scratchbird/odbc/odbc_client_bridge.h"
#undef private

using namespace scratchbird::odbc;

namespace {

class FakeOdbcClientBridge : public scratchbird::odbc::OdbcClientBridge {
public:
    SQLRETURN executeSQL(const std::string& sql,
                         std::vector<std::vector<std::string>>& results,
                         std::vector<scratchbird::odbc::ColumnMetadata>& columns,
                         SQLLEN& rows_affected) override {
        (void)columns;
        results.clear();
        rows_affected = 0;

        if (sql == "SHOW TABLES") {
            results = {{"users"}, {"audit_log"}};
            return SQL_SUCCESS;
        }
        if (sql == "SHOW COLUMNS FROM users") {
            results = {
                {"id", "UUID", "NO", "PRI", "", ""},
                {"created_at", "TIMESTAMP", "NO", "", "", ""},
                {"name", "VARCHAR(64)", "YES", "", "", ""}
            };
            return SQL_SUCCESS;
        }
        if (sql == "SHOW COLUMNS FROM audit_log") {
            results = {
                {"event_id", "BIGINT", "NO", "PRI", "", ""},
                {"event_time", "TIMESTAMP", "NO", "", "", ""}
            };
            return SQL_SUCCESS;
        }
        if (sql == "SHOW INDEXES FROM users") {
            results = {{"users", "0", "PRIMARY", "id", "BTREE"}};
            return SQL_SUCCESS;
        }
        if (sql == "SHOW INDEXES FROM audit_log") {
            results = {{"audit_log", "0", "PRIMARY", "event_id", "BTREE"}};
            return SQL_SUCCESS;
        }
        if (sql == "SELECT schema_id, schema_name FROM sb_catalog.sb_schemas") {
            results = {{"schema_public", "public"}};
            return SQL_SUCCESS;
        }
        if (sql == "SELECT table_id, schema_id, table_name FROM sb_catalog.sb_tables") {
            results = {
                {"tbl_users", "schema_public", "users"},
                {"tbl_audit", "schema_public", "audit_log"}
            };
            return SQL_SUCCESS;
        }
        if (sql == "SELECT fk_name, child_table_id, parent_table_id, child_columns, parent_columns, on_update, on_delete, match_type, is_enabled "
                   "FROM sb_catalog.sb_foreign_keys") {
            results = {
                {"fk_audit_user", "tbl_audit", "tbl_users", "user_id,role_id", "id,role_id", "2", "1", "0", "1"}
            };
            return SQL_SUCCESS;
        }

        return SQL_ERROR;
    }
};

class OdbcCatalogTest : public ::testing::Test {
protected:
    scratchbird::odbc::OdbcEnvironment env_{};
    scratchbird::odbc::OdbcConnection conn_{&env_};
    scratchbird::odbc::OdbcStatement stmt_{&conn_};

    void SetUp() override {
        conn_.connected_ = true;
        conn_.current_database_ = "testdb";
        conn_.current_schema_ = "public";
        conn_.client_bridge_ = std::make_unique<FakeOdbcClientBridge>();
    }

    static const SQLCHAR* toSqlChar(const std::string& value) {
        return reinterpret_cast<const SQLCHAR*>(value.c_str());
    }

    static const std::vector<std::string>* findRow(const std::vector<std::vector<std::string>>& rows,
                                                   size_t index,
                                                   const std::string& value) {
        for (const auto& row : rows) {
            if (row.size() > index && row[index] == value) {
                return &row;
            }
        }
        return nullptr;
    }
};

TEST_F(OdbcCatalogTest, TablesHonorsPatterns) {
    std::string table_pattern = "user%";
    SQLRETURN rc = stmt_.tables(nullptr, 0, nullptr, 0,
                                toSqlChar(table_pattern), SQL_NTS,
                                nullptr, 0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_EQ(stmt_.columns_.size(), 5u);
    ASSERT_EQ(stmt_.rows_.size(), 1u);
    EXPECT_EQ(stmt_.columns_[0].name, "TABLE_CAT");
    EXPECT_EQ(stmt_.columns_[1].name, "TABLE_SCHEM");
    EXPECT_EQ(stmt_.columns_[2].name, "TABLE_NAME");
    EXPECT_EQ(stmt_.rows_[0][0], "testdb");
    EXPECT_EQ(stmt_.rows_[0][1], "public");
    EXPECT_EQ(stmt_.rows_[0][2], "users");
    EXPECT_EQ(stmt_.rows_[0][3], "TABLE");

    std::string view_type = "'VIEW'";
    rc = stmt_.tables(nullptr, 0, nullptr, 0, nullptr, 0,
                      toSqlChar(view_type), SQL_NTS);
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_TRUE(stmt_.rows_.empty());
}

TEST_F(OdbcCatalogTest, ColumnsParseTypesAndPrimaryKeys) {
    std::string table_pattern = "users";
    SQLRETURN rc = stmt_.columns(nullptr, 0, nullptr, 0,
                                 toSqlChar(table_pattern), SQL_NTS,
                                 nullptr, 0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_EQ(stmt_.rows_.size(), 3u);

    const auto* id_row = findRow(stmt_.rows_, 3, "id");
    ASSERT_NE(id_row, nullptr);
    EXPECT_EQ((*id_row)[4], std::to_string(SQL_GUID));
    EXPECT_EQ((*id_row)[5], "UUID");

    const auto* ts_row = findRow(stmt_.rows_, 3, "created_at");
    ASSERT_NE(ts_row, nullptr);
    EXPECT_EQ((*ts_row)[4], std::to_string(SQL_TYPE_TIMESTAMP));
    EXPECT_EQ((*ts_row)[5], "TIMESTAMP");

    const auto* name_row = findRow(stmt_.rows_, 3, "name");
    ASSERT_NE(name_row, nullptr);
    EXPECT_EQ((*name_row)[4], std::to_string(SQL_VARCHAR));
    EXPECT_EQ((*name_row)[6], "64");
    EXPECT_EQ((*name_row)[17], "YES");

    rc = stmt_.primaryKeys(nullptr, 0, nullptr, 0, toSqlChar(table_pattern), SQL_NTS);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_EQ(stmt_.rows_.size(), 1u);
    EXPECT_EQ(stmt_.rows_[0][2], "users");
    EXPECT_EQ(stmt_.rows_[0][3], "id");
    EXPECT_EQ(stmt_.rows_[0][5], "PRIMARY");
}

TEST_F(OdbcCatalogTest, StatisticsAndSpecialColumnsUsePrimaryKey) {
    std::string table_pattern = "users";
    SQLRETURN rc = stmt_.statistics(nullptr, 0, nullptr, 0, toSqlChar(table_pattern), SQL_NTS,
                                    0, 0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_EQ(stmt_.rows_.size(), 1u);
    EXPECT_EQ(stmt_.rows_[0][2], "users");
    EXPECT_EQ(stmt_.rows_[0][3], "0");
    EXPECT_EQ(stmt_.rows_[0][5], "PRIMARY");
    EXPECT_EQ(stmt_.rows_[0][8], "id");

    rc = stmt_.specialColumns(0, nullptr, 0, nullptr, 0, toSqlChar(table_pattern), SQL_NTS,
                               0, 0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_EQ(stmt_.rows_.size(), 1u);
    EXPECT_EQ(stmt_.rows_[0][1], "id");
    EXPECT_EQ(stmt_.rows_[0][0], "2");
    EXPECT_EQ(stmt_.rows_[0][7], "1");
}

TEST_F(OdbcCatalogTest, ForeignKeysExposeMappings) {
    std::string pk_table = "users";
    std::string fk_table = "audit_%";
    SQLRETURN rc = stmt_.foreignKeys(nullptr, 0, nullptr, 0,
                                     toSqlChar(pk_table), SQL_NTS,
                                     nullptr, 0, nullptr, 0,
                                     toSqlChar(fk_table), SQL_NTS);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_EQ(stmt_.rows_.size(), 2u);

    const auto* user_row = findRow(stmt_.rows_, 7, "user_id");
    ASSERT_NE(user_row, nullptr);
    EXPECT_EQ((*user_row)[2], "users");
    EXPECT_EQ((*user_row)[6], "audit_log");
    EXPECT_EQ((*user_row)[3], "id");
    EXPECT_EQ((*user_row)[8], "1");
    EXPECT_EQ((*user_row)[9], "0");
    EXPECT_EQ((*user_row)[10], "1");
    EXPECT_EQ((*user_row)[11], "fk_audit_user");
    EXPECT_EQ((*user_row)[12], "PRIMARY");
    EXPECT_EQ((*user_row)[13], "7");

    const auto* role_row = findRow(stmt_.rows_, 7, "role_id");
    ASSERT_NE(role_row, nullptr);
    EXPECT_EQ((*role_row)[3], "role_id");
    EXPECT_EQ((*role_row)[8], "2");
}

TEST(OdbcGetDataTest, TemporalAndGuidConversions) {
    scratchbird::odbc::OdbcEnvironment env;
    scratchbird::odbc::OdbcConnection conn(&env);
    scratchbird::odbc::OdbcStatement stmt(&conn);

    scratchbird::odbc::ColumnMetadata date_col;
    date_col.sql_type = SQL_TYPE_DATE;
    scratchbird::odbc::ColumnMetadata time_col;
    time_col.sql_type = SQL_TYPE_TIME;
    scratchbird::odbc::ColumnMetadata ts_col;
    ts_col.sql_type = SQL_TYPE_TIMESTAMP;
    scratchbird::odbc::ColumnMetadata guid_col;
    guid_col.sql_type = SQL_GUID;

    std::string date_value = "2025-01-02";
    std::string time_value = "13:14:15.123456";
    std::string ts_value = "2025-01-02 13:14:15.654321";
    std::string guid_text = "00112233-4455-6677-8899-aabbccddeeff";
    std::string guid_binary;
    guid_binary.assign(
        reinterpret_cast<const char*>("\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff"),
        16);

    stmt.has_results_ = true;
    stmt.columns_ = {date_col, time_col, ts_col, guid_col};
    stmt.rows_ = {
        {date_value, time_value, ts_value, guid_text},
        {date_value, time_value, ts_value, guid_binary}
    };

    stmt.current_row_ = 1;
    SQL_DATE_STRUCT date_struct{};
    SQLLEN ind = 0;
    ASSERT_EQ(stmt.getData(1, SQL_C_DATE, &date_struct, sizeof(date_struct), &ind), SQL_SUCCESS);
    EXPECT_EQ(date_struct.year, 2025);
    EXPECT_EQ(date_struct.month, 1);
    EXPECT_EQ(date_struct.day, 2);
    EXPECT_EQ(ind, static_cast<SQLLEN>(sizeof(SQL_DATE_STRUCT)));

    SQL_TIME_STRUCT time_struct{};
    ASSERT_EQ(stmt.getData(2, SQL_C_TIME, &time_struct, sizeof(time_struct), &ind), SQL_SUCCESS);
    EXPECT_EQ(time_struct.hour, 13);
    EXPECT_EQ(time_struct.minute, 14);
    EXPECT_EQ(time_struct.second, 15);

    SQL_TIMESTAMP_STRUCT ts_struct{};
    ASSERT_EQ(stmt.getData(3, SQL_C_TIMESTAMP, &ts_struct, sizeof(ts_struct), &ind), SQL_SUCCESS);
    EXPECT_EQ(ts_struct.year, 2025);
    EXPECT_EQ(ts_struct.month, 1);
    EXPECT_EQ(ts_struct.day, 2);
    EXPECT_EQ(ts_struct.hour, 13);
    EXPECT_EQ(ts_struct.minute, 14);
    EXPECT_EQ(ts_struct.second, 15);
    EXPECT_EQ(ts_struct.fraction, 654321000u);

    SQLGUID guid{};
    ASSERT_EQ(stmt.getData(4, SQL_C_GUID, &guid, sizeof(guid), &ind), SQL_SUCCESS);
    EXPECT_EQ(guid.Data1, 0x00112233u);
    EXPECT_EQ(guid.Data2, 0x4455u);
    EXPECT_EQ(guid.Data3, 0x6677u);
    EXPECT_EQ(guid.Data4[0], 0x88u);
    EXPECT_EQ(guid.Data4[7], 0xffu);

    stmt.current_row_ = 2;
    SQLGUID guid_bin{};
    ASSERT_EQ(stmt.getData(4, SQL_C_GUID, &guid_bin, sizeof(guid_bin), &ind), SQL_SUCCESS);
    EXPECT_EQ(guid_bin.Data1, 0x00112233u);
    EXPECT_EQ(guid_bin.Data2, 0x4455u);
    EXPECT_EQ(guid_bin.Data3, 0x6677u);
    EXPECT_EQ(guid_bin.Data4[0], 0x88u);
    EXPECT_EQ(guid_bin.Data4[7], 0xffu);
}

class RecordingClientBridge : public scratchbird::odbc::OdbcClientBridge {
public:
    bool connected{true};
    std::vector<std::string> sql_log;
    SQLRETURN connect(const scratchbird::odbc::ConnectionParams& /*params*/,
                      std::string& /*error*/) override {
        connected = true;
        return SQL_SUCCESS;
    }
    SQLRETURN executeSQL(const std::string& sql,
                         std::vector<std::vector<std::string>>& results,
                         std::vector<scratchbird::odbc::ColumnMetadata>& columns,
                         SQLLEN& rows_affected) override {
        results.clear();
        columns.clear();
        rows_affected = 0;
        sql_log.push_back(sql);
        return SQL_SUCCESS;
    }
    bool isConnected() const override {
        return connected;
    }
};

class SmokeClientBridge : public RecordingClientBridge {
public:
    SQLRETURN executeSQL(const std::string& sql,
                         std::vector<std::vector<std::string>>& results,
                         std::vector<scratchbird::odbc::ColumnMetadata>& columns,
                         SQLLEN& rows_affected) override {
        results.clear();
        columns.clear();
        rows_affected = 0;
        sql_log.push_back(sql);

        if (sql == "SELECT 1") {
            scratchbird::odbc::ColumnMetadata col;
            col.name = "one";
            col.sql_type = SQL_INTEGER;
            columns.push_back(col);
            results.push_back({"1"});
            return SQL_SUCCESS;
        }
        return SQL_SUCCESS;
    }
};

TEST(OdbcAutocommitTest, SetAutocommitSendsConflictClause) {
    scratchbird::odbc::OdbcEnvironment env;
    scratchbird::odbc::OdbcConnection conn(&env);

    auto bridge = std::make_unique<RecordingClientBridge>();
    auto* bridge_ptr = bridge.get();
    conn.client_bridge_ = std::move(bridge);
    conn.connected_ = true;

    SQLRETURN rc = conn.setAttribute(SQL_ATTR_AUTOCOMMIT,
                                     reinterpret_cast<SQLPOINTER>(
                                         static_cast<uintptr_t>(SQL_AUTOCOMMIT_OFF)),
                                     0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(conn.auto_commit_, SQL_AUTOCOMMIT_OFF);
    EXPECT_TRUE(conn.in_transaction_);
    ASSERT_EQ(bridge_ptr->sql_log.size(), 1u);
    EXPECT_EQ(bridge_ptr->sql_log[0], "SET AUTOCOMMIT OFF ON CONFLICT KEEP");

    rc = conn.setAttribute(SQL_ATTR_AUTOCOMMIT,
                           reinterpret_cast<SQLPOINTER>(
                               static_cast<uintptr_t>(SQL_AUTOCOMMIT_ON)),
                           0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(conn.auto_commit_, SQL_AUTOCOMMIT_ON);
    EXPECT_FALSE(conn.in_transaction_);
    ASSERT_EQ(bridge_ptr->sql_log.size(), 2u);
    EXPECT_EQ(bridge_ptr->sql_log[1], "SET AUTOCOMMIT ON ON CONFLICT COMMIT");
}

TEST(OdbcAutocommitTest, IsolationMappingUsesSetTransaction) {
    scratchbird::odbc::OdbcEnvironment env;
    scratchbird::odbc::OdbcConnection conn(&env);

    auto bridge = std::make_unique<RecordingClientBridge>();
    auto* bridge_ptr = bridge.get();
    conn.client_bridge_ = std::move(bridge);
    conn.connected_ = true;

    SQLRETURN rc = conn.setAttribute(SQL_ATTR_TXN_ISOLATION,
                                     reinterpret_cast<SQLPOINTER>(
                                         static_cast<uintptr_t>(SQL_TXN_SERIALIZABLE)),
                                     0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(conn.txn_isolation_, SQL_TXN_SERIALIZABLE);
    ASSERT_EQ(bridge_ptr->sql_log.size(), 2u);
    EXPECT_EQ(bridge_ptr->sql_log[0],
              "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE ON CONFLICT COMMIT");
    EXPECT_EQ(bridge_ptr->sql_log[1], "SET AUTOCOMMIT ON ON CONFLICT COMMIT");
}

TEST(OdbcFetchTest, BindAndFetchPopulateBuffers) {
    scratchbird::odbc::OdbcEnvironment env;
    scratchbird::odbc::OdbcConnection conn(&env);
    scratchbird::odbc::OdbcStatement stmt(&conn);

    scratchbird::odbc::ColumnMetadata int_col;
    int_col.sql_type = SQL_INTEGER;
    scratchbird::odbc::ColumnMetadata text_col;
    text_col.sql_type = SQL_VARCHAR;

    stmt.has_results_ = true;
    stmt.columns_ = {int_col, text_col};
    stmt.rows_ = {{"42", "hello"}, {"100", ""}};
    stmt.current_row_ = 0;

    SQLINTEGER out_int = 0;
    char out_text[16] = {};
    SQLLEN int_ind = 0;
    SQLLEN text_ind = 0;
    SQLUSMALLINT row_status = 0;
    SQLULEN rows_fetched = 0;

    ASSERT_EQ(stmt.bindCol(1, SQL_C_LONG, &out_int, sizeof(out_int), &int_ind), SQL_SUCCESS);
    ASSERT_EQ(stmt.bindCol(2, SQL_C_CHAR, out_text, sizeof(out_text), &text_ind), SQL_SUCCESS);
    stmt.row_status_ptr_ = &row_status;
    stmt.rows_fetched_ptr_ = &rows_fetched;

    SQLRETURN rc = stmt.fetch();
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(out_int, 42);
    EXPECT_STREQ(out_text, "hello");
    EXPECT_EQ(int_ind, static_cast<SQLLEN>(sizeof(SQLINTEGER)));
    EXPECT_EQ(text_ind, static_cast<SQLLEN>(std::strlen("hello")));
    EXPECT_EQ(row_status, SQL_ROW_SUCCESS);
    EXPECT_EQ(rows_fetched, 1u);

    rc = stmt.fetch();
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(out_int, 100);
    EXPECT_EQ(text_ind, SQL_NULL_DATA);
}

TEST(OdbcSmokeTest, ConnectExecFetch) {
    scratchbird::odbc::OdbcEnvironment env;
    scratchbird::odbc::OdbcConnection conn(&env);

    auto bridge = std::make_unique<SmokeClientBridge>();
    auto* bridge_ptr = bridge.get();
    conn.client_bridge_ = std::move(bridge);

    SQLRETURN rc = conn.connect(nullptr, 0, nullptr, 0, nullptr, 0);
    ASSERT_EQ(rc, SQL_SUCCESS);
    ASSERT_TRUE(conn.connected_);
    ASSERT_GE(bridge_ptr->sql_log.size(), 2u);
    EXPECT_EQ(bridge_ptr->sql_log[0],
              "SET TRANSACTION ISOLATION LEVEL READ COMMITTED ON CONFLICT COMMIT");
    EXPECT_EQ(bridge_ptr->sql_log[1], "SET AUTOCOMMIT ON ON CONFLICT COMMIT");

    auto* stmt = conn.createStatement();
    ASSERT_NE(stmt, nullptr);
    rc = stmt->execDirect(reinterpret_cast<const SQLCHAR*>("SELECT 1"), SQL_NTS);
    ASSERT_EQ(rc, SQL_SUCCESS);

    SQLINTEGER out_value = 0;
    SQLLEN ind = 0;
    ASSERT_EQ(stmt->bindCol(1, SQL_C_LONG, &out_value, sizeof(out_value), &ind), SQL_SUCCESS);
    rc = stmt->fetch();
    ASSERT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(out_value, 1);
}

} // namespace
