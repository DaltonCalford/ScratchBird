#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;
using ObjectType = CatalogManager::ObjectType;

namespace {

void appendUVarint(std::vector<uint8_t>& bytecode, uint64_t value)
{
    size_t offset = bytecode.size();
    bytecode.resize(offset + 10);
    size_t count = scratchbird::sblr::writeUVarint(&bytecode[offset], value);
    bytecode.resize(offset + count);
}

void appendUint32(std::vector<uint8_t>& bytecode, uint32_t value)
{
    size_t offset = bytecode.size();
    bytecode.resize(offset + 4);
    scratchbird::sblr::writeInt32(&bytecode[offset], value);
}

void appendString(std::vector<uint8_t>& bytecode, const std::string& value)
{
    appendUVarint(bytecode, static_cast<uint64_t>(value.size()));
    bytecode.insert(bytecode.end(), value.begin(), value.end());
}

std::vector<uint8_t> startBytecode(scratchbird::sblr::Opcode op)
{
    std::vector<uint8_t> bytecode;
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::SBLR_VERSION));
    bytecode.push_back(static_cast<uint8_t>(op));
    return bytecode;
}

std::vector<uint8_t> buildCreateIndexBytecode(
    const std::string& index_name,
    const std::string& table_name,
    const std::vector<std::string>& column_names,
    bool is_unique = false,
    scratchbird::core::CatalogManager::IndexType index_type =
        scratchbird::core::CatalogManager::IndexType::BTREE)
{
    auto bytecode = startBytecode(scratchbird::sblr::Opcode::CREATE_INDEX);
    appendString(bytecode, index_name);
    appendString(bytecode, table_name);
    bytecode.push_back(is_unique ? 1 : 0);
    appendUint32(bytecode, static_cast<uint32_t>(column_names.size()));
    for (const auto& col : column_names)
    {
        appendString(bytecode, col);
    }
    appendUint32(bytecode, 0);  // Include column count
    appendString(bytecode, "");  // Tablespace name
    bytecode.push_back(static_cast<uint8_t>(index_type));
    bytecode.push_back(0);  // has expressions
    bytecode.push_back(0);  // has predicate
    return bytecode;
}

std::vector<uint8_t> buildDropTableBytecode(const std::string& table_name, bool if_exists = false,
                                            bool cascade = false)
{
    auto bytecode = startBytecode(scratchbird::sblr::Opcode::DROP_TABLE);
    appendString(bytecode, table_name);
    uint8_t flags = 0;
    if (if_exists)
    {
        flags |= 0x01;
    }
    if (cascade)
    {
        flags |= 0x02;
    }
    bytecode.push_back(flags);
    return bytecode;
}

std::vector<uint8_t> buildDropIndexBytecode(const std::string& index_name, bool if_exists = false)
{
    auto bytecode = startBytecode(scratchbird::sblr::Opcode::DROP_INDEX);
    appendString(bytecode, index_name);
    bytecode.push_back(if_exists ? 1 : 0);
    return bytecode;
}

std::vector<uint8_t> buildCreateTableBytecode(const std::string& table_name,
                                              const std::vector<std::string>& column_names)
{
    auto bytecode = startBytecode(scratchbird::sblr::Opcode::CREATE_TABLE);
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::TABLE_REF));
    bytecode.push_back(0);  // ref_kind: name
    appendString(bytecode, table_name);
    appendString(bytecode, "");  // alias
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::BEGIN_LIST));
    appendUVarint(bytecode, static_cast<uint64_t>(column_names.size()));
    for (const auto& col_name : column_names)
    {
        bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::COLUMN_DEF));
        bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::COLUMN_REF));
        appendString(bytecode, "");
        appendString(bytecode, col_name);
        bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::TYPE_INTEGER));
    }
    bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::END_LIST));
    appendString(bytecode, "");
    return bytecode;
}

std::vector<uint8_t> buildTruncateTableBytecode(const std::string& table_name, bool sync = false)
{
    auto bytecode = startBytecode(scratchbird::sblr::Opcode::TRUNCATE_TABLE);
    appendString(bytecode, table_name);
    bytecode.push_back(sync ? 1 : 0);
    return bytecode;
}

