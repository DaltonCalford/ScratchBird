/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::ExecutionResult;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::v3::Container;
using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::Value;
using scratchbird::testing::TestDatabaseFile;

namespace {

const Value::Object* asObject(const Value& value) {
    return std::get_if<Value::Object>(&value.data);
}

const Value::Object* requireObjectField(const Value::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    return std::get_if<Value::Object>(&it->second.data);
}

const Value::List* requireListField(const Value::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    return std::get_if<Value::List>(&it->second.data);
}

const Instruction* requireInstrField(const Value::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return nullptr;
    }
    const auto* ptr = std::get_if<Value::InstrPtr>(&it->second.data);
    if (!ptr || !*ptr) {
        return nullptr;
    }
    return ptr->get();
}

bool getU64(const Value::Object& obj, const std::string& key, uint64_t& out) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return false;
    }
    if (const auto* u = std::get_if<uint64_t>(&it->second.data)) {
        out = *u;
        return true;
    }
    if (const auto* i = std::get_if<int64_t>(&it->second.data)) {
        out = static_cast<uint64_t>(*i);
        return true;
    }
    return false;
}

bool getString(const Value::Object& obj, const std::string& key, std::string& out) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return false;
    }
    if (const auto* s = std::get_if<std::string>(&it->second.data)) {
        out = *s;
        return true;
    }
    return false;
}

std::string objectKeys(const Value::Object& obj) {
    std::string out;
    bool first = true;
    for (const auto& [key, _] : obj) {
        if (!first) {
            out += ",";
        }
        out += key;
        first = false;
    }
    return out;
}

std::string optionKeys(const Value::List& options) {
    std::string out;
    bool first = true;
    for (const auto& option_value : options) {
        const auto* option_obj = std::get_if<Value::Object>(&option_value.data);
        if (!option_obj) {
            continue;
        }
        auto it = option_obj->find("key");
        if (it == option_obj->end()) {
            continue;
        }
        const auto* key = std::get_if<std::string>(&it->second.data);
        if (!key) {
            continue;
        }
        if (!first) {
            out += ",";
        }
        out += *key;
        first = false;
    }
    return out;
}

bool findInstruction(const std::vector<uint8_t>& bytecode,
                     Opcode opcode,
                     Instruction& found,
                     std::string& error_out) {
    Container container;
    if (!scratchbird::sblr::v3::decodeContainer(bytecode.data(),
                                                bytecode.size(),
                                                container,
                                                error_out)) {
        return false;
    }

    size_t offset = 0;
    DecodeError decode_err;
    while (offset < container.bytecode_stream.size()) {
        Instruction inst;
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                                 container.bytecode_stream.size(),
                                                                 offset,
                                                                 inst,
                                                                 decode_err)) {
            error_out = decode_err.message;
            return false;
        }
        if (inst.opcode == static_cast<uint16_t>(opcode)) {
            found = std::move(inst);
            return true;
        }
    }
    error_out = "opcode not found";
    return false;
}

}  // namespace

class IndexEmitterPayloadContractsTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("index_emitter_payload_contracts");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());
        ASSERT_NE(db_->catalog_manager(), nullptr);

        scratchbird::core::CatalogManager::SchemaInfo public_schema;
        ASSERT_EQ(db_->catalog_manager()->getSchema("public", public_schema, &ctx), Status::OK)
            << ctx.message;
        compiler_->setCurrentSchema(public_schema.schema_id);
        executor_->setCurrentSchema(public_schema.schema_id);

        ASSERT_TRUE(executeSql("CREATE TABLE users (id INT, name TEXT)")) << "failed to create users table";
        ASSERT_TRUE(executeSql("CREATE INDEX idx_users_id ON users USING BTREE (id)"))
            << "failed to create baseline index";
    }

    bool executeSql(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            if (!compile_result.errors().empty()) {
                ADD_FAILURE() << "Compilation failed for SQL: " << sql
                              << " :: " << compile_result.errors().front();
            } else {
                ADD_FAILURE() << "Compilation failed for SQL: " << sql;
            }
            return false;
        }
        ExecutionResult exec_result = executor_->execute(compile_result.bytecode());
        if (!exec_result.success()) {
            ADD_FAILURE() << "Execution failed for SQL: " << sql << " :: " << exec_result.error();
        }
        return exec_result.success();
    }

    std::vector<uint8_t> compileSql(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        EXPECT_TRUE(compile_result.success()) << sql;
        if (!compile_result.success()) {
            return {};
        }
        return compile_result.bytecode();
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
};

