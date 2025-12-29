#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/opcodes.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::Database;
using scratchbird::core::Status;
using scratchbird::sblr::Executor;
using scratchbird::sblr::Opcode;
using scratchbird::sblr::ExtendedOpcode;
using scratchbird::sblr::QueryCompilerV2;

namespace {

std::string uniqueDbPath() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    std::filesystem::path base = std::filesystem::path("build") / "database";
    std::filesystem::create_directories(base);
    oss << (base / ("stored_code_dep_" + std::to_string(now) + ".sbdb"));
    return oss.str();
}

void writeInt32(std::vector<uint8_t>& buf, int32_t value) {
    uint32_t v = static_cast<uint32_t>(value);
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void writeInt16(std::vector<uint8_t>& buf, uint16_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void writeString(std::vector<uint8_t>& buf, const std::string& str) {
    writeInt32(buf, static_cast<int32_t>(str.size()));
    buf.insert(buf.end(), str.begin(), str.end());
}

std::vector<uint8_t> buildCreateFunctionBytecode(const std::string& name,
                                                 const std::string& body) {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(Opcode::VERSION));
    bc.push_back(scratchbird::sblr::SBLR_VERSION);
    bc.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    writeInt16(bc, static_cast<uint16_t>(ExtendedOpcode::EXT_CREATE_FUNCTION_STMT));

    uint8_t flags = 0x00; // no OR REPLACE, INVOKER
    bc.push_back(flags);
    writeString(bc, name);

    bc.push_back(0); // param count

    bc.push_back(static_cast<uint8_t>(scratchbird::core::DataType::INT32)); // return type
    writeInt32(bc, 0); // return precision
    writeInt32(bc, 0); // return scale

    writeString(bc, body);
    bc.push_back(static_cast<uint8_t>(Opcode::END));
    return bc;
}

std::vector<uint8_t> buildDropFunctionBytecode(const std::string& name) {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(Opcode::VERSION));
    bc.push_back(scratchbird::sblr::SBLR_VERSION);
    bc.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    writeInt16(bc, static_cast<uint16_t>(ExtendedOpcode::EXT_DROP_FUNCTION_STMT));
    bc.push_back(0x01); // IF EXISTS
    writeString(bc, name);
    bc.push_back(static_cast<uint8_t>(Opcode::END));
    return bc;
}

std::vector<uint8_t> buildCreateProcedureBytecode(const std::string& name,
                                                  const std::string& body) {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(Opcode::VERSION));
    bc.push_back(scratchbird::sblr::SBLR_VERSION);
    bc.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    writeInt16(bc, static_cast<uint16_t>(ExtendedOpcode::EXT_CREATE_PROCEDURE_STMT));

    bc.push_back(0x00); // flags
    writeString(bc, name);
    bc.push_back(0); // param count
    writeString(bc, body);
    bc.push_back(static_cast<uint8_t>(Opcode::END));
    return bc;
}

} // namespace

class StoredCodeDependencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = uniqueDbPath();
        scratchbird::core::ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK);
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK);

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", schema_info, &ctx), Status::OK);
        schema_id_ = schema_info.schema_id;

        QueryCompilerV2 compiler(&db_);
        compiler.setCurrentSchema(schema_id_);
        auto create_table = compiler.compile("CREATE TABLE deps(id INT)");
        ASSERT_TRUE(create_table.success()) << "compile error";

        Executor exec(&db_);
        exec.execute(create_table.bytecode());

        ASSERT_EQ(catalog_->getTable(schema_id_, "deps", table_info_, &ctx), Status::OK);
    }

    void TearDown() override {
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    Database db_{};
    CatalogManager* catalog_ = nullptr;
    CatalogManager::SchemaInfo schema_{};
    CatalogManager::TableInfo table_info_{};
    scratchbird::core::ID schema_id_{};
    std::string db_path_;
};

TEST_F(StoredCodeDependencyTest, CreateFunctionRegistersDependencies) {
    auto bytecode = buildCreateFunctionBytecode("deps_func", "SELECT id FROM deps");
    Executor exec(&db_);
    auto result = exec.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    CatalogManager::FunctionInfo fn_info;
    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(catalog_->getFunction("deps_func", fn_info, &ctx), Status::OK);

    std::vector<CatalogManager::DependencyInfo> deps;
    ASSERT_EQ(catalog_->getDependenciesFor(fn_info.function_id, deps, &ctx), Status::OK);
    ASSERT_FALSE(deps.empty());
    EXPECT_EQ(deps[0].referenced_object_id, table_info_.table_id);
    EXPECT_EQ(deps[0].referenced_type, CatalogManager::ObjectType::TABLE);
}

TEST_F(StoredCodeDependencyTest, DropFunctionClearsDependencies) {
    Executor exec(&db_);
    auto create_bc = buildCreateFunctionBytecode("deps_func2", "SELECT id FROM deps");
    ASSERT_TRUE(exec.execute(create_bc).success());

    CatalogManager::FunctionInfo fn_info;
    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(catalog_->getFunction("deps_func2", fn_info, &ctx), Status::OK);

    auto drop_bc = buildDropFunctionBytecode("deps_func2");
    ASSERT_TRUE(exec.execute(drop_bc).success());

    std::vector<CatalogManager::DependencyInfo> deps;
    catalog_->getDependenciesFor(fn_info.function_id, deps, &ctx);
    EXPECT_TRUE(deps.empty());
}

TEST_F(StoredCodeDependencyTest, CreateProcedureRegistersDependencies) {
    auto bytecode = buildCreateProcedureBytecode("deps_proc", "SELECT id FROM deps");
    Executor exec(&db_);
    ASSERT_TRUE(exec.execute(bytecode).success());

    CatalogManager::ProcedureInfo proc_info;
    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(catalog_->getProcedure("deps_proc", proc_info, &ctx), Status::OK);

    std::vector<CatalogManager::DependencyInfo> deps;
    ASSERT_EQ(catalog_->getDependenciesFor(proc_info.procedure_id, deps, &ctx), Status::OK);
    ASSERT_FALSE(deps.empty());
    EXPECT_EQ(deps[0].referenced_object_id, table_info_.table_id);
}