std::vector<uint8_t> buildAlterTableRenameColumnBytecode(
    const std::string& table_name,
    const std::string& old_name,
    const std::string& new_name)
{
    auto bytecode = startBytecode(scratchbird::sblr::Opcode::ALTER_TABLE);
    appendString(bytecode, table_name);
    bytecode.push_back(5);  // RENAME_COLUMN
    appendString(bytecode, old_name);
    appendString(bytecode, new_name);
    return bytecode;
}

}  // namespace

class SchemaPathResolutionTest : public ::testing::Test
{
protected:
    std::string test_db_path_;
    Database* db_ = nullptr;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID system_user_id_{};

    void SetUp() override
    {
        test_db_path_ = "/tmp/test_schema_path_" + std::to_string(getpid()) + ".db";
        std::remove(test_db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_, 16384, &ctx), Status::OK);

        db_ = new Database();
        ASSERT_EQ(db_->open(test_db_path_, &ctx), Status::OK);

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK);
        ConnectionContext::setCurrent(conn_.get());

        system_user_id_ = catalog_->getSystemUserId(&ctx);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_) {
            db_->close();
            delete db_;
            db_ = nullptr;
            catalog_ = nullptr;
        }
        std::remove(test_db_path_.c_str());
    }

    CatalogManager::ColumnInfo makeColumn(const std::string& name) const
    {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = static_cast<uint16_t>(DataType::INT32);
        col.max_length = 4;
        col.nullable = false;
        return col;
    }

    ID createSchemaPath(const std::string& path)
    {
        ErrorContext ctx;
        ID schema_id;
        auto status = catalog_->createSchemaPath(path, CatalogManager::SchemaType::APPLICATION,
                                                 schema_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return schema_id;
    }

    ID schemaIdForPath(const std::string& path)
    {
        ErrorContext ctx;
        CatalogManager::SchemaInfo info;
        auto status = catalog_->getSchema(path, info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.schema_id;
    }

    ID createDelimitedSchema(const ID& parent_schema_id, const std::string& name)
    {
        CatalogManager::SchemaInfo schema;
        schema.schema_id = generateUuidV7();
        schema.parent_schema_id = parent_schema_id;
        schema.schema_name = name;
        schema.name_is_delimited = true;
        schema.permissions = 0x0FFF;
        schema.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        schema.last_modified_time = schema.created_time;

        ErrorContext ctx;
        auto status = catalog_->writeSchemaRecord(schema, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        status = catalog_->readSchemaRecords(&ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        return schema.schema_id;
    }

    ID createTable(const ID& schema_id, const std::string& table_name)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns{makeColumn("id")};
        ID table_id;
        auto status = catalog_->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    ID createView(const ID& schema_id, const std::string& name)
    {
        ErrorContext ctx;
        auto status = catalog_->createView(schema_id, name, "select 1", false, false, false,
                                           {}, ID{}, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;

        CatalogManager::ViewInfo info;
        status = catalog_->getView(schema_id, name, info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return info.view_id;
    }

    Status resolveTable(PathType type, const std::vector<std::string>& components,
                        ID& object_id_out, ObjectType& type_out, ErrorContext& ctx)
    {
        ObjectPath path;
        path.type = type;
        path.components = components;
        CatalogManager::ResolveOptions opts;
        return catalog_->resolveObjectPath(path, ObjectType::TABLE, opts, object_id_out, type_out, &ctx);
    }

    Status resolveObject(ObjectType expected_type, PathType type,
                         const std::vector<std::string>& components,
                         ID& object_id_out, ObjectType& type_out, ErrorContext& ctx)
    {
        ObjectPath path;
        path.type = type;
        path.components = components;
        CatalogManager::ResolveOptions opts;
        return catalog_->resolveObjectPath(path, expected_type, opts, object_id_out, type_out, &ctx);
    }
};

TEST_F(SchemaPathResolutionTest, UnqualifiedPrefersCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    ID current_table = createTable(user_schema, "target");
    ID search_table = createTable(public_schema, "target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::UNQUALIFIED, {"target"}, resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, current_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
    (void)search_table;
}

TEST_F(SchemaPathResolutionTest, UnqualifiedFallsBackToSearchPath)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    ID search_table = createTable(public_schema, "fallback");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::UNQUALIFIED, {"fallback"}, resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, search_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
}

TEST_F(SchemaPathResolutionTest, CurrentPathIsStrict)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(public_schema, "strict_only");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::CURRENT, {"strict_only"}, resolved_id, resolved_type, ctx);

    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(SchemaPathResolutionTest, RelativePathResolvesFromCurrent)
{
    ID user_schema = createSchemaPath("users.alice");
    ID nested_schema = createSchemaPath("users.alice.dev.myproj");

    ID nested_table = createTable(nested_schema, "rel_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::CURRENT, {"dev", "myproj", "rel_target"},
                                 resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, nested_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
}

TEST_F(SchemaPathResolutionTest, AbsolutePathResolves)
{
    ID nested_schema = createSchemaPath("users.alice.dev.myproj");
    ID nested_table = createTable(nested_schema, "abs_target");

    conn_->setCurrentSchemaId(nested_schema);
    conn_->set_search_path({"public"});

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::ABSOLUTE,
                                 {"users", "alice", "dev", "myproj", "abs_target"},
                                 resolved_id, resolved_type, ctx);

    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, nested_table);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);
}

TEST_F(SchemaPathResolutionTest, AmbiguousSearchPathReturnsError)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");
    ID app_schema = schemaIdForPath("app");

    createTable(public_schema, "ambiguous");
    createTable(app_schema, "ambiguous");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public", "app"});

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::UNQUALIFIED, {"ambiguous"}, resolved_id, resolved_type, ctx);

    EXPECT_EQ(status, Status::AMBIGUOUS_COLUMN);
}