TEST_F(IndexEmitterPayloadContractsTest, CreateIndexCarriesTypedOptionPayload) {
    auto bytecode = compileSql(
        "CREATE INDEX idx_users_vec ON users USING IVF_SQ8_HYBRID (id) "
        "WITH (sample_rate = 0.25, nprobe = 16)");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_CREATE_INDEX, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    std::string index_type;
    ASSERT_TRUE(getString(*payload, "index_type", index_type));
    EXPECT_EQ(index_type, "IVF_SQ8_HYBRID");

    const auto* options = requireListField(*payload, "options");
    ASSERT_NE(options, nullptr) << "payload keys=" << objectKeys(*payload);
    ASSERT_EQ(options->size(), 2u) << "options keys=" << optionKeys(*options);

    const auto* option_entry = std::get_if<Value::Object>(&options->front().data);
    ASSERT_NE(option_entry, nullptr);

    std::string option_key;
    ASSERT_TRUE(getString(*option_entry, "key", option_key));
    EXPECT_EQ(option_key, "SAMPLE_RATE");

    const Instruction* option_value = requireInstrField(*option_entry, "value");
    ASSERT_NE(option_value, nullptr);
    EXPECT_EQ(option_value->opcode, static_cast<uint16_t>(Opcode::SBLR3_LITERAL_DOUBLE));
}

TEST_F(IndexEmitterPayloadContractsTest, CreateIndexCarriesTablespaceFieldWhenSpecified) {
    auto bytecode = compileSql(
        "CREATE INDEX idx_users_ts ON users (id) TABLESPACE ts_hot");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_CREATE_INDEX, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    const auto* tablespace = requireListField(*payload, "tablespace");
    ASSERT_NE(tablespace, nullptr) << "payload keys=" << objectKeys(*payload);
    ASSERT_EQ(tablespace->size(), 1u);
    const auto* ts_name = std::get_if<std::string>(&tablespace->front().data);
    ASSERT_NE(ts_name, nullptr);
    EXPECT_EQ(*ts_name, "ts_hot");
}

TEST_F(IndexEmitterPayloadContractsTest, CreateTablespaceCarriesTypedLifecyclePayload) {
    auto bytecode = compileSql(
        "CREATE TABLESPACE ts_hot LOCATION '/tmp/ts_hot' "
        "AUTOEXTEND OFF AUTOEXTEND_SIZE 128 MAXSIZE 512 PREALLOC 16");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_CREATE_TABLESPACE, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    const auto* path = requireListField(*payload, "path");
    ASSERT_NE(path, nullptr) << "payload keys=" << objectKeys(*payload);
    ASSERT_EQ(path->size(), 1u);

    std::string location;
    ASSERT_TRUE(getString(*payload, "location", location));
    EXPECT_EQ(location, "/tmp/ts_hot");

    auto autoextend_it = payload->find("autoextend_enabled");
    ASSERT_NE(autoextend_it, payload->end());
    const auto* autoextend_enabled = std::get_if<bool>(&autoextend_it->second.data);
    ASSERT_NE(autoextend_enabled, nullptr);
    EXPECT_FALSE(*autoextend_enabled);

    uint64_t autoextend_size_mb = 0;
    ASSERT_TRUE(getU64(*payload, "autoextend_size_mb", autoextend_size_mb));
    EXPECT_EQ(autoextend_size_mb, 128u);

    uint64_t max_size_mb = 0;
    ASSERT_TRUE(getU64(*payload, "max_size_mb", max_size_mb));
    EXPECT_EQ(max_size_mb, 512u);

    uint64_t prealloc_pages = 0;
    ASSERT_TRUE(getU64(*payload, "prealloc_pages", prealloc_pages));
    EXPECT_EQ(prealloc_pages, 16u);
}