TEST_F(SchemaPathResolutionTest, CrossTypeAmbiguityReturnsError)
{
    ID schema_id = createSchemaPath("users.alice");

    createTable(schema_id, "dup_name");
    createView(schema_id, "dup_name");

    ErrorContext ctx;
    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveObject(ObjectType::UNKNOWN, PathType::ABSOLUTE,
                                  {"users", "alice", "dup_name"},
                                  resolved_id, resolved_type, ctx);

    EXPECT_EQ(status, Status::AMBIGUOUS_COLUMN);
}

TEST_F(SchemaPathResolutionTest, GetSchemaHonorsDelimitedNames)
{
    ErrorContext ctx;
    CatalogManager::SchemaInfo root;
    ASSERT_EQ(catalog_->getSchema("root", root, &ctx), Status::OK) << ctx.message;

    ID delimited_id = createDelimitedSchema(root.schema_id, "MixedCase");

    CatalogManager::SchemaInfo info;
    Status status = catalog_->getSchema("MixedCase", info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(info.schema_id, delimited_id);

    status = catalog_->getSchema("mixedcase", info, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);

    status = catalog_->getSchema("root.MixedCase", info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(info.schema_id, delimited_id);

    status = catalog_->getSchema("root.mixedcase", info, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST_F(SchemaPathResolutionTest, ResolverHonorsDelimitedSchemaNames)
{
    ErrorContext ctx;
    CatalogManager::SchemaInfo root;
    ASSERT_EQ(catalog_->getSchema("root", root, &ctx), Status::OK) << ctx.message;

    ID delimited_id = createDelimitedSchema(root.schema_id, "MixedCase");
    ID table_id = createTable(delimited_id, "widget");

    ID resolved_id;
    ObjectType resolved_type;
    Status status = resolveTable(PathType::ABSOLUTE, {"MixedCase", "widget"},
                                 resolved_id, resolved_type, ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(resolved_id, table_id);
    EXPECT_EQ(resolved_type, ObjectType::TABLE);

    ErrorContext miss_ctx;
    status = resolveTable(PathType::ABSOLUTE, {"mixedcase", "widget"},
                          resolved_id, resolved_type, miss_ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
}

TEST_F(SchemaPathResolutionTest, ExecutorDropTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(user_schema, "drop_target");
    createTable(public_schema, "drop_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto bytecode = buildDropTableBytecode("drop_target");
    scratchbird::sblr::Executor executor(db_);
    executor.setConnectionContext(conn_.get());
    auto result = executor.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::TableInfo table_info;
    ErrorContext ctx;
    EXPECT_NE(catalog_->getTable(user_schema, "drop_target", table_info, &ctx), Status::OK);
    EXPECT_EQ(catalog_->getTable(public_schema, "drop_target", table_info, &ctx), Status::OK);
}

TEST_F(SchemaPathResolutionTest, ExecutorCreateTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(public_schema, "create_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto bytecode = buildCreateTableBytecode("create_target", {"id"});
    scratchbird::sblr::Executor executor(db_);
    executor.setConnectionContext(conn_.get());
    auto result = executor.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::TableInfo table_info;
    ErrorContext ctx;
    EXPECT_EQ(catalog_->getTable(user_schema, "create_target", table_info, &ctx), Status::OK);
    EXPECT_EQ(catalog_->getTable(public_schema, "create_target", table_info, &ctx), Status::OK);
}

TEST_F(SchemaPathResolutionTest, ExecutorTruncateTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");

    createTable(public_schema, "truncate_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto bytecode = buildTruncateTableBytecode("truncate_target");
    scratchbird::sblr::Executor executor(db_);
    executor.setConnectionContext(conn_.get());
    auto result = executor.execute(bytecode);

    EXPECT_FALSE(result.success());
    EXPECT_NE(result.error().find("Object not found"), std::string::npos);
}

TEST_F(SchemaPathResolutionTest, ExecutorCreateIndexUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID table_id = createTable(user_schema, "idx_table");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});

    auto bytecode = buildCreateIndexBytecode("idx_current_id", "idx_table", {"id"});
    scratchbird::sblr::Executor executor(db_);
    executor.setConnectionContext(conn_.get());
    auto result = executor.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::IndexInfo index_info;
    ErrorContext ctx;
    auto status = catalog_->getIndex(table_id, "idx_current_id", index_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
}

TEST_F(SchemaPathResolutionTest, ExecutorDropIndexUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID table_id = createTable(user_schema, "idx_drop_table");

    ErrorContext ctx;
    ID index_id;
    auto status = catalog_->createIndex(table_id, "idx_drop", {"id"}, index_id,
                                        false, CatalogManager::IndexType::BTREE, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});

    auto bytecode = buildDropIndexBytecode("idx_drop");
    scratchbird::sblr::Executor executor(db_);
    executor.setConnectionContext(conn_.get());
    auto result = executor.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::IndexInfo index_info;
    status = catalog_->getIndex(table_id, "idx_drop", index_info, &ctx);
    EXPECT_NE(status, Status::OK);
}

TEST_F(SchemaPathResolutionTest, ExecutorAlterTableUsesCurrentSchema)
{
    ID user_schema = createSchemaPath("users.alice");
    ID public_schema = schemaIdForPath("public");
    ID user_table_id = createTable(user_schema, "alter_target");
    ID public_table_id = createTable(public_schema, "alter_target");

    conn_->setCurrentSchemaId(user_schema);
    conn_->set_search_path({"public"});
    conn_->setCurrentUser(system_user_id_, true);

    auto bytecode = buildAlterTableRenameColumnBytecode("alter_target", "id", "id_renamed");
    scratchbird::sblr::Executor executor(db_);
    executor.setConnectionContext(conn_.get());
    auto result = executor.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::ColumnInfo column_info;
    ErrorContext ctx;
    EXPECT_EQ(catalog_->getColumn(user_table_id, "id_renamed", column_info, &ctx), Status::OK);
    EXPECT_NE(catalog_->getColumn(user_table_id, "id", column_info, &ctx), Status::OK);
    EXPECT_EQ(catalog_->getColumn(public_table_id, "id", column_info, &ctx), Status::OK);
    EXPECT_NE(catalog_->getColumn(public_table_id, "id_renamed", column_info, &ctx), Status::OK);
}