TEST_F(IndexEmitterPayloadContractsTest, AlterTablespaceCarriesStructuredAlterationList) {
    auto bytecode = compileSql(
        "ALTER TABLESPACE ts_hot AUTOEXTEND ON AUTOEXTEND_SIZE 128 "
        "MAXSIZE UNLIMITED RENAME TO ts_cold");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_ALTER_TABLESPACE, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    const auto* alterations = requireListField(*payload, "alterations");
    ASSERT_NE(alterations, nullptr) << "payload keys=" << objectKeys(*payload);
    ASSERT_EQ(alterations->size(), 4u);

    const auto* autoextend_obj = std::get_if<Value::Object>(&(*alterations)[0].data);
    ASSERT_NE(autoextend_obj, nullptr);
    uint64_t action = 0;
    ASSERT_TRUE(getU64(*autoextend_obj, "action", action));
    EXPECT_EQ(action, static_cast<uint64_t>(scratchbird::parser::v3::TablespaceAlterAction::SET_AUTOEXTEND));
    auto autoextend_it = autoextend_obj->find("autoextend_enabled");
    ASSERT_NE(autoextend_it, autoextend_obj->end());
    const auto* autoextend_enabled = std::get_if<bool>(&autoextend_it->second.data);
    ASSERT_NE(autoextend_enabled, nullptr);
    EXPECT_TRUE(*autoextend_enabled);

    const auto* size_obj = std::get_if<Value::Object>(&(*alterations)[1].data);
    ASSERT_NE(size_obj, nullptr);
    ASSERT_TRUE(getU64(*size_obj, "action", action));
    EXPECT_EQ(action, static_cast<uint64_t>(scratchbird::parser::v3::TablespaceAlterAction::SET_AUTOEXTEND_SIZE));
    uint64_t size_mb = 0;
    ASSERT_TRUE(getU64(*size_obj, "size_mb", size_mb));
    EXPECT_EQ(size_mb, 128u);

    const auto* max_obj = std::get_if<Value::Object>(&(*alterations)[2].data);
    ASSERT_NE(max_obj, nullptr);
    ASSERT_TRUE(getU64(*max_obj, "action", action));
    EXPECT_EQ(action, static_cast<uint64_t>(scratchbird::parser::v3::TablespaceAlterAction::SET_MAXSIZE));
    ASSERT_TRUE(getU64(*max_obj, "size_mb", size_mb));
    EXPECT_EQ(size_mb, 0u);

    const auto* rename_obj = std::get_if<Value::Object>(&(*alterations)[3].data);
    ASSERT_NE(rename_obj, nullptr);
    ASSERT_TRUE(getU64(*rename_obj, "action", action));
    EXPECT_EQ(action, static_cast<uint64_t>(scratchbird::parser::v3::TablespaceAlterAction::RENAME_TO));
    std::string new_name;
    ASSERT_TRUE(getString(*rename_obj, "new_name", new_name));
    EXPECT_EQ(new_name, "ts_cold");
}

TEST_F(IndexEmitterPayloadContractsTest, AlterTableSetTablespaceCarriesOnlineFlag) {
    auto bytecode = compileSql("ALTER TABLE users SET TABLESPACE ts_hot");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode,
                                Opcode::SBLR3_ALTER_TABLE_SET_TABLESPACE,
                                inst,
                                decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    const auto* table = requireListField(*payload, "table");
    ASSERT_NE(table, nullptr) << "payload keys=" << objectKeys(*payload);
    const auto* tablespace = requireListField(*payload, "tablespace");
    ASSERT_NE(tablespace, nullptr) << "payload keys=" << objectKeys(*payload);

    auto online_it = payload->find("online");
    ASSERT_NE(online_it, payload->end());
    const auto* online = std::get_if<bool>(&online_it->second.data);
    ASSERT_NE(online, nullptr);
    EXPECT_FALSE(*online);
}

TEST_F(IndexEmitterPayloadContractsTest, AlterIndexRelocateCarriesModeAndTargetFilespace) {
    auto bytecode = compileSql(
        "ALTER INDEX users.idx_users_id RELOCATE TO FILESPACE fs_hot ONLINE "
        "WITH (max_bytes_per_txn = 1024, throttle_ms = 10)");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_ALTER_INDEX, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    uint64_t action = 0;
    ASSERT_TRUE(getU64(*payload, "action", action));
    EXPECT_EQ(action, static_cast<uint64_t>(scratchbird::parser::v3::AlterIndexAction::RELOCATE));

    uint64_t mode = 0;
    ASSERT_TRUE(getU64(*payload, "mode", mode));
    EXPECT_EQ(mode, static_cast<uint64_t>(scratchbird::parser::v3::IndexMaintenanceMode::ONLINE));

    const auto* target_filespace = requireListField(*payload, "target_filespace");
    ASSERT_NE(target_filespace, nullptr) << "payload keys=" << objectKeys(*payload);

    const auto* options = requireListField(*payload, "options");
    ASSERT_NE(options, nullptr);
    ASSERT_EQ(options->size(), 2u) << "options keys=" << optionKeys(*options);

    const auto* option_entry = std::get_if<Value::Object>(&options->front().data);
    ASSERT_NE(option_entry, nullptr);

    std::string option_key;
    ASSERT_TRUE(getString(*option_entry, "key", option_key));
    EXPECT_EQ(option_key, "MAX_BYTES_PER_TXN");
}

TEST_F(IndexEmitterPayloadContractsTest, AnalyzeIndexUsesDedicatedAnalyzeSchemaPayload) {
    auto bytecode = compileSql("ANALYZE INDEX users.idx_users_id WITH (sample_rate = 0.25)");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_ANALYZE, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    uint64_t target = 0;
    ASSERT_TRUE(getU64(*payload, "target", target));
    EXPECT_EQ(target, 2u);

    const auto* index_path = requireListField(*payload, "index_path");
    ASSERT_NE(index_path, nullptr) << "payload keys=" << objectKeys(*payload);

    auto sample_it = payload->find("sample_rate");
    ASSERT_NE(sample_it, payload->end());
    const auto* sample_value = std::get_if<double>(&sample_it->second.data);
    ASSERT_NE(sample_value, nullptr);
    EXPECT_NEAR(*sample_value, 0.25, 1e-9);
}

TEST_F(IndexEmitterPayloadContractsTest, ShowIndexHealthEncodesProfileInValueField) {
    auto bytecode = compileSql("SHOW INDEX HEALTH users.idx_users_id");
    ASSERT_FALSE(bytecode.empty());

    Instruction inst;
    std::string decode_error;
    ASSERT_TRUE(findInstruction(bytecode, Opcode::SBLR3_SHOW_INDEX, inst, decode_error))
        << decode_error;
    const auto* payload = asObject(inst.payload);
    ASSERT_NE(payload, nullptr);

    std::string key;
    ASSERT_TRUE(getString(*payload, "key", key));
    EXPECT_EQ(key, "users.idx_users_id");

    const Instruction* profile_instr = requireInstrField(*payload, "value");
    ASSERT_NE(profile_instr, nullptr);
    EXPECT_EQ(profile_instr->opcode, static_cast<uint16_t>(Opcode::SBLR3_LITERAL_STRING));

    const auto* profile_payload = asObject(profile_instr->payload);
    ASSERT_NE(profile_payload, nullptr);
    std::string profile;
    ASSERT_TRUE(getString(*profile_payload, "value", profile));
    EXPECT_EQ(profile, "HEALTH");
}
