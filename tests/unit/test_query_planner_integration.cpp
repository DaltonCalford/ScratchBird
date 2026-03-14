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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/query_profiler.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "test_helpers.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
namespace sblr_v3 = scratchbird::sblr::v3;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    auto isZeroId(const ID& id) -> bool
    {
        return std::all_of(id.bytes.begin(), id.bytes.end(),
                           [](uint8_t byte) { return byte == 0; });
    }

    auto normalizedMisestimateRatio(uint64_t estimated_rows,
                                    uint64_t actual_rows) -> double
    {
        const double estimated =
            static_cast<double>(std::max<uint64_t>(estimated_rows, 1));
        const double actual =
            static_cast<double>(std::max<uint64_t>(actual_rows, 1));
        return std::max(estimated, actual) / std::min(estimated, actual);
    }

    auto normalizedExplainSnapshot(const nlohmann::json& explain_json)
        -> nlohmann::json
    {
        nlohmann::json snapshot;
        snapshot["runtime_plan_contract"] =
            explain_json.value("runtime_plan_contract", "");

        const auto join_graph_it = explain_json.find("join_graph");
        if (join_graph_it != explain_json.end() && join_graph_it->is_object())
        {
            snapshot["join_graph"]["contract"] =
                join_graph_it->value("contract", "");
            snapshot["join_graph"]["relations"] = nlohmann::json::array();
            const auto relations_it = join_graph_it->find("relations");
            if (relations_it != join_graph_it->end() && relations_it->is_array())
            {
                for (const auto& relation : *relations_it)
                {
                    nlohmann::json normalized_relation;
                    normalized_relation["alias"] = relation.value("alias", "");
                    normalized_relation["table_path"] =
                        relation.value("table_path", "");
                    normalized_relation["scan_kind"] =
                        relation.value("scan_kind", "");
                    normalized_relation["scan_family"] =
                        relation.value("scan_family", "");
                    normalized_relation["scan_family_tags"] =
                        relation.value("scan_family_tags",
                                       nlohmann::json::array());
                    normalized_relation["candidate_scan_families"] =
                        relation.value("candidate_scan_families",
                                       nlohmann::json::array());
                    normalized_relation["index_name"] =
                        relation.value("index_name", "");
                    normalized_relation["covering_index"] =
                        relation.value("covering_index", false);
                    normalized_relation["parameterized"] =
                        relation.value("parameterized", false);
                    normalized_relation["ordered_output"] =
                        relation.value("ordered_output", false);
                    normalized_relation["ordered_prefix_length"] =
                        relation.value("ordered_prefix_length", 0u);
                    normalized_relation["required_outer_relation_aliases"] =
                        relation.value("required_outer_relation_aliases",
                                       nlohmann::json::array());
                    normalized_relation["partition_pruned"] =
                        relation.value("partition_pruned", false);
                    normalized_relation["partition_key_columns"] =
                        relation.value("partition_key_columns",
                                       nlohmann::json::array());
                    normalized_relation["partition_targets_pruned_at_plan"] =
                        relation.value("partition_targets_pruned_at_plan",
                                       nlohmann::json::array());
                    normalized_relation["runtime_partition_pruning_eligible"] =
                        relation.value("runtime_partition_pruning_eligible", false);
                    normalized_relation["runtime_partition_pruning_sources"] =
                        relation.value("runtime_partition_pruning_sources",
                                       nlohmann::json::array());
                    snapshot["join_graph"]["relations"].push_back(
                        std::move(normalized_relation));
                }
            }

            snapshot["join_graph"]["join_steps"] = nlohmann::json::array();
            const auto steps_it = join_graph_it->find("join_steps");
            if (steps_it != join_graph_it->end() && steps_it->is_array())
            {
                for (const auto& step : *steps_it)
                {
                    nlohmann::json normalized_step;
                    normalized_step["join_type"] = step.value("join_type", "");
                    normalized_step["method"] = step.value("method", "");
                    normalized_step["join_edge_left_alias"] =
                        step.value("join_edge_left_alias", "");
                    normalized_step["join_edge_right_alias"] =
                        step.value("join_edge_right_alias", "");
                    normalized_step["disconnected_component"] =
                        step.value("disconnected_component", false);
                    normalized_step["legality_class"] =
                        step.value("legality_class", "");
                    normalized_step["legal_method_families"] =
                        step.value("legal_method_families",
                                   nlohmann::json::array());
                    normalized_step["method_enablers"] =
                        step.value("method_enablers", nlohmann::json::array());
                    normalized_step["reorderable"] =
                        step.value("reorderable", true);
                    normalized_step["outer_reorder_barrier"] =
                        step.value("outer_reorder_barrier", false);
                    normalized_step["semi_reorder_barrier"] =
                        step.value("semi_reorder_barrier", false);
                    normalized_step["anti_reorder_barrier"] =
                        step.value("anti_reorder_barrier", false);
                    normalized_step["using_reorder_barrier"] =
                        step.value("using_reorder_barrier", false);
                    normalized_step["natural_reorder_barrier"] =
                        step.value("natural_reorder_barrier", false);
                    normalized_step["lateral_reorder_barrier"] =
                        step.value("lateral_reorder_barrier", false);
                    snapshot["join_graph"]["join_steps"].push_back(
                        std::move(normalized_step));
                }
            }
        }

        const auto trace_it = explain_json.find("optimizer_trace");
        if (trace_it != explain_json.end() && trace_it->is_object())
        {
            snapshot["optimizer_trace"]["diagnostics_contract"] =
                trace_it->value("diagnostics_contract", "");
            snapshot["optimizer_trace"]["search_summary"] =
                trace_it->value("search_summary", nlohmann::json::object());
            const auto considered_it = trace_it->find("considered_paths");
            if (considered_it != trace_it->end() && considered_it->is_array())
            {
                snapshot["optimizer_trace"]["considered_path_count"] =
                    considered_it->size();
            }
            const auto rejected_it = trace_it->find("rejected_paths");
            if (rejected_it != trace_it->end() && rejected_it->is_array())
            {
                snapshot["optimizer_trace"]["rejected_path_count"] =
                    rejected_it->size();
            }
            const auto provenance_it = trace_it->find("statistics_provenance");
            if (provenance_it != trace_it->end() && provenance_it->is_array())
            {
                snapshot["optimizer_trace"]["statistics_provenance_count"] =
                    provenance_it->size();
            }
            const auto signals_it = trace_it->find("advisor_signals");
            if (signals_it != trace_it->end() && signals_it->is_array())
            {
                snapshot["optimizer_trace"]["advisor_signal_count"] =
                    signals_it->size();
            }
            const auto recommendations_it =
                trace_it->find("advisor_recommendations");
            if (recommendations_it != trace_it->end() &&
                recommendations_it->is_array())
            {
                snapshot["optimizer_trace"]["advisor_recommendation_count"] =
                    recommendations_it->size();
            }
        }

        const auto plan_root_it = explain_json.find("plan_root");
        if (plan_root_it != explain_json.end() && plan_root_it->is_object())
        {
            snapshot["plan_root"]["node_type"] =
                plan_root_it->value("node_type", "");
            snapshot["plan_root"]["join_type"] =
                plan_root_it->value("join_type", "");
            snapshot["plan_root"]["estimated_rows"] =
                plan_root_it->value("estimated_rows", 0u);
            snapshot["plan_root"]["actuals_available"] =
                plan_root_it->value("actuals_available", false);
            snapshot["plan_root"]["actual_rows"] =
                plan_root_it->value("actual_rows", 0u);
            snapshot["plan_root"]["spill_expected"] =
                plan_root_it->value("spill_expected", false);
        }

        const auto analyze_it = explain_json.find("analyze");
        if (analyze_it != explain_json.end() && analyze_it->is_object())
        {
            snapshot["analyze"]["rows"] = analyze_it->value("rows", 0u);
        }

        return snapshot;
    }

    auto normalizedRuntimeJoinStep(
        const scratchbird::optimizer::RuntimePlanJoinStep& step)
        -> nlohmann::json
    {
        return nlohmann::json{
            {"join_type", step.join_type},
            {"method", step.method},
            {"join_edge_left_alias", step.join_edge_left_alias},
            {"join_edge_right_alias", step.join_edge_right_alias},
            {"disconnected_component", step.disconnected_component},
            {"legality_class", step.legality_class},
            {"legal_method_families", step.legal_method_families},
            {"method_enablers", step.method_enablers},
            {"reorderable", step.reorderable},
            {"outer_reorder_barrier", step.outer_reorder_barrier},
            {"semi_reorder_barrier", step.semi_reorder_barrier},
            {"anti_reorder_barrier", step.anti_reorder_barrier},
            {"using_reorder_barrier", step.using_reorder_barrier},
            {"natural_reorder_barrier", step.natural_reorder_barrier},
            {"lateral_reorder_barrier", step.lateral_reorder_barrier},
            {"parameterized_dependency", step.parameterized_dependency},
            {"estimated_rows", step.estimated_rows},
            {"selectivity", step.selectivity},
        };
    }
}

/**
 * Integration test for Query Planner (Phase 1, Task 1.3)
 *
 * Verifies that:
 * 1. Query planner is properly integrated with QueryCompilerV3
 * 2. Optimizer components are initialized
 * 3. Bytecode structure is preserved
 */
class QueryPlannerIntegrationTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    bool createDatabase()
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        db_file_ = std::make_unique<TestDatabaseFile>("test_query_planner");

        ErrorContext ctx;
        Status status = Database::create(db_file_->path(), 16384, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        db_ = std::make_unique<Database>();
        status = db_->open(db_file_->path(), &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            return false;
        }

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog->getSchema("public", public_schema_info, &ctx);
        if (status != Status::OK)
        {
            return false;
        }

        status = db_->connect(connection_ctx_, &ctx);
        if (status != Status::OK)
        {
            return false;
        }
        connection_ctx_->setCurrentSchemaId(public_schema_info.schema_id);
        const auto system_user_id = catalog->getSystemUserId(&ctx);
        if (isZeroId(system_user_id))
        {
            return false;
        }
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        const std::vector<std::string> ddl = {
            "CREATE TABLE users (id INTEGER, name VARCHAR(100), email VARCHAR(100), age INTEGER)",
            "CREATE TABLE products (id INTEGER, name VARCHAR(100), price DOUBLE)",
            "CREATE TABLE orders (id INTEGER, user_id INTEGER, amount DOUBLE)",
            "CREATE TABLE test (id INTEGER)",
            "GRANT SELECT ON users TO PUBLIC",
            "GRANT SELECT ON products TO PUBLIC",
            "GRANT SELECT ON orders TO PUBLIC",
            "GRANT SELECT ON test TO PUBLIC"
        };

        for (const auto& sql : ddl)
        {
            auto compile_result = compiler_->compile(sql);
            if (!compile_result.success())
            {
                return false;
            }
            auto exec_result = executor_->execute(compile_result.bytecode());
            if (!exec_result.success())
            {
                return false;
            }
        }

        return true;
    }

    std::vector<uint8_t> compileSQL(const std::string &sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            last_compile_errors_.clear();
            for (const auto& err : compile_result.errors())
            {
                if (!last_compile_errors_.empty())
                {
                    last_compile_errors_ += "\n";
                }
                last_compile_errors_ += err;
            }
            return {};
        }
        last_compile_errors_.clear();
        return compile_result.bytecode();
    }

    QueryCompilerV3::CompileResult compileSQLWithParameters(
        const std::string& sql,
        const optimizer::ParameterBindings& bindings)
    {
        auto compile_result = compiler_->compileWithParameters(sql, bindings);
        if (!compile_result.success())
        {
            last_compile_errors_.clear();
            for (const auto& err : compile_result.errors())
            {
                if (!last_compile_errors_.empty())
                {
                    last_compile_errors_ += "\n";
                }
                last_compile_errors_ += err;
            }
        }
        else
        {
            last_compile_errors_.clear();
        }
        return compile_result;
    }

    ExecutionResult executeSQL(const std::string &sql)
    {
        auto bytecode = compileSQL(sql);
        if (bytecode.empty())
        {
            return ExecutionResult("Compilation failed: " + last_compile_errors_);
        }
        return executor_->execute(bytecode);
    }

    ExecutionResult executeBytecode(const std::vector<uint8_t>& bytecode)
    {
        if (bytecode.empty())
        {
            return ExecutionResult("Bytecode payload is empty");
        }
        return executor_->execute(bytecode);
    }

    ExecutionResult executeBytecodeWithParameters(
        const std::vector<uint8_t>& bytecode,
        const std::vector<std::string>& values,
        const std::vector<bool>& nulls = {})
    {
        if (bytecode.empty())
        {
            return ExecutionResult("Bytecode payload is empty");
        }
        executor_->setParameters(values, nulls);
        auto result = executor_->execute(bytecode);
        executor_->clearParameters();
        return result;
    }

    bool containsOpcode(const std::vector<uint8_t> &bytecode, sblr_v3::Opcode opcode)
    {
        const auto target = static_cast<uint16_t>(opcode);
        std::function<bool(const sblr_v3::Value&)> valueContainsOpcode;
        std::function<bool(const sblr_v3::Instruction&)> instructionContainsOpcode;

        valueContainsOpcode = [&](const sblr_v3::Value& value) -> bool {
            if (const auto* stmt = std::get_if<sblr_v3::Value::InstrPtr>(&value.data)) {
                return *stmt && instructionContainsOpcode(**stmt);
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data)) {
                for (const auto& element : *list) {
                    if (valueContainsOpcode(element)) {
                        return true;
                    }
                }
                return false;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data)) {
                for (const auto& [_, element] : *obj) {
                    if (valueContainsOpcode(element)) {
                        return true;
                    }
                }
                return false;
            }
            return false;
        };

        instructionContainsOpcode = [&](const sblr_v3::Instruction& inst) -> bool {
            if (inst.opcode == target) {
                return true;
            }
            return valueContainsOpcode(inst.payload);
        };

        sblr_v3::Container container;
        std::string err;
        if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        {
            return false;
        }
        size_t offset = 0;
        sblr_v3::DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            sblr_v3::Instruction inst;
            if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                      container.bytecode_stream.size(),
                                                      offset,
                                                      inst,
                                                      decode_err) &&
                !sblr_v3::decodeInstruction(container.bytecode_stream.data(),
                                            container.bytecode_stream.size(),
                                            offset,
                                            inst,
                                            decode_err))
            {
                break;
            }
            if (instructionContainsOpcode(inst))
            {
                return true;
            }
        }
        return false;
    }

    bool decodeFirstSelect(const std::vector<uint8_t>& bytecode, sblr_v3::Instruction& out)
    {
        sblr_v3::Container container;
        std::string err;
        if (!sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        {
            return false;
        }
        size_t offset = 0;
        sblr_v3::DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            sblr_v3::Instruction inst;
            if (!sblr_v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                      container.bytecode_stream.size(),
                                                      offset,
                                                      inst,
                                                      decode_err))
            {
                return false;
            }
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_SELECT)
            {
                out = std::move(inst);
                return true;
            }
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_EXPLAIN_PLAN)
            {
                const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
                if (!obj)
                {
                    return false;
                }
                auto it_query = obj->find("query");
                if (it_query == obj->end())
                {
                    return false;
                }
                const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&it_query->second.data);
                if (ptr && *ptr &&
                    static_cast<sblr_v3::Opcode>((*ptr)->opcode) == sblr_v3::Opcode::SBLR3_SELECT)
                {
                    out = **ptr;
                    return true;
                }
            }
        }
        return false;
    }

    bool decodeRuntimePlan(const std::vector<uint8_t>& bytecode,
                           scratchbird::optimizer::RuntimePlan& plan_out)
    {
        sblr_v3::Instruction select_inst;
        if (!decodeFirstSelect(bytecode, select_inst))
        {
            return false;
        }
        const auto* obj = std::get_if<sblr_v3::Value::Object>(&select_inst.payload.data);
        if (!obj)
        {
            return false;
        }
        auto it_plan = obj->find("plan");
        if (it_plan == obj->end())
        {
            return false;
        }
        const auto* bytes = std::get_if<sblr_v3::Value::Bytes>(&it_plan->second.data);
        if (!bytes)
        {
            return false;
        }
        std::string err;
        return scratchbird::optimizer::decodeRuntimePlan(*bytes, plan_out, err);
    }

    const scratchbird::optimizer::RuntimePlanControlEntry* findOptimizerControl(
        const scratchbird::optimizer::RuntimePlan& plan,
        const std::string& name)
    {
        for (const auto& entry : plan.optimizer_controls)
        {
            if (entry.name == name)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    nlohmann::json loadTableMetadataJson(const std::string& table_name)
    {
        nlohmann::json metadata = nlohmann::json::object();
        if (db_ == nullptr)
        {
            ADD_FAILURE() << "Database is not initialized";
            return metadata;
        }
        auto* catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            ADD_FAILURE() << "Catalog manager is not available";
            return metadata;
        }

        ErrorContext ctx;
        CatalogManager::SchemaInfo public_schema;
        if (catalog->getSchema("public", public_schema, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to resolve public schema: " << ctx.message;
            return metadata;
        }

        CatalogManager::TableInfo table_info;
        if (catalog->getTable(public_schema.schema_id, table_name, table_info, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to load table metadata row: " << ctx.message;
            return metadata;
        }
        if (isZeroId(table_info.storage_params_oid))
        {
            ADD_FAILURE() << "Table storage_params_oid was empty for " << table_name;
            return metadata;
        }

        std::string params;
        if (catalog->loadStringFromToast(table_info.storage_params_oid, 0, params, &ctx) != Status::OK)
        {
            ADD_FAILURE() << "Failed to load table metadata TOAST payload: " << ctx.message;
            return metadata;
        }
        if (params.empty())
        {
            ADD_FAILURE() << "Table metadata payload was empty for " << table_name;
            return metadata;
        }

        try
        {
            metadata = nlohmann::json::parse(params);
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "Failed to parse metadata JSON: " << ex.what();
        }
        return metadata;
    }

    bool findFirstOpcode(const sblr_v3::Instruction& root,
                         sblr_v3::Opcode target,
                         sblr_v3::Instruction& out)
    {
        std::function<bool(const sblr_v3::Instruction&)> visit_inst;
        std::function<bool(const sblr_v3::Value&)> visit_value;

        visit_value = [&](const sblr_v3::Value& value) -> bool {
            if (const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
            {
                return ptr != nullptr && *ptr != nullptr && visit_inst(**ptr);
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data))
            {
                for (const auto& entry : *list)
                {
                    if (visit_value(entry))
                    {
                        return true;
                    }
                }
                return false;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data))
            {
                for (const auto& [_, entry] : *obj)
                {
                    if (visit_value(entry))
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        visit_inst = [&](const sblr_v3::Instruction& inst) -> bool {
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == target)
            {
                out = inst;
                return true;
            }
            return visit_value(inst.payload);
        };

        return visit_inst(root);
    }

    std::vector<std::pair<std::string, std::string>> collectColumnRefs(
        const sblr_v3::Instruction& root)
    {
        std::vector<std::pair<std::string, std::string>> refs;
        std::function<void(const sblr_v3::Instruction&)> visit_inst;
        std::function<void(const sblr_v3::Value&)> visit_value;

        visit_value = [&](const sblr_v3::Value& value) {
            if (const auto* ptr = std::get_if<sblr_v3::Value::InstrPtr>(&value.data))
            {
                if (ptr != nullptr && *ptr != nullptr)
                {
                    visit_inst(**ptr);
                }
                return;
            }
            if (const auto* list = std::get_if<sblr_v3::Value::List>(&value.data))
            {
                for (const auto& entry : *list)
                {
                    visit_value(entry);
                }
                return;
            }
            if (const auto* obj = std::get_if<sblr_v3::Value::Object>(&value.data))
            {
                for (const auto& [_, entry] : *obj)
                {
                    visit_value(entry);
                }
            }
        };

        visit_inst = [&](const sblr_v3::Instruction& inst) {
            if (static_cast<sblr_v3::Opcode>(inst.opcode) == sblr_v3::Opcode::SBLR3_COLUMN_REF)
            {
                const auto* obj = std::get_if<sblr_v3::Value::Object>(&inst.payload.data);
                if (obj != nullptr)
                {
                    std::string column_name;
                    std::string qualifier;
                    auto col_it = obj->find("column");
                    if (col_it != obj->end())
                    {
                        if (const auto* col = std::get_if<std::string>(&col_it->second.data))
                        {
                            column_name = *col;
                        }
                    }
                    auto path_it = obj->find("path");
                    if (path_it != obj->end())
                    {
                        if (const auto* path =
                                std::get_if<sblr_v3::Value::List>(&path_it->second.data))
                        {
                            for (auto it = path->rbegin(); it != path->rend(); ++it)
                            {
                                if (const auto* part = std::get_if<std::string>(&it->data))
                                {
                                    if (!part->empty())
                                    {
                                        qualifier = *part;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    refs.emplace_back(std::move(qualifier), std::move(column_name));
                }
            }
            visit_value(inst.payload);
        };

        visit_inst(root);
        return refs;
    }

    std::vector<std::string> resultStrings(const ExecutionResult& result)
    {
        std::vector<std::string> lines;
        if (!result.success() || !result.hasResultSet() || result.resultSet() == nullptr)
        {
            return lines;
        }

        auto* rs = result.resultSet();
        for (size_t row = 0; row < rs->rowCount(); ++row)
        {
            lines.push_back(rs->getValue(row, 0).toString());
        }
        return lines;
    }

    size_t resultRowCount(const ExecutionResult& result)
    {
        if (!result.success() || !result.hasResultSet() || result.resultSet() == nullptr)
        {
            return 0;
        }
        return result.resultSet()->rowCount();
    }

    double meanDurationMs(size_t iterations,
                          const std::function<bool()>& fn)
    {
        if (iterations == 0)
        {
            return 0.0;
        }

        using clock = std::chrono::steady_clock;
        double total_ms = 0.0;
        for (size_t i = 0; i < iterations; ++i)
        {
            const auto start = clock::now();
            if (!fn())
            {
                return -1.0;
            }
            const auto end = clock::now();
            total_ms += std::chrono::duration<double, std::milli>(end - start).count();
        }
        return total_ms / static_cast<double>(iterations);
    }

    void enableParallelPlanning(const std::string& setup_cost = "0",
                                const std::string& tuple_cost = "0")
    {
        ASSERT_NE(connection_ctx_, nullptr);
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_SCAN", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_HASH", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_AGGREGATE", "ON");
        connection_ctx_->setSessionVariable("ENABLE_PARALLEL_JOIN", "ON");
        connection_ctx_->setSessionVariable("PARALLEL_LEADER_PARTICIPATION", "ON");
        connection_ctx_->setSessionVariable("MAX_PARALLEL_WORKERS", "4");
        connection_ctx_->setSessionVariable("MAX_PARALLEL_WORKERS_PER_GATHER", "4");
        connection_ctx_->setSessionVariable("MIN_PARALLEL_ROWS_PER_WORKER", "1");
        connection_ctx_->setSessionVariable("MIN_PARALLEL_TABLE_SCAN_SIZE", "1");
        connection_ctx_->setSessionVariable("PARALLEL_SETUP_COST", setup_cost);
        connection_ctx_->setSessionVariable("PARALLEL_TUPLE_COST", tuple_cost);
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
    std::string last_compile_errors_;
};

// ===== Basic Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, QueryCompilerV3ProducesBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_TABLE_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, DatabaseHasQueryPlannerComponents)
{
    ASSERT_TRUE(createDatabase());

    EXPECT_NE(db_->statistics_manager(), nullptr);
    scratchbird::optimizer::QueryPlanner planner(
        db_.get(),
        scratchbird::optimizer::CostModel(),
        db_->statistics_manager());
    (void)planner;
}

TEST_F(QueryPlannerIntegrationTest, CompilerRequiresCatalog)
{
    QueryCompilerV3 compiler(nullptr);
    auto result = compiler.compile("SELECT 1");
    EXPECT_FALSE(result.success());
}

// ===== Optimizer Integration Tests =====

TEST_F(QueryPlannerIntegrationTest, SelectGeneratesWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto select_bytecode = compileSQL("SELECT * FROM users");
    EXPECT_FALSE(select_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(select_bytecode, sblr_v3::Opcode::SBLR3_SELECT));
}

TEST_F(QueryPlannerIntegrationTest, OptimizedSelectEmbedsRuntimePlanPayload)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_FALSE(plan.plan_hash.empty());
    EXPECT_FALSE(plan.explain_text.empty());
    EXPECT_EQ(plan.cache_mode, "GENERIC");
    EXPECT_FALSE(plan.plan_profile_signature.empty());
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().table_path, "users");
    EXPECT_FALSE(plan.relations.front().scan_kind.empty());
    EXPECT_FALSE(plan.root.node_type.empty());
}

TEST_F(QueryPlannerIntegrationTest, DisabledOptimizationsDoNotEmbedRuntimePlanPayload)
{
    ASSERT_TRUE(createDatabase());

    compiler_->setOptimizationsEnabled(false);
    auto bytecode = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    EXPECT_FALSE(decodeRuntimePlan(bytecode, plan));
}

TEST_F(QueryPlannerIntegrationTest, RepeatedSelectHitsPlanCache)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();
    const auto baseline = QueryCompilerV3::planCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);
    EXPECT_EQ(after_first.entries, baseline.entries + 1u);

    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
    EXPECT_EQ(after_second.entries, baseline.entries + 1u);
    EXPECT_EQ(first, second);
}

TEST_F(QueryPlannerIntegrationTest, SchemaMutationInvalidatesCachedPlansLocally)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();
    const auto baseline = QueryCompilerV3::planCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto mutation = compileSQL("CREATE INDEX idx_users_age ON users (age)");
    ASSERT_FALSE(mutation.empty()) << last_compile_errors_;

    auto after_mutation = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_mutation.hits, 1u);
    EXPECT_EQ(after_mutation.misses, 1u);
    EXPECT_EQ(after_mutation.inserts, 1u);
    EXPECT_EQ(after_mutation.invalidations, 1u);
    EXPECT_EQ(after_mutation.entries, baseline.entries);
}

TEST_F(QueryPlannerIntegrationTest, DmlDoesNotInvalidateCachedPlans)
{
    ASSERT_TRUE(createDatabase());

    QueryCompilerV3::resetPlanCacheStats();
    const auto baseline = QueryCompilerV3::planCacheStats();

    auto first = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(first.empty()) << last_compile_errors_;
    auto second = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(second.empty()) << last_compile_errors_;
    auto dml = compileSQL("INSERT INTO users (id, name) VALUES (1, 'alice')");
    ASSERT_FALSE(dml.empty()) << last_compile_errors_;
    auto third = compileSQL("SELECT id FROM users WHERE id = 42");
    ASSERT_FALSE(third.empty()) << last_compile_errors_;

    auto after_dml = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_dml.hits, 2u);
    EXPECT_EQ(after_dml.misses, 1u);
    EXPECT_EQ(after_dml.inserts, 1u);
    EXPECT_EQ(after_dml.invalidations, 0u);
    EXPECT_EQ(after_dml.entries, baseline.entries + 1u);
}

TEST_F(QueryPlannerIntegrationTest, ParameterizedSelectUsesCustomPlanProfileAndHitsBucketedCache)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    QueryCompilerV3::resetPlanCacheStats();

    auto first = compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(first.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM);
    EXPECT_TRUE(first.planProfile().parameter_sensitive);
    EXPECT_NE(first.planProfile().signature.find("CUSTOM:"), std::string::npos);
    scratchbird::optimizer::RuntimePlan custom_plan;
    ASSERT_TRUE(decodeRuntimePlan(first.bytecode(), custom_plan));
    EXPECT_EQ(custom_plan.cache_mode, "CUSTOM");
    EXPECT_TRUE(custom_plan.parameter_sensitive);
    EXPECT_EQ(custom_plan.plan_profile_signature, first.planProfile().signature);
    EXPECT_FALSE(custom_plan.considered_paths.empty());
    EXPECT_FALSE(custom_plan.statistics_provenance.empty());

    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    auto second = compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(second.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().signature, second.planProfile().signature);

    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
}

TEST_F(QueryPlannerIntegrationTest, ParameterizedRangeBucketsChangeWithBindingSelectivity)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    optimizer::ParameterBindings selective;
    selective.positional.push_back({false, "5"});
    optimizer::ParameterBindings broad;
    broad.positional.push_back({false, "250"});

    auto selective_compile =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", selective);
    ASSERT_TRUE(selective_compile.success()) << last_compile_errors_;
    auto broad_compile =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", broad);
    ASSERT_TRUE(broad_compile.success()) << last_compile_errors_;

    EXPECT_NE(selective_compile.planProfile().selectivity_bucket_signature,
              broad_compile.planProfile().selectivity_bucket_signature);
    EXPECT_NE(selective_compile.planProfile().signature,
              broad_compile.planProfile().signature);

    scratchbird::optimizer::RuntimePlan selective_plan;
    scratchbird::optimizer::RuntimePlan broad_plan;
    ASSERT_TRUE(decodeRuntimePlan(selective_compile.bytecode(), selective_plan));
    ASSERT_TRUE(decodeRuntimePlan(broad_compile.bytecode(), broad_plan));
    ASSERT_FALSE(selective_plan.relations.empty());
    ASSERT_FALSE(broad_plan.relations.empty());
    EXPECT_EQ(selective_plan.cache_mode, "CUSTOM");
    EXPECT_TRUE(selective_plan.parameter_sensitive);
    EXPECT_EQ(selective_plan.selectivity_bucket_signature,
              selective_compile.planProfile().selectivity_bucket_signature);
    EXPECT_FALSE(selective_plan.considered_paths.empty());
    EXPECT_FALSE(selective_plan.statistics_provenance.empty());
    EXPECT_LT(selective_plan.relations.front().estimated_rows,
              broad_plan.relations.front().estimated_rows);
}

TEST_F(QueryPlannerIntegrationTest, PlanProfileDirectiveForcesGenericParameterizedPlan)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=GENERIC");

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    auto compile_result =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;
    EXPECT_EQ(compile_result.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC);
    EXPECT_FALSE(compile_result.planProfile().parameter_sensitive);

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(compile_result.bytecode(), plan));
    EXPECT_EQ(plan.cache_mode, "GENERIC");
    EXPECT_FALSE(plan.parameter_sensitive);
    const auto* plan_profile = findOptimizerControl(plan, "PLAN_PROFILE");
    ASSERT_NE(plan_profile, nullptr);
    EXPECT_EQ(plan_profile->value, "GENERIC");
    EXPECT_EQ(plan_profile->source, "DIRECTIVE");
}

TEST_F(QueryPlannerIntegrationTest, AutoPlanProfileUsesChooserAndPublishesReuseMetadata)
{
    ASSERT_TRUE(createDatabase());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', " +
                               std::to_string(20 + (i % 10)) + ")")
                        .success());
    }

    CatalogManager::TableInfo table_info;
    ErrorContext stats_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "users",
                                               table_info,
                                               &stats_ctx),
              Status::OK)
        << stats_ctx.message;
    ASSERT_EQ(db_->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &stats_ctx),
              Status::OK)
        << stats_ctx.message;

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=AUTO");
    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "5"});

    QueryCompilerV3::resetPlanCacheStats();

    auto first =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(first.success()) << last_compile_errors_;
    EXPECT_EQ(first.planProfile().decision_source, "CHOOSER");
    EXPECT_FALSE(first.planProfile().statistics_snapshot_signature.empty());
    EXPECT_FALSE(first.planProfile().cost_profile_id.empty());
    EXPECT_FALSE(first.planProfile().policy_snapshot_id.empty());

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(first.bytecode(), plan));
    EXPECT_EQ(plan.cache_mode,
              first.planProfile().mode ==
                      scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::CUSTOM
                  ? "CUSTOM"
                  : "GENERIC");
    const auto *plan_profile = findOptimizerControl(plan, "PLAN_PROFILE");
    ASSERT_NE(plan_profile, nullptr);
    EXPECT_EQ(plan_profile->value, "AUTO");
    EXPECT_EQ(plan_profile->source, "DIRECTIVE");
    const auto *reuse_decision = findOptimizerControl(plan, "PLAN_REUSE_DECISION");
    ASSERT_NE(reuse_decision, nullptr);
    EXPECT_EQ(reuse_decision->value, plan.cache_mode);
    EXPECT_EQ(reuse_decision->source, "CHOOSER");
    const auto *stats_snapshot = findOptimizerControl(plan, "PLAN_STATS_SNAPSHOT");
    ASSERT_NE(stats_snapshot, nullptr);
    EXPECT_EQ(stats_snapshot->value,
              first.planProfile().statistics_snapshot_signature);
    const auto *cost_profile = findOptimizerControl(plan, "PLAN_COST_PROFILE");
    ASSERT_NE(cost_profile, nullptr);
    EXPECT_EQ(cost_profile->value, first.planProfile().cost_profile_id);
    const auto *policy_snapshot = findOptimizerControl(plan, "PLAN_POLICY_SNAPSHOT");
    ASSERT_NE(policy_snapshot, nullptr);
    EXPECT_EQ(policy_snapshot->value, first.planProfile().policy_snapshot_id);

    auto after_first = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_first.hits, 0u);
    EXPECT_EQ(after_first.misses, 1u);
    EXPECT_EQ(after_first.inserts, 1u);

    auto second =
        compileSQLWithParameters("SELECT id FROM users WHERE id < $1", bindings);
    ASSERT_TRUE(second.success()) << last_compile_errors_;
    EXPECT_EQ(second.planProfile().decision_source, "CHOOSER");
    EXPECT_EQ(second.planProfile().signature, first.planProfile().signature);
    EXPECT_EQ(second.planProfile().statistics_snapshot_signature,
              first.planProfile().statistics_snapshot_signature);
    EXPECT_EQ(second.planProfile().cost_profile_id,
              first.planProfile().cost_profile_id);

    auto after_second = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_second.hits, 1u);
    EXPECT_EQ(after_second.misses, 1u);
    EXPECT_EQ(after_second.inserts, 1u);
}

TEST_F(QueryPlannerIntegrationTest, CardinalityFeedbackBypassesStaleCacheAndRebuildsPlan)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    QueryCompilerV3::resetPlanCacheStats();

    const std::string sql = "SELECT id FROM users WHERE id > 0";
    auto stale_bytecode = compileSQL(sql);
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));
    ASSERT_GT(stale_plan.root.estimated_rows, 0u);

    for (int i = 5; i <= 200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'u" + std::to_string(i) +
                               "', 'u" + std::to_string(i) + "@x', 30)")
                        .success());
    }

    auto stale_result = executeBytecode(stale_bytecode);
    ASSERT_TRUE(stale_result.success()) << stale_result.error();
    ASSERT_TRUE(stale_result.hasResultSet());
    ASSERT_EQ(stale_result.resultSet()->rowCount(), 200u);

    const auto feedback_key =
        std::to_string(sblr_v3::stableHash64(
            optimizer::QueryProfiler::getInstance().fingerprintQuery(sql)));
    auto feedback =
        optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(feedback_key);
    ASSERT_TRUE(feedback.has_value());
    EXPECT_TRUE(feedback->available);
    EXPECT_TRUE(feedback->replan_required);
    EXPECT_EQ(feedback->last_estimated_rows, stale_plan.root.estimated_rows);
    EXPECT_EQ(feedback->last_actual_rows, 200u);

    auto refreshed_bytecode = compileSQL(sql);
    ASSERT_FALSE(refreshed_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan refreshed_plan;
    ASSERT_TRUE(decodeRuntimePlan(refreshed_bytecode, refreshed_plan));
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.available);
    EXPECT_FALSE(refreshed_plan.adaptive_feedback.replan_required);
    EXPECT_TRUE(refreshed_plan.adaptive_feedback.stats_refresh_applied);
    EXPECT_EQ(refreshed_plan.adaptive_feedback.observation_count, 1u);
    EXPECT_EQ(refreshed_plan.adaptive_feedback.last_actual_rows, 200u);
    EXPECT_NE(refreshed_plan.root.estimated_rows, stale_plan.root.estimated_rows);

    auto adaptive_it = std::find_if(
        refreshed_plan.statistics_provenance.begin(),
        refreshed_plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance& entry) {
            return entry.source == "ADAPTIVE_CARDINALITY_FEEDBACK";
        });
    EXPECT_NE(adaptive_it, refreshed_plan.statistics_provenance.end());

    const auto cache_stats = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(cache_stats.hits, 0u);
    EXPECT_EQ(cache_stats.misses, 1u);
    EXPECT_EQ(cache_stats.inserts, 2u);
    EXPECT_GE(cache_stats.invalidations, 1u);
}

TEST_F(QueryPlannerIntegrationTest, EqualityJoinChoosesHashJoinPlan)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "INNER");
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_hash_keys);
    EXPECT_EQ(plan.join_steps.front().left_hash_key.column_name, "id");
    EXPECT_EQ(plan.join_steps.front().right_hash_key.column_name, "id");
}

TEST_F(QueryPlannerIntegrationTest, FullOuterJoinFailsClosedToNestedLoop)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users FULL JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "FULL");
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_FALSE(plan.join_steps.front().has_hash_keys);
}

TEST_F(QueryPlannerIntegrationTest,
       RuntimePlanCarriesVersionedJoinGraphAndSearchSummaryContracts)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.version, scratchbird::optimizer::kRuntimePlanPayloadVersion);
    EXPECT_EQ(plan.contract_id, scratchbird::optimizer::kRuntimePlanContractId);
    EXPECT_EQ(plan.join_graph_contract_id,
              scratchbird::optimizer::kJoinGraphContractId);
    EXPECT_EQ(plan.diagnostics_contract_id,
              scratchbird::optimizer::kOptimizerDiagnosticsContractId);
    EXPECT_FALSE(plan.search_summary.requested_strategy.empty());
    EXPECT_FALSE(plan.search_summary.selected_strategy.empty());
    EXPECT_GE(plan.search_summary.considered_state_count, 1u);
    EXPECT_EQ(plan.search_summary.rejected_candidate_count,
              plan.rejected_paths.size());

    ASSERT_EQ(plan.join_steps.size(), 1u);
    const auto& step = plan.join_steps.front();
    EXPECT_EQ(step.join_edge_left_relation_index, 0u);
    EXPECT_EQ(step.join_edge_right_relation_index, 1u);
    EXPECT_EQ(step.join_edge_left_alias, "users");
    EXPECT_EQ(step.join_edge_right_alias, "products");
    EXPECT_FALSE(step.join_edge_left_id_text.empty());
    EXPECT_FALSE(step.join_edge_right_id_text.empty());
    EXPECT_FALSE(step.legal_method_families.empty());
    EXPECT_TRUE(step.outer_reorder_barrier);
    EXPECT_FALSE(step.using_reorder_barrier);
    EXPECT_FALSE(step.natural_reorder_barrier);
    EXPECT_FALSE(step.lateral_reorder_barrier);
    EXPECT_FALSE(step.parameterized_dependency);
    ASSERT_EQ(step.equijoin_keys.size(), 1u);
    EXPECT_EQ(step.equijoin_keys.front().left_column_name, "id");
    EXPECT_EQ(step.equijoin_keys.front().right_column_name, "id");
    EXPECT_TRUE(step.residual_predicates.empty());
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonPublishesStatsHealthAndStalePenalty)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE TABLE planner_stats_users (id INTEGER, age INTEGER)").success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON planner_stats_users TO PUBLIC").success());

    for (int i = 1; i <= 1000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO planner_stats_users (id, age) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(20 + (i % 10)) + ")")
                        .success());
    }
    ErrorContext commit_ctx;
    ASSERT_EQ(connection_ctx_->commit(&commit_ctx), Status::OK)
        << commit_ctx.message;
    ASSERT_TRUE(executeSQL("ANALYZE planner_stats_users").success());
    ErrorContext analyze_commit_ctx;
    ASSERT_EQ(connection_ctx_->commit(&analyze_commit_ctx), Status::OK)
        << analyze_commit_ctx.message;

    auto fresh_bytecode =
        compileSQL("SELECT id FROM planner_stats_users WHERE age = 25");
    ASSERT_FALSE(fresh_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan fresh_plan;
    ASSERT_TRUE(decodeRuntimePlan(fresh_bytecode, fresh_plan));
    auto initial_freshness_trace = std::find_if(
        fresh_plan.considered_paths.begin(),
        fresh_plan.considered_paths.end(),
        [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
            return entry.phase == "STATS_FRESHNESS";
        });
    EXPECT_EQ(initial_freshness_trace, fresh_plan.considered_paths.end());

    for (int i = 1001; i <= 1100; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO planner_stats_users (id, age) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(20 + (i % 10)) + ")")
                        .success());
    }
    ErrorContext stale_commit_ctx;
    ASSERT_EQ(connection_ctx_->commit(&stale_commit_ctx), Status::OK)
        << stale_commit_ctx.message;

    auto stale_bytecode =
        compileSQL("SELECT id FROM planner_stats_users WHERE age = 26");
    ASSERT_FALSE(stale_bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan stale_plan;
    ASSERT_TRUE(decodeRuntimePlan(stale_bytecode, stale_plan));

    auto relation_stats_it = std::find_if(
        stale_plan.statistics_provenance.begin(),
        stale_plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            const bool source_is_column_stats =
                entry.source.find("MCV") != std::string::npos ||
                entry.source.find("NDISTINCT") != std::string::npos ||
                entry.source.find("COLUMN_STATS") != std::string::npos;
            return entry.subject == "relation:planner_stats_users" &&
                   source_is_column_stats &&
                   entry.detail.find("age") != std::string::npos;
        });
    ASSERT_NE(relation_stats_it, stale_plan.statistics_provenance.end());
    EXPECT_GT(relation_stats_it->stats_snapshot_id, 0u);
    EXPECT_GT(relation_stats_it->last_analyzed_time, 0u);
    EXPECT_GT(relation_stats_it->sample_ratio, 0.0);
    EXPECT_FALSE(relation_stats_it->staleness_class.empty());
    EXPECT_FALSE(relation_stats_it->confidence_class.empty());
    EXPECT_GT(relation_stats_it->auto_analyze_threshold, 0u);

    auto explain_result = executeSQL(
        "EXPLAIN (FORMAT JSON) SELECT id FROM planner_stats_users WHERE age = 26");
    ASSERT_TRUE(explain_result.success()) << explain_result.error();
    ASSERT_TRUE(explain_result.hasResultSet());

    const auto explain_lines = resultStrings(explain_result);
    ASSERT_EQ(explain_lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(explain_lines.front());
    ASSERT_TRUE(parsed.contains("optimizer_trace"));
    const auto &stats_array = parsed["optimizer_trace"]["statistics_provenance"];
    auto explain_stats_it = std::find_if(
        stats_array.begin(),
        stats_array.end(),
        [](const nlohmann::json &entry) {
            const std::string source = entry.value("source", std::string());
            const bool source_is_column_stats =
                source.find("MCV") != std::string::npos ||
                source.find("NDISTINCT") != std::string::npos ||
                source.find("COLUMN_STATS") != std::string::npos;
            return entry.value("subject", std::string()) ==
                       "relation:planner_stats_users" &&
                   source_is_column_stats &&
                   entry.value("detail", std::string()).find("age") != std::string::npos;
        });
    ASSERT_NE(explain_stats_it, stats_array.end());
    EXPECT_EQ(explain_stats_it->value("stats_snapshot_id", 0ULL),
              relation_stats_it->stats_snapshot_id);
    EXPECT_EQ(explain_stats_it->value("last_analyzed_time", 0ULL),
              relation_stats_it->last_analyzed_time);
    EXPECT_DOUBLE_EQ(explain_stats_it->value("sample_ratio", 0.0),
                     relation_stats_it->sample_ratio);
    EXPECT_EQ(explain_stats_it->value("modified_rows_since_analyze", 0ULL),
              relation_stats_it->modified_rows_since_analyze);
    EXPECT_EQ(explain_stats_it->value("staleness_class", std::string()),
              relation_stats_it->staleness_class);
    EXPECT_EQ(explain_stats_it->value("confidence_class", std::string()),
              relation_stats_it->confidence_class);
    EXPECT_EQ(explain_stats_it->value("auto_analyze_threshold", 0ULL),
              relation_stats_it->auto_analyze_threshold);
}

TEST_F(QueryPlannerIntegrationTest, LeftJoinCarriesFormalLegalityMetadata)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "LEFT");
    EXPECT_EQ(plan.join_steps.front().legality_class, "LEFT_OUTER_BARRIER");
    EXPECT_EQ(plan.join_steps.front().legal_method_families.size(), 3u);
    EXPECT_EQ(plan.join_steps.front().legal_method_families.front(),
              "NESTED_LOOP");
    EXPECT_EQ(plan.join_steps.front().legal_method_families[1], "HASH_JOIN");
    EXPECT_EQ(plan.join_steps.front().legal_method_families[2], "MERGE_JOIN");
    EXPECT_FALSE(plan.join_steps.front().reorderable);
    EXPECT_TRUE(plan.join_steps.front().preserves_left_rows);
    EXPECT_FALSE(plan.join_steps.front().preserves_right_rows);
    EXPECT_FALSE(plan.join_steps.front().null_introduces_left);
    EXPECT_TRUE(plan.join_steps.front().null_introduces_right);
    EXPECT_TRUE(plan.join_steps.front().requires_original_order);
    EXPECT_TRUE(plan.join_steps.front().outer_reorder_barrier);
    EXPECT_FALSE(plan.join_steps.front().using_reorder_barrier);
    EXPECT_FALSE(plan.join_steps.front().natural_reorder_barrier);
}

TEST_F(QueryPlannerIntegrationTest,
       CanonicalJoinBackendOwnsMultiRelationSearchStrategy)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.search_summary.requested_strategy, "AUTO");
    EXPECT_EQ(plan.search_summary.selected_strategy, "EXHAUSTIVE_DP");
    EXPECT_TRUE(plan.search_summary.fallback_reason.empty());
    EXPECT_GE(plan.search_summary.considered_state_count, 2u);
}

TEST_F(QueryPlannerIntegrationTest, AutoMediumJoinGraphSelectsBoundedDp)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.EXHAUSTIVE_JOIN_LIMIT", "2");
    connection_ctx_->setSessionVariable("OPTIMIZER.BOUNDED_DP_JOIN_LIMIT", "3");
    connection_ctx_->setSessionVariable("OPTIMIZER.MAX_STATES_CONSIDERED", "32");
    connection_ctx_->setSessionVariable("OPTIMIZER.SEARCH_DEPTH", "32");

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.search_summary.requested_strategy, "AUTO");
    EXPECT_EQ(plan.search_summary.selected_strategy, "BOUNDED_DP");
    EXPECT_EQ(plan.search_summary.exhaustive_join_limit, 2u);
    EXPECT_EQ(plan.search_summary.bounded_dp_join_limit, 3u);
    EXPECT_EQ(plan.search_summary.max_states_considered, 32u);
    EXPECT_EQ(plan.search_summary.max_pair_evaluations, 32u);
    EXPECT_TRUE(plan.search_summary.fallback_reason.empty());
}

TEST_F(QueryPlannerIntegrationTest, SearchBudgetFallbackTelemetryPublishesThreshold)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_SEARCH", "BOUNDED_DP");
    connection_ctx_->setSessionVariable("OPTIMIZER.EXHAUSTIVE_JOIN_LIMIT", "2");
    connection_ctx_->setSessionVariable("OPTIMIZER.BOUNDED_DP_JOIN_LIMIT", "4");
    connection_ctx_->setSessionVariable("OPTIMIZER.MAX_STATES_CONSIDERED", "1");
    connection_ctx_->setSessionVariable("OPTIMIZER.FALLBACK_PRUNE_LEVEL", "2");

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.search_summary.requested_strategy, "BOUNDED_DP");
    EXPECT_EQ(plan.search_summary.selected_strategy, "HEURISTIC_GREEDY");
    EXPECT_EQ(plan.search_summary.fallback_reason, "MAX_STATES_CONSIDERED");
    EXPECT_EQ(plan.search_summary.fallback_threshold_name,
              "MAX_STATES_CONSIDERED");
    EXPECT_EQ(plan.search_summary.fallback_threshold_value, 1u);
    EXPECT_EQ(plan.search_summary.max_states_considered, 1u);
}

TEST_F(QueryPlannerIntegrationTest, UsingJoinMarksReorderBarrierAndFailsClosedToNestedLoop)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products USING (id)");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().join_type, "INNER");
    EXPECT_EQ(plan.join_steps.front().legality_class, "USING_BARRIER");
    EXPECT_FALSE(plan.join_steps.front().reorderable);
    EXPECT_TRUE(plan.join_steps.front().requires_original_order);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");
    EXPECT_FALSE(plan.join_steps.front().has_hash_keys);
    EXPECT_TRUE(plan.join_steps.front().using_reorder_barrier);
    EXPECT_FALSE(plan.join_steps.front().outer_reorder_barrier);
}

TEST_F(QueryPlannerIntegrationTest, MixedOuterAndInnerJoinKeepsSyntacticRelationOrder)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT users.id "
        "FROM users "
        "LEFT JOIN products ON users.id = products.id "
        "JOIN test ON test.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 3u);
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.join_steps[0].source_join_index, 0u);
    EXPECT_EQ(plan.join_steps[1].source_join_index, 1u);
    EXPECT_EQ(plan.join_steps[0].legality_class, "LEFT_OUTER_BARRIER");
    EXPECT_FALSE(plan.join_steps[0].reorderable);
    EXPECT_EQ(plan.join_steps[1].join_type, "INNER");
    EXPECT_EQ(plan.search_summary.selected_strategy, "INPUT_ORDER_ONLY");
    EXPECT_EQ(plan.search_summary.fallback_reason, "JOIN_REORDER_BARRIER");
}

TEST_F(QueryPlannerIntegrationTest, DisconnectedJoinGraphKeepsExplicitCrossJoinStep)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (id) VALUES (10)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO test (id) VALUES (20)").success());

    const std::string sql =
        "SELECT users.id "
        "FROM users "
        "JOIN products ON users.id = products.id "
        "CROSS JOIN test";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 2u);
    EXPECT_EQ(plan.search_summary.selected_strategy, "EXHAUSTIVE_DP");

    auto cross_it = std::find_if(plan.join_steps.begin(),
                                 plan.join_steps.end(),
                                 [](const scratchbird::optimizer::RuntimePlanJoinStep &step) {
                                     return step.join_type == "CROSS";
                                 });
    ASSERT_NE(cross_it, plan.join_steps.end());
    EXPECT_EQ(cross_it->method, "NESTED_LOOP");
    EXPECT_TRUE(cross_it->disconnected_component);
    EXPECT_EQ(cross_it->join_type, "CROSS");
    EXPECT_EQ(cross_it->legality_class, "CROSS_REORDERABLE");
    EXPECT_TRUE(cross_it->residual_predicates.empty());
}

TEST_F(QueryPlannerIntegrationTest, MgaCanonicalTelemetryFeedsPlannerCostingAndRuntimePlanProvenance)
{
    ASSERT_TRUE(createDatabase());

    CatalogManager::TableInfo orders_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "orders",
                                               orders_info,
                                               nullptr),
              Status::OK);

    StorageEngine::FragmentationAdvisory advisory{};
    advisory.page_id = 23;
    advisory.reclaimable_bytes = 8192;
    advisory.deleted_slots = 12;
    advisory.chain_depth_hint = 8;
    advisory.same_page_back_versions = 1;
    advisory.same_page_update_ratio = 0.125;
    advisory.dead_space_ratio = 0.45;
    advisory.rewrite_recommended = true;
    db_->storage_engine()->publishFragmentationAdvisory(
        orders_info.table_id, advisory.page_id, advisory);

    auto bytecode = compileSQL("SELECT * FROM orders WHERE user_id = 7");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto *mga_active = findOptimizerControl(plan, "MGA_COSTING_ACTIVE");
    ASSERT_NE(mga_active, nullptr);
    EXPECT_EQ(mga_active->value, "true");

    const auto *mga_contract = findOptimizerControl(plan, "MGA_COSTING_CONTRACT");
    ASSERT_NE(mga_contract, nullptr);
    EXPECT_EQ(mga_contract->value, "sb_mga_observability/v1");

    auto provenance_it = std::find_if(
        plan.statistics_provenance.begin(),
        plan.statistics_provenance.end(),
        [](const scratchbird::optimizer::RuntimePlanStatisticsProvenance &entry) {
            return entry.subject == "relation:orders" &&
                   entry.source == "MGA_CANONICAL_METRICS";
        });
    ASSERT_NE(provenance_it, plan.statistics_provenance.end());
    EXPECT_NE(provenance_it->detail.find("cleanup_debt_bytes=8192"),
              std::string::npos);
    EXPECT_NE(provenance_it->detail.find("same_page_update_ratio=0.125"),
              std::string::npos);

    auto trace_it = std::find_if(
        plan.considered_paths.begin(),
        plan.considered_paths.end(),
        [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
            return entry.phase == "MGA_COSTING" &&
                   entry.subject == "relation:orders" &&
                   entry.verdict == "ADJUSTED";
        });
    ASSERT_NE(trace_it, plan.considered_paths.end());
    EXPECT_NE(trace_it->reason.find("penalty="), std::string::npos);
    EXPECT_GT(trace_it->total_cost, 0.0);
}

TEST_F(QueryPlannerIntegrationTest, HashJoinPlanExecutesAndReturnsExpectedRows)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 32)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (3, 'carol', 'c@example.com', 34)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (2, 'p2', 20.5)").success());
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (4, 'p4', 40.5)").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");

    auto result =
        executeSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 2u);
    std::sort(rows.begin(), rows.end());
    EXPECT_EQ(rows[0], "1");
    EXPECT_EQ(rows[1], "2");
}

TEST_F(QueryPlannerIntegrationTest, HashJoinRuntimePlanTracksMemoryBudgetAndSpillMetadata)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "HASH_JOIN");
    EXPECT_TRUE(plan.join_steps.front().spill_expected);
    EXPECT_GT(plan.join_steps.front().spill_passes, 0u);
    EXPECT_GT(plan.join_steps.front().estimated_memory_bytes, 0u);
    EXPECT_GT(plan.join_steps.front().memory_budget_bytes, 0u);
    EXPECT_EQ(plan.join_steps.front().spill_policy, "ALLOW");
    EXPECT_EQ(plan.root.node_type, "HashJoin");
    EXPECT_TRUE(plan.root.spill_expected);
    EXPECT_GT(plan.root.memory_budget_bytes, 0u);
    const auto* work_mem = findOptimizerControl(plan, "WORK_MEM");
    ASSERT_NE(work_mem, nullptr);
    EXPECT_EQ(work_mem->source, "SESSION");
    const auto* spill_policy = findOptimizerControl(plan, "SPILL_POLICY");
    ASSERT_NE(spill_policy, nullptr);
    EXPECT_EQ(spill_policy->value, "ALLOW");
}

TEST_F(QueryPlannerIntegrationTest, JoinMethodControlForcesNestedLoopAndChangesCacheIdentity)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id";
    QueryCompilerV3::resetPlanCacheStats();

    auto default_bytecode = compileSQL(sql);
    ASSERT_FALSE(default_bytecode.empty()) << last_compile_errors_;
    auto after_default = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_default.misses, 1u);

    scratchbird::optimizer::RuntimePlan default_plan;
    ASSERT_TRUE(decodeRuntimePlan(default_bytecode, default_plan));

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "NESTED_LOOP");

    auto forced_bytecode = compileSQL(sql);
    ASSERT_FALSE(forced_bytecode.empty()) << last_compile_errors_;
    auto after_forced = QueryCompilerV3::planCacheStats();
    EXPECT_EQ(after_forced.misses, 2u);

    scratchbird::optimizer::RuntimePlan forced_plan;
    ASSERT_TRUE(decodeRuntimePlan(forced_bytecode, forced_plan));
    ASSERT_EQ(forced_plan.join_steps.size(), 1u);
    EXPECT_EQ(forced_plan.join_steps.front().method, "NESTED_LOOP");
    const auto* join_method = findOptimizerControl(forced_plan, "JOIN_METHOD");
    ASSERT_NE(join_method, nullptr);
    EXPECT_EQ(join_method->value, "NESTED_LOOP");
    EXPECT_EQ(join_method->source, "SESSION");
    EXPECT_NE(default_plan.plan_hash, forced_plan.plan_hash);
}

TEST_F(QueryPlannerIntegrationTest, UnsupportedOptimizerDirectiveFailsClosed)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES", "FOO=BAR");
    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    EXPECT_TRUE(bytecode.empty());
    EXPECT_NE(last_compile_errors_.find("Unsupported optimizer directive"),
              std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, SpillPolicyDisallowRejectsSpilledHashJoin)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    connection_ctx_->setSessionVariable("OPTIMIZER.SPILL_POLICY", "DISALLOW");

    for (int i = 1; i <= 1600; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + (i % 100)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "NESTED_LOOP");

    bool found_hash_rejection = false;
    for (const auto& entry : plan.rejected_paths)
    {
        if (entry.candidate == "HASH_JOIN" &&
            entry.reason.find("spill policy disallows") != std::string::npos)
        {
            found_hash_rejection = true;
            break;
        }
    }
    EXPECT_TRUE(found_hash_rejection);
}

TEST_F(QueryPlannerIntegrationTest, MergeJoinPlanExecutesAndPreservesRuntimeMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + i) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + i) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id "
        "FROM users JOIN products ON users.id = products.id "
        "WHERE users.id = 2 AND products.id = 2";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_merge_keys);
    EXPECT_EQ(plan.join_steps.front().left_merge_key.column_name, "id");
    EXPECT_EQ(plan.join_steps.front().right_merge_key.column_name, "id");
    EXPECT_EQ(plan.root.node_type, "MergeJoin");
    EXPECT_NE(plan.root.detail_text.find("presorted"), std::string::npos);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front(), "2");
}

TEST_F(QueryPlannerIntegrationTest, ForcedMergeJoinUsesExplicitSortToMergeCandidate)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 4; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + i) + ")")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (" +
                               std::to_string(i) + ", 'p" + std::to_string(i) +
                               "', " + std::to_string(10 + i) + ".0)")
                        .success());
    }

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "MERGE_JOIN");

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id = products.id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps.front().method, "MERGE_JOIN");
    EXPECT_TRUE(plan.join_steps.front().has_merge_keys);
    EXPECT_FALSE(plan.join_steps.front().merge_outer_presorted);
    EXPECT_FALSE(plan.join_steps.front().merge_inner_presorted);
    ASSERT_EQ(plan.join_steps.front().method_enablers.size(), 2u);
    EXPECT_EQ(plan.join_steps.front().method_enablers[0], "SORT_OUTER");
    EXPECT_EQ(plan.join_steps.front().method_enablers[1], "SORT_INNER");
}

TEST_F(QueryPlannerIntegrationTest, ForcedHashJoinFailsClosedOnNonEquiJoin)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "HASH_JOIN");

    auto bytecode =
        compileSQL("SELECT users.id FROM users JOIN products ON users.id > products.id");
    EXPECT_TRUE(bytecode.empty());
    EXPECT_NE(last_compile_errors_.find("JOIN_METHOD_NON_EQUI"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonFormatsRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT id FROM users WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("\"plan_hash\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"options\":["), std::string::npos);
    EXPECT_NE(lines.front().find("\"VERBOSE\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan_root\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"node_type\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"cache_mode\":\"GENERIC\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"plan_profile_signature\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"runtime_plan_contract\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"join_graph\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"optimizer_trace\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"diagnostics_contract\":\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"search_summary\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"considered_paths\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"rejected_paths\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"statistics_provenance\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"adaptive_feedback\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"analyze\":{\"rows\":"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ExplainAnalyzeJsonPublishesPerNodeActuals)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(1, 'alice', 'a@example.com', 26)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(3, 'carol', 'c@example.com', 26)")
                    .success());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT id FROM users WHERE age = 26 "
                             "ORDER BY name LIMIT 1");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(lines.front());

    ASSERT_TRUE(parsed.contains("plan_root"));
    const auto& plan_root = parsed.at("plan_root");
    EXPECT_TRUE(plan_root.at("actuals_available").get<bool>());
    EXPECT_TRUE(plan_root.contains("actual_rows"));
    EXPECT_TRUE(plan_root.contains("rows_examined"));
    EXPECT_TRUE(plan_root.contains("rows_filtered"));
    EXPECT_TRUE(plan_root.contains("loop_count"));

    std::function<const nlohmann::json*(const nlohmann::json&)> find_scan_node;
    find_scan_node = [&](const nlohmann::json& node) -> const nlohmann::json* {
        const std::string node_type = node.value("node_type", "");
        if (node_type == "SeqScan" || node_type == "IndexScan" ||
            node_type == "IndexOnlyScan" || node_type == "BitmapIndexScan")
        {
            return &node;
        }
        const auto children_it = node.find("children");
        if (children_it == node.end() || !children_it->is_array())
        {
            return nullptr;
        }
        for (const auto& child : *children_it)
        {
            if (const auto* match = find_scan_node(child))
            {
                return match;
            }
        }
        return nullptr;
    };

    const auto* scan_node = find_scan_node(plan_root);
    ASSERT_NE(scan_node, nullptr);
    EXPECT_TRUE(scan_node->at("actuals_available").get<bool>());
    EXPECT_TRUE(scan_node->contains("actual_rows"));
    EXPECT_TRUE(scan_node->contains("rows_examined"));
    EXPECT_TRUE(scan_node->contains("rows_filtered"));
}

TEST_F(QueryPlannerIntegrationTest,
       ExplainAnalyzeJsonPublishesAdvisorRecommendationsWithProvenance)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(1, 'alice', 'a@example.com', 26)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES "
                    "(2, 'bob', 'b@example.com', 31)")
                    .success());

    auto result = executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) "
                             "SELECT u.id FROM users u "
                             "WHERE u.age = 26 ORDER BY u.name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(lines.front());

    ASSERT_TRUE(parsed.contains("optimizer_trace"));
    const auto& optimizer_trace = parsed.at("optimizer_trace");
    ASSERT_TRUE(optimizer_trace.contains("advisor_signals"));
    ASSERT_TRUE(optimizer_trace.contains("advisor_recommendations"));
    ASSERT_TRUE(optimizer_trace.at("advisor_signals").is_array());
    ASSERT_TRUE(optimizer_trace.at("advisor_recommendations").is_array());
    EXPECT_FALSE(optimizer_trace.at("advisor_signals").empty());
    EXPECT_FALSE(optimizer_trace.at("advisor_recommendations").empty());

    const auto& recommendation =
        optimizer_trace.at("advisor_recommendations").front();
    EXPECT_EQ(recommendation.at("rank").get<uint32_t>(), 1u);
    EXPECT_FALSE(recommendation.at("recommendation_type").get<std::string>().empty());
    EXPECT_FALSE(recommendation.at("provenance_source").get<std::string>().empty());
    EXPECT_FALSE(recommendation.at("query_fingerprint").get<std::string>().empty());
    ASSERT_TRUE(recommendation.at("signal_names").is_array());

    ASSERT_TRUE(optimizer_trace.contains("statistics_provenance"));
    const auto& statistics_provenance =
        optimizer_trace.at("statistics_provenance");
    const auto advisor_stats_it = std::find_if(
        statistics_provenance.begin(),
        statistics_provenance.end(),
        [](const auto& entry) {
            return entry.value("source", std::string()) == "ADVISOR_FEEDBACK";
        });
    EXPECT_NE(advisor_stats_it, statistics_provenance.end());
}

TEST_F(QueryPlannerIntegrationTest, CompiledRuntimePlanCarriesAdvisorRecommendations)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT u.id FROM users u WHERE u.age = 26 ORDER BY u.name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_FALSE(plan.advisor_signals.empty());
    EXPECT_FALSE(plan.advisor_recommendations.empty());
    ASSERT_FALSE(plan.advisor_recommendations.empty());
    const auto& recommendation = plan.advisor_recommendations.front();
    EXPECT_EQ(recommendation.rank, 1u);
    EXPECT_FALSE(recommendation.recommendation_type.empty());
    EXPECT_EQ(recommendation.provenance_source, "INDEX_ADVISOR");
    EXPECT_FALSE(recommendation.query_fingerprint.empty());
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonPublishesFormulaProfileAndExpandedCostTerms)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT id FROM users WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    const auto parsed = nlohmann::json::parse(lines.front());

    ASSERT_TRUE(parsed.contains("plan_root"));
    const auto &plan_root = parsed.at("plan_root");
    EXPECT_TRUE(plan_root.contains("formula_profile_id"));
    EXPECT_TRUE(plan_root.contains("formula_profile_version"));
    EXPECT_TRUE(plan_root.contains("calibration_profile_id"));
    EXPECT_TRUE(plan_root.contains("resource_governance_outcome"));
    EXPECT_TRUE(plan_root.contains("input_estimates"));
    EXPECT_TRUE(plan_root.contains("expanded_cost_terms"));
    ASSERT_TRUE(plan_root.at("input_estimates").is_array());
    ASSERT_TRUE(plan_root.at("expanded_cost_terms").is_array());
    EXPECT_FALSE(plan_root.at("formula_profile_id").get<std::string>().empty());
    EXPECT_GT(plan_root.at("formula_profile_version").get<uint32_t>(), 0u);
    EXPECT_FALSE(plan_root.at("calibration_profile_id").get<std::string>().empty());
    EXPECT_FALSE(plan_root.at("input_estimates").empty());
    EXPECT_FALSE(plan_root.at("expanded_cost_terms").empty());
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonPublishesJoinGraphContractFields)
{
    ASSERT_TRUE(createDatabase());

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT users.id FROM users LEFT JOIN products ON users.id = products.id");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);

    const auto parsed = nlohmann::json::parse(lines.front());
    EXPECT_EQ(parsed.at("runtime_plan_contract").get<std::string>(),
              scratchbird::optimizer::kRuntimePlanContractId);

    const auto& join_graph = parsed.at("join_graph");
    EXPECT_EQ(join_graph.at("contract").get<std::string>(),
              scratchbird::optimizer::kJoinGraphContractId);
    ASSERT_TRUE(join_graph.at("relations").is_array());
    ASSERT_TRUE(join_graph.at("join_steps").is_array());
    ASSERT_EQ(join_graph.at("relations").size(), 2u);
    ASSERT_EQ(join_graph.at("join_steps").size(), 1u);
    EXPECT_TRUE(join_graph.at("relations")[0].contains("scan_family"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("scan_family_tags"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("candidate_scan_families"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("ordered_output"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("ordered_prefix_length"));
    EXPECT_TRUE(join_graph.at("relations")[0].contains("required_outer_relation_aliases"));
    EXPECT_EQ(join_graph.at("join_steps")[0].at("join_edge_left_alias").get<std::string>(),
              "users");
    EXPECT_EQ(join_graph.at("join_steps")[0].at("join_edge_right_alias").get<std::string>(),
              "products");
    EXPECT_TRUE(join_graph.at("join_steps")[0].contains("legal_method_families"));
    EXPECT_TRUE(join_graph.at("join_steps")[0].contains("method_enablers"));
    EXPECT_TRUE(join_graph.at("join_steps")[0].at("outer_reorder_barrier").get<bool>());

    const auto& optimizer_trace = parsed.at("optimizer_trace");
    EXPECT_EQ(optimizer_trace.at("diagnostics_contract").get<std::string>(),
              scratchbird::optimizer::kOptimizerDiagnosticsContractId);
    const auto& search_summary = optimizer_trace.at("search_summary");
    EXPECT_TRUE(search_summary.contains("requested_strategy"));
    EXPECT_TRUE(search_summary.contains("selected_strategy"));
    EXPECT_TRUE(search_summary.contains("considered_state_count"));
    EXPECT_TRUE(search_summary.contains("pruned_state_count"));
    EXPECT_TRUE(search_summary.contains("pair_evaluation_count"));
    EXPECT_TRUE(search_summary.contains("rejected_candidate_count"));
    EXPECT_TRUE(search_summary.contains("max_pair_evaluations"));
    EXPECT_TRUE(search_summary.contains("max_states_considered"));
    EXPECT_TRUE(search_summary.contains("fallback_threshold_name"));
    EXPECT_TRUE(search_summary.contains("fallback_threshold_value"));
}

TEST_F(QueryPlannerIntegrationTest, ExplainJsonIncludesOperatorMemoryAndSpillMetadata)
{
    ASSERT_TRUE(createDatabase());

    connection_ctx_->setSessionVariable("WORK_MEM", "64KB");
    for (int i = 1; i <= 2000; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 50)) + ")")
                        .success());
    }

    auto result = executeSQL("EXPLAIN (FORMAT JSON) "
                             "SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    const auto lines = resultStrings(result);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("\"estimated_memory_bytes\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"memory_budget_bytes\":"), std::string::npos);
    EXPECT_NE(lines.front().find("\"spill_expected\":true"), std::string::npos);
    EXPECT_NE(lines.front().find("\"spill_policy\":\"ALLOW\""), std::string::npos);
    EXPECT_NE(lines.front().find("\"optimizer_controls\":"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, ReorderedJoinPayloadKeepsOriginalRelationIndexes)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_products_id ON products (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'u" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("INSERT INTO products (id, name, price) VALUES (42, 'widget', 12.5)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE products").success());

    const std::string sql =
        "SELECT users.id FROM users JOIN products ON users.id = products.id "
        "WHERE products.id = 42";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.relations.front().source_relation_index, 1u);

    sblr_v3::Instruction select_inst;
    ASSERT_TRUE(decodeFirstSelect(bytecode, select_inst));
    const auto* select_payload = std::get_if<sblr_v3::Value::Object>(&select_inst.payload.data);
    ASSERT_NE(select_payload, nullptr);
    auto from_it = select_payload->find("from");
    ASSERT_NE(from_it, select_payload->end());
    const auto* from_obj = std::get_if<sblr_v3::Value::Object>(&from_it->second.data);
    ASSERT_NE(from_obj, nullptr);
    auto relation_index_it = from_obj->find("source_relation_index");
    ASSERT_NE(relation_index_it, from_obj->end());
    const auto* relation_index = std::get_if<uint64_t>(&relation_index_it->second.data);
    ASSERT_NE(relation_index, nullptr);
    EXPECT_EQ(*relation_index, 1u);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows.front(), "42");
}

TEST_F(QueryPlannerIntegrationTest, OptimizerParityBaselineCorpusCapturesStableSummary)
{
    ASSERT_TRUE(createDatabase());

    const std::vector<std::string> setup_sql = {
        "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)",
        "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)",
        "INSERT INTO users (id, name, email, age) VALUES (3, 'carol', 'c@example.com', 32)",
        "INSERT INTO products (id, name, price) VALUES (1, 'p1', 10.5)",
        "INSERT INTO products (id, name, price) VALUES (2, 'p2', 11.5)",
        "INSERT INTO orders (id, user_id, amount) VALUES (1, 1, 12.0)",
        "INSERT INTO orders (id, user_id, amount) VALUES (2, 1, 24.0)",
        "INSERT INTO orders (id, user_id, amount) VALUES (3, 2, 36.0)",
        "INSERT INTO test (id) VALUES (10)",
        "INSERT INTO test (id) VALUES (20)",
        "CREATE INDEX idx_users_id ON users (id)",
        "CREATE INDEX idx_products_id ON products (id)",
        "CREATE INDEX idx_orders_user_id ON orders (user_id)"
    };

    for (const auto& sql : setup_sql)
    {
        ASSERT_TRUE(executeSQL(sql).success()) << sql;
    }

    struct BaselineQuery
    {
        std::string id;
        std::string sql;
    };

    const std::vector<BaselineQuery> corpus = {
        {"single_relation_filter",
         "SELECT id FROM users WHERE id = 1"},
        {"left_join_barrier",
         "SELECT users.id FROM users LEFT JOIN products ON users.id = products.id"},
        {"disconnected_cross_bridge",
         "SELECT users.id FROM users JOIN products ON users.id = products.id, test"},
        {"ordered_inner_join",
         "SELECT users.id FROM users JOIN products ON users.id = products.id ORDER BY users.id"},
        {"grouped_aggregate",
         "SELECT user_id FROM orders GROUP BY user_id ORDER BY user_id"},
    };

    nlohmann::json baseline;
    baseline["schema"] = "scratchbird.optimizer_parity.baseline.v1";
    baseline["query_count"] = corpus.size();
    baseline["queries"] = nlohmann::json::array();

    for (const auto& entry : corpus)
    {
        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();

        auto bytecode = compileSQL(entry.sql);
        ASSERT_FALSE(bytecode.empty()) << entry.id << ": " << last_compile_errors_;

        scratchbird::optimizer::RuntimePlan plan;
        ASSERT_TRUE(decodeRuntimePlan(bytecode, plan)) << entry.id;

        auto explain_result =
            executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) " + entry.sql);
        ASSERT_TRUE(explain_result.success()) << entry.id << ": "
                                              << explain_result.error();
        ASSERT_TRUE(explain_result.hasResultSet()) << entry.id;
        const auto explain_lines = resultStrings(explain_result);
        ASSERT_EQ(explain_lines.size(), 1u) << entry.id;

        const auto explain_json = nlohmann::json::parse(explain_lines.front());

        optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
        optimizer::QueryProfiler::getInstance().clearProfiles();
        auto count_result =
            executeSQL("SELECT COUNT(*) FROM (" + entry.sql + ") AS baseline_count");
        ASSERT_TRUE(count_result.success()) << entry.id << ": "
                                            << count_result.error();
        ASSERT_TRUE(count_result.hasResultSet()) << entry.id;
        const auto count_rows = resultStrings(count_result);
        ASSERT_EQ(count_rows.size(), 1u) << entry.id;

        uint64_t actual_rows = 0;
        try
        {
            actual_rows = static_cast<uint64_t>(std::stoull(count_rows.front()));
        }
        catch (const std::exception&)
        {
            FAIL() << entry.id << ": invalid row-count payload '" << count_rows.front()
                   << "'";
        }
        const double compile_mean_ms =
            meanDurationMs(3, [&]() {
                optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
                optimizer::QueryProfiler::getInstance().clearProfiles();
                return !compileSQL(entry.sql).empty();
            });
        ASSERT_GE(compile_mean_ms, 0.0) << entry.id;

        const double explain_mean_ms =
            meanDurationMs(3, [&]() {
                optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
                optimizer::QueryProfiler::getInstance().clearProfiles();
                auto result =
                    executeSQL("EXPLAIN (FORMAT JSON, ANALYZE, VERBOSE) " + entry.sql);
                return result.success() && result.hasResultSet();
            });
        ASSERT_GE(explain_mean_ms, 0.0) << entry.id;

        const double execute_mean_ms =
            meanDurationMs(3, [&]() {
                optimizer::QueryProfiler::getInstance().clearCardinalityFeedback();
                optimizer::QueryProfiler::getInstance().clearProfiles();
                auto result = executeSQL(entry.sql);
                return result.success();
            });
        ASSERT_GE(execute_mean_ms, 0.0) << entry.id;

        nlohmann::json join_steps = nlohmann::json::array();
        for (const auto& join_step : plan.join_steps)
        {
            join_steps.push_back(normalizedRuntimeJoinStep(join_step));
        }

        nlohmann::json query_summary;
        query_summary["id"] = entry.id;
        query_summary["sql"] = entry.sql;
        query_summary["runtime_plan_contract"] = plan.contract_id;
        query_summary["join_graph_contract"] = plan.join_graph_contract_id;
        query_summary["diagnostics_contract"] = plan.diagnostics_contract_id;
        query_summary["cache_mode"] = plan.cache_mode;
        query_summary["plan_profile_signature"] = plan.plan_profile_signature;
        query_summary["selected_strategy"] = plan.search_summary.selected_strategy;
        query_summary["requested_strategy"] = plan.search_summary.requested_strategy;
        query_summary["search_budget"] = plan.search_summary.search_budget;
        query_summary["considered_state_count"] =
            plan.search_summary.considered_state_count;
        query_summary["pruned_state_count"] =
            plan.search_summary.pruned_state_count;
        query_summary["rejected_candidate_count"] =
            plan.search_summary.rejected_candidate_count;
        query_summary["relation_count"] = plan.relations.size();
        query_summary["join_count"] = plan.join_steps.size();
        query_summary["root_node_type"] = plan.root.node_type;
        query_summary["estimated_root_rows"] = plan.root.estimated_rows;
        query_summary["actual_rows"] = actual_rows;
        query_summary["misestimate_ratio"] =
            normalizedMisestimateRatio(plan.root.estimated_rows, actual_rows);
        query_summary["compile_mean_ms"] = compile_mean_ms;
        query_summary["explain_mean_ms"] = explain_mean_ms;
        query_summary["execute_mean_ms"] = execute_mean_ms;
        query_summary["runtime_join_steps"] = std::move(join_steps);
        query_summary["explain_snapshot"] =
            normalizedExplainSnapshot(explain_json);
        baseline["queries"].push_back(std::move(query_summary));
    }

    const char* output_path = std::getenv("SB_OPTIMIZER_PARITY_BASELINE_JSON");
    if (output_path != nullptr && *output_path != '\0')
    {
        const std::filesystem::path path(output_path);
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open()) << path.string();
        out << baseline.dump(2) << '\n';
    }

    ASSERT_TRUE(baseline["queries"].is_array());
    ASSERT_EQ(baseline["queries"].size(), corpus.size());
    for (const auto& query : baseline["queries"])
    {
        EXPECT_EQ(query.at("runtime_plan_contract").get<std::string>(),
                  scratchbird::optimizer::kRuntimePlanContractId);
        EXPECT_EQ(query.at("join_graph_contract").get<std::string>(),
                  scratchbird::optimizer::kJoinGraphContractId);
        EXPECT_EQ(query.at("diagnostics_contract").get<std::string>(),
                  scratchbird::optimizer::kOptimizerDiagnosticsContractId);
        EXPECT_TRUE(query.at("misestimate_ratio").get<double>() >= 1.0);
    }
}

TEST_F(QueryPlannerIntegrationTest, CoveringIndexPlanUsesIndexOnlyScan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" + std::to_string(i) +
                               "', 'user" + std::to_string(i) +
                               "@example.com', " + std::to_string(20 + (i % 40)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_TRUE(plan.relations.front().covering_index);
    EXPECT_TRUE(plan.relations.front().exact_key_lookup);
    EXPECT_EQ(plan.relations.front().index_name, "idx_users_id_name");

    auto result = executeSQL("SELECT id, name FROM users WHERE id = 777");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "user777");
}

TEST_F(QueryPlannerIntegrationTest,
       MulticolumnOrderedAccessFamiliesSurviveIntoRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto &relation = plan.relations.front();
    EXPECT_EQ(relation.scan_kind, "INDEX_ONLY_SCAN");
    EXPECT_EQ(relation.scan_family, "ORDERED_INDEX_SCAN");
    EXPECT_TRUE(relation.ordered_output);
    EXPECT_GE(relation.ordered_prefix_length, 2u);
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "MULTICOLUMN_PREFIX_INDEX_SCAN"),
              relation.candidate_scan_families.end());
    EXPECT_NE(std::find(relation.candidate_scan_families.begin(),
                        relation.candidate_scan_families.end(),
                        "ORDERED_INDEX_SCAN"),
              relation.candidate_scan_families.end());

    auto result =
        executeSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 201u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1000");
}

TEST_F(QueryPlannerIntegrationTest, ExistingOrderedIndexPathAvoidsExplicitSortNode)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_id_name ON users (id, name)").success());
    for (int i = 1; i <= 1200; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'user" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 30)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode =
        compileSQL("SELECT id, name FROM users WHERE id >= 1000 ORDER BY id, name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "IndexOnlyScan");
    EXPECT_EQ(plan.root.children.size(), 0u);

    const auto trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(trace_it, plan.considered_paths.end());
    EXPECT_NE(trace_it->reason.find("satisfies ORDER BY"), std::string::npos);
}

TEST_F(QueryPlannerIntegrationTest, BitmapIndexPlanExecutesExactProbes)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE search_users (id INTEGER, age INTEGER, city VARCHAR(64), cohort VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL("GRANT SELECT ON search_users TO PUBLIC").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_search_users_age ON search_users (age)").success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_search_users_city ON search_users (city)").success());
    for (int i = 1; i <= 1600; ++i)
    {
        const int age = 20 + (i % 8);
        const std::string city = (i % 4 == 0) ? "Seattle" :
                                 (i % 4 == 1) ? "Austin" :
                                 (i % 4 == 2) ? "Boston" : "Denver";
        ASSERT_TRUE(executeSQL("INSERT INTO search_users (id, age, city, cohort) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(age) + ", '" +
                               city + "', 'c" + std::to_string(i % 5) + "')")
                        .success());
    }
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO search_users (id, age, city, cohort) VALUES (9001, 30, 'Seattle', 'target')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE search_users").success());

    auto bytecode =
        compileSQL("SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_kind, "BITMAP_INDEX_SCAN");
    EXPECT_EQ(plan.relations.front().bitmap_op, "AND");
    EXPECT_TRUE(plan.relations.front().exact_key_lookup);
    ASSERT_GE(plan.relations.front().index_predicates.size(), 2u);

    auto result =
        executeSQL("SELECT id FROM search_users WHERE age = 30 AND city = 'Seattle'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto rows = resultStrings(result);
    ASSERT_FALSE(rows.empty());
    EXPECT_NE(std::find(rows.begin(), rows.end(), "9001"), rows.end());
}

TEST_F(QueryPlannerIntegrationTest, SkipScanFamilyCanWinSingleRelationPlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(
        executeSQL("CREATE INDEX idx_users_age_name ON users (age, name)").success());
    for (int i = 1; i <= 1600; ++i)
    {
        const std::string name = (i == 777) ? "needle" : "user" + std::to_string(i);
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", '" + name + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 8)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE name = 'needle'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_EQ(plan.relations.front().scan_family, "SKIP_SCAN");
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "SKIP_SCAN"),
              plan.relations.front().candidate_scan_families.end());

    auto result = executeSQL("SELECT id FROM users WHERE name = 'needle'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "777");
}

TEST_F(QueryPlannerIntegrationTest,
       PartialAndExpressionIndexFamiliesAreEnumeratedInRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE INDEX idx_users_active_email ON users(email) WHERE age = 30")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE INDEX idx_users_lower_name ON users((LOWER(name)))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'MiXeD', 'mixed@example.com', 30)")
                    .success());
    for (int i = 2; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(i) + "', 'u" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 15)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL(
        "SELECT id FROM users "
        "WHERE age = 30 AND email = 'mixed@example.com' AND LOWER(name) = 'mixed'");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "PARTIAL_INDEX_SCAN"),
              plan.relations.front().candidate_scan_families.end());
    EXPECT_NE(std::find(plan.relations.front().candidate_scan_families.begin(),
                        plan.relations.front().candidate_scan_families.end(),
                        "EXPRESSION_INDEX_SCAN"),
              plan.relations.front().candidate_scan_families.end());

    auto result = executeSQL(
        "SELECT id FROM users "
        "WHERE age = 30 AND email = 'mixed@example.com' AND LOWER(name) = 'mixed'");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
}

TEST_F(QueryPlannerIntegrationTest, PassThroughViewIsFlattenedInRuntimePlan)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE VIEW v_users_flat AS SELECT * FROM users").success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (42, 'flattened', 'f@example.com', 25)")
                    .success());
    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM v_users_flat WHERE id = 42");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_TRUE(plan.relations.front().flattened_derived);
    EXPECT_FALSE(plan.relations.front().physical_table_path.empty());
    EXPECT_EQ(plan.relations.front().physical_table_path, "users");

    auto result = executeSQL("SELECT id FROM v_users_flat WHERE id = 42");
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "42");
}

TEST_F(QueryPlannerIntegrationTest, LateralJoinUsesParameterizedNestedLoopPath)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());

    const std::string sql =
        "SELECT u.id, o.order_id "
        "FROM users AS u "
        "LEFT JOIN LATERAL "
        "(SELECT id AS order_id FROM orders WHERE orders.user_id = u.id) AS o "
        "ON TRUE "
        "ORDER BY u.id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);
    EXPECT_TRUE(plan.relations[1].lateral);
    EXPECT_TRUE(plan.relations[1].parameterized);
    ASSERT_EQ(plan.join_steps.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].method, "PARAMETERIZED_NESTED_LOOP");
    EXPECT_TRUE(plan.join_steps[0].parameterized_dependency);
    ASSERT_EQ(plan.join_steps[0].legal_method_families.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].legal_method_families[0],
              "PARAMETERIZED_NESTED_LOOP");
    ASSERT_EQ(plan.join_steps[0].method_enablers.size(), 1u);
    EXPECT_EQ(plan.join_steps[0].method_enablers[0],
              "OUTER_PARAMETER_BINDING");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 2u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "10");
    EXPECT_EQ(result.resultSet()->getValue(1, 0).toString(), "2");
    EXPECT_TRUE(result.resultSet()->getValue(1, 1).isNull());
}

TEST_F(QueryPlannerIntegrationTest,
       LateralJoinPublishesParameterizedIndexCandidateFamily)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (1, 'alice', 'a@example.com', 30)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO users (id, name, email, age) VALUES (2, 'bob', 'b@example.com', 31)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (10, 1, 50.0)")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT u.id, o.order_id "
        "FROM users AS u "
        "LEFT JOIN LATERAL "
        "(SELECT id AS order_id FROM orders WHERE orders.user_id = u.id) AS o "
        "ON TRUE "
        "ORDER BY u.id";

    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 2u);

    const auto relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation &relation) {
                         return relation.alias == "o" || relation.table_path == "orders";
                     });
    ASSERT_NE(relation_it, plan.relations.end());
    EXPECT_TRUE(relation_it->parameterized);
    EXPECT_NE(std::find(relation_it->candidate_scan_families.begin(),
                        relation_it->candidate_scan_families.end(),
                        "PARAMETERIZED_INDEX_SCAN"),
              relation_it->candidate_scan_families.end());
    EXPECT_NE(std::find(relation_it->required_outer_relation_aliases.begin(),
                        relation_it->required_outer_relation_aliases.end(),
                        "u"),
              relation_it->required_outer_relation_aliases.end());
}

TEST_F(QueryPlannerIntegrationTest, CorrelatedExistsAndNotExistsUseOuterRowBindings)
{
    ASSERT_TRUE(createDatabase());

    for (int id : {1, 2, 3})
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(id) + ", 'u" + std::to_string(id) +
                               "', 'u" + std::to_string(id) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (100, 1, 25.0)")
                    .success());
    ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (101, 3, 30.0)")
                    .success());

    auto exists_result = executeSQL(
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id) "
        "ORDER BY u.id");
    ASSERT_TRUE(exists_result.success()) << exists_result.error();
    ASSERT_TRUE(exists_result.hasResultSet());
    ASSERT_EQ(exists_result.resultSet()->rowCount(), 2u);
    EXPECT_EQ(exists_result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(exists_result.resultSet()->getValue(1, 0).toString(), "3");

    auto not_exists_result = executeSQL(
        "SELECT u.id FROM users AS u "
        "WHERE NOT EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id) "
        "ORDER BY u.id");
    ASSERT_TRUE(not_exists_result.success()) << not_exists_result.error();
    ASSERT_TRUE(not_exists_result.hasResultSet());
    ASSERT_EQ(not_exists_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(not_exists_result.resultSet()->getValue(0, 0).toString(), "2");
}

TEST_F(QueryPlannerIntegrationTest, PartitionedParentSelectPrunesChildTargetsAndReturnsRows)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement (id INTEGER, region INTEGER, note VARCHAR(32)) "
                    "PARTITION BY RANGE (region)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_p1 (id INTEGER, region INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_p2 (id INTEGER, region INTEGER, note VARCHAR(32))")
                    .success());
    const auto create_metadata = loadTableMetadataJson("measurement");
    ASSERT_TRUE(create_metadata.contains("partition"));
    ASSERT_TRUE(create_metadata["partition"].is_object());
    EXPECT_EQ(create_metadata["partition"].value("strategy", std::string()), "RANGE");
    ASSERT_TRUE(create_metadata["partition"].contains("columns"));
    ASSERT_TRUE(create_metadata["partition"]["columns"].is_array());
    ASSERT_EQ(create_metadata["partition"]["columns"].size(), 1u);
    EXPECT_EQ(create_metadata["partition"]["columns"][0].get<std::string>(), "region");
    {
        auto attach_p1 = executeSQL(
            "ALTER TABLE measurement ATTACH PARTITION measurement_p1 "
            "FOR VALUES FROM (0) TO (100)");
        ASSERT_TRUE(attach_p1.success()) << attach_p1.error();
    }
    {
        auto attach_p2 = executeSQL(
            "ALTER TABLE measurement ATTACH PARTITION measurement_p2 "
            "FOR VALUES FROM (100) TO (200)");
        ASSERT_TRUE(attach_p2.success()) << attach_p2.error();
    }

    const auto metadata = loadTableMetadataJson("measurement");
    ASSERT_TRUE(metadata.contains("partition"));
    ASSERT_TRUE(metadata["partition"].is_object());
    EXPECT_EQ(metadata["partition"].value("strategy", std::string()), "RANGE");
    ASSERT_TRUE(metadata["partition"].contains("columns"));
    ASSERT_TRUE(metadata["partition"]["columns"].is_array());
    ASSERT_EQ(metadata["partition"]["columns"].size(), 1u);
    EXPECT_EQ(metadata["partition"]["columns"][0].get<std::string>(), "region");
    ASSERT_TRUE(metadata["partition"].contains("children"));
    ASSERT_TRUE(metadata["partition"]["children"].is_array());
    ASSERT_EQ(metadata["partition"]["children"].size(), 2u);

    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_p1 (id, region, note) VALUES (1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_p2 (id, region, note) VALUES (2, 150, 'east')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_p1").success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_p2").success());

    const std::string sql =
        "SELECT id, note FROM measurement WHERE region >= 100 AND region < 200 ORDER BY id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    EXPECT_TRUE(plan.relations.front().partition_pruned);
    EXPECT_EQ(plan.relations.front().partition_strategy, "RANGE");
    EXPECT_EQ(plan.relations.front().partition_key_column, "region");
    ASSERT_EQ(plan.relations.front().partition_targets.size(), 1u);
    EXPECT_EQ(plan.relations.front().partition_targets.front(), "measurement_p2");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, MultiColumnStaticPartitionPruningPublishesPrunedTargets)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32)) "
                    "PARTITION BY LIST (region, bucket)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p1 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p2 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p1 "
                    "FOR VALUES IN ((1, 10), (1, 20))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p2 "
                    "FOR VALUES IN ((2, 30), (2, 40))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p1 (id, region, bucket, note) "
                    "VALUES (1, 1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p2 (id, region, bucket, note) "
                    "VALUES (2, 2, 40, 'east')")
                    .success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_mc_p1").success());
    ASSERT_TRUE(executeSQL("ANALYZE measurement_mc_p2").success());

    const std::string sql =
        "SELECT id, note FROM measurement_mc WHERE region = 2 AND bucket = 40";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    EXPECT_TRUE(relation.partition_pruned);
    EXPECT_EQ(relation.partition_strategy, "LIST");
    ASSERT_EQ(relation.partition_key_columns.size(), 2u);
    EXPECT_EQ(relation.partition_key_columns[0], "region");
    EXPECT_EQ(relation.partition_key_columns[1], "bucket");
    ASSERT_EQ(relation.partition_targets.size(), 1u);
    EXPECT_EQ(relation.partition_targets.front(), "measurement_mc_p2");
    ASSERT_EQ(relation.partition_targets_pruned_at_plan.size(), 1u);
    EXPECT_EQ(relation.partition_targets_pruned_at_plan.front(),
              "measurement_mc_p1");
    EXPECT_FALSE(relation.runtime_partition_pruning_eligible);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, ParameterSensitivePartitionPruningUsesBoundValuesAtPlanTime)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32)) "
                    "PARTITION BY LIST (region, bucket)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p1 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p2 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p1 "
                    "FOR VALUES IN ((1, 10), (1, 20))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p2 "
                    "FOR VALUES IN ((2, 30), (2, 40))")
                    .success());

    optimizer::ParameterBindings bindings;
    bindings.positional.push_back({false, "2"});
    bindings.positional.push_back({false, "40"});

    auto compile_result = compileSQLWithParameters(
        "SELECT id FROM measurement_mc WHERE region = $1 AND bucket = $2",
        bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(compile_result.bytecode(), plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    EXPECT_TRUE(plan.parameter_sensitive);
    EXPECT_TRUE(relation.partition_pruned);
    EXPECT_FALSE(relation.runtime_partition_pruning_eligible);
    ASSERT_EQ(relation.partition_targets.size(), 1u);
    EXPECT_EQ(relation.partition_targets.front(), "measurement_mc_p2");
    ASSERT_EQ(relation.partition_targets_pruned_at_plan.size(), 1u);
    EXPECT_EQ(relation.partition_targets_pruned_at_plan.front(),
              "measurement_mc_p1");
}

TEST_F(QueryPlannerIntegrationTest, GenericPlanRuntimePartitionPruningUsesLateParameterValues)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32)) "
                    "PARTITION BY LIST (region, bucket)")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p1 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "CREATE TABLE measurement_mc_p2 "
                    "(id INTEGER, region INTEGER, bucket INTEGER, note VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p1 "
                    "FOR VALUES IN ((1, 10), (1, 20))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "ALTER TABLE measurement_mc ATTACH PARTITION measurement_mc_p2 "
                    "FOR VALUES IN ((2, 30), (2, 40))")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p1 (id, region, bucket, note) "
                    "VALUES (1, 1, 10, 'west')")
                    .success());
    ASSERT_TRUE(executeSQL(
                    "INSERT INTO measurement_mc_p2 (id, region, bucket, note) "
                    "VALUES (2, 2, 40, 'east')")
                    .success());

    connection_ctx_->setSessionVariable("OPTIMIZER.PLAN_DIRECTIVES",
                                        "PLAN_PROFILE=GENERIC");
    optimizer::ParameterBindings compile_bindings;
    compile_bindings.positional.push_back({false, "1"});
    compile_bindings.positional.push_back({false, "10"});

    auto compile_result = compileSQLWithParameters(
        "SELECT id, note FROM measurement_mc WHERE region = $1 AND bucket = $2",
        compile_bindings);
    ASSERT_TRUE(compile_result.success()) << last_compile_errors_;
    EXPECT_EQ(compile_result.planProfile().mode,
              scratchbird::sblr::detail::QueryCompilerV3PlanProfileMode::GENERIC);
    EXPECT_FALSE(compile_result.planProfile().parameter_sensitive);
    const auto& bytecode = compile_result.bytecode();

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.relations.size(), 1u);
    const auto& relation = plan.relations.front();
    EXPECT_FALSE(plan.parameter_sensitive);
    EXPECT_FALSE(relation.partition_pruned);
    EXPECT_TRUE(relation.runtime_partition_pruning_eligible);
    ASSERT_EQ(relation.runtime_partition_pruning_sources.size(), 1u);
    EXPECT_EQ(relation.runtime_partition_pruning_sources.front(), "PARAMETER");
    ASSERT_EQ(relation.partition_predicates.size(), 2u);

    auto result = executeBytecodeWithParameters(bytecode,
                                                {"2", "40"},
                                                {false, false});
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    CatalogManager::TableInfo p1_info;
    CatalogManager::TableInfo p2_info;
    ErrorContext table_ctx;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "measurement_mc_p1",
                                               p1_info,
                                               &table_ctx),
              Status::OK)
        << table_ctx.message;
    ASSERT_EQ(db_->catalog_manager()->getTable(connection_ctx_->getCurrentSchemaId(),
                                               "measurement_mc_p2",
                                               p2_info,
                                               &table_ctx),
              Status::OK)
        << table_ctx.message;

    const auto& touched_tables = executor_->getLastSelectTableIds();
    EXPECT_EQ(touched_tables.count(p1_info.table_id), 0u);
    EXPECT_EQ(touched_tables.count(p2_info.table_id), 1u);
    ASSERT_EQ(result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "2");
    EXPECT_EQ(result.resultSet()->getValue(0, 1).toString(), "east");
}

TEST_F(QueryPlannerIntegrationTest, JoinRuntimeFilterUsesRightSideIndexMetadata)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)").success());
    for (int id = 1; id <= 4; ++id)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(id) + ", 'u" + std::to_string(id) +
                               "', 'u" + std::to_string(id) + "@example.com', 30)")
                        .success());
    }
    for (int order_id = 1; order_id <= 12; ++order_id)
    {
        const int user_id = ((order_id - 1) % 4) + 1;
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(order_id) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + order_id) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT o.user_id, o.amount "
        "FROM users AS u "
        "JOIN orders AS o ON u.id = o.user_id "
        "WHERE u.id <= 2 "
        "ORDER BY o.user_id, o.amount";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    auto relation_it =
        std::find_if(plan.relations.begin(),
                     plan.relations.end(),
                     [](const scratchbird::optimizer::RuntimePlanRelation& relation) {
                         return relation.table_path == "orders" || relation.alias == "o";
                     });
    ASSERT_NE(relation_it, plan.relations.end());
    EXPECT_TRUE(relation_it->runtime_filter_enabled);
    EXPECT_EQ(relation_it->runtime_filter_column, "user_id");
    EXPECT_EQ(relation_it->runtime_filter_index_name, "idx_orders_user_id");

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 6u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(5, 0).toString(), "2");
}

TEST_F(QueryPlannerIntegrationTest, TopNSortPlanAnnotatesRuntimePlanAndExecutesCorrectly)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'topn" + std::to_string(i) +
                               "', 'topn" + std::to_string(i) +
                               "@example.com', 20)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT id FROM users ORDER BY id DESC LIMIT 5";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Limit");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Sort");
    EXPECT_NE(plan.root.children.front().detail_text.find("topn=5"), std::string::npos);

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 5u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "128");
    EXPECT_EQ(result.resultSet()->getValue(4, 0).toString(), "124");
}

TEST_F(QueryPlannerIntegrationTest,
       GroupedAggregateReusesOrderedInputAndAvoidsExplicitSort)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    for (int i = 1; i <= 8192; ++i)
    {
        const int user_id = 1 + (i % 32);
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + (i % 17)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT user_id, COUNT(*) "
        "FROM orders "
        "WHERE user_id <= 5 "
        "GROUP BY user_id "
        "ORDER BY user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto group_trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:group_by" &&
                                entry.candidate == "GROUP_ORDER_REUSE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(group_trace_it, plan.considered_paths.end());

    const auto order_trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(order_trace_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 5u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "1");
    EXPECT_EQ(result.resultSet()->getValue(4, 0).toString(), "5");
}

TEST_F(QueryPlannerIntegrationTest,
       AggregateStagePublishesHashStrategyWhenGroupOrderUnavailable)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'agg" +
                               std::to_string(i) + "', 'agg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 7)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT age, COUNT(*) FROM users GROUP BY age";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:aggregate" &&
                                entry.candidate == "HASH_AGGREGATE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:aggregate" &&
                                entry.candidate == "GROUP_AGGREGATE_REUSE_ORDER" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       DistinctStageUsesOrderedDistinctWhenInputOrderIsAvailable)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_orders_user_id ON orders (user_id)")
                    .success());
    for (int i = 1; i <= 512; ++i)
    {
        const int user_id = 1 + (i % 8);
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " +
                               std::to_string(user_id) + ", " +
                               std::to_string(10 + (i % 11)) + ".0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    const std::string sql =
        "SELECT DISTINCT user_id FROM orders WHERE user_id <= 5 ORDER BY user_id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Aggregate");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");
    EXPECT_EQ(plan.root.detail_text, "DISTINCT");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "ORDERED_DISTINCT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows.front(), "1");
    EXPECT_EQ(rows.back(), "5");
}

TEST_F(QueryPlannerIntegrationTest,
       DistinctStageUsesHashDistinctWhenOrderIsUnavailable)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'distinct" +
                               std::to_string(i) + "', 'distinct" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 6)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT DISTINCT age FROM users");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "HASH_DISTINCT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:distinct" &&
                                entry.candidate == "ORDERED_DISTINCT" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       WindowPreservesOrderedInputAndAvoidsFinalSort)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'wuser" +
                               std::to_string(i) + "', 'wuser" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT id, ROW_NUMBER() OVER (ORDER BY id) AS rn "
        "FROM users "
        "WHERE id >= 200 "
        "ORDER BY id";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Window");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto trace_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(trace_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    ASSERT_EQ(result.resultSet()->rowCount(), 57u);
    EXPECT_EQ(result.resultSet()->getValue(0, 0).toString(), "200");
    EXPECT_EQ(result.resultSet()->getValue(56, 0).toString(), "256");
}

TEST_F(QueryPlannerIntegrationTest,
       WindowStagePublishesLocalSortStrategyWhenOrderIsMissing)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 64; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'z" +
                               std::to_string(65 - i) + "', 'window" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }

    auto bytecode =
        compileSQL("SELECT ROW_NUMBER() OVER (ORDER BY name) FROM users");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:window" &&
                                entry.candidate == "WINDOW_LOCAL_SORT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:window" &&
                                entry.candidate == "WINDOW_REUSE_ORDER" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       UnorderedInputRequiresExplicitSortAndRecordsRejectionTrace)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        const int reversed = 129 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'user" +
                               std::to_string(reversed) + "', 'sort" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql = "SELECT name FROM users ORDER BY name";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Sort");
    ASSERT_EQ(plan.root.children.size(), 1u);

    const auto rejected_it =
        std::find_if(plan.rejected_paths.begin(),
                     plan.rejected_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_AVOIDANCE" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "EXISTING_ORDER" &&
                                entry.verdict == "REJECTED";
                     });
    ASSERT_NE(rejected_it, plan.rejected_paths.end());

    const auto sort_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "SORT_STRATEGY" &&
                                entry.subject == "query:order_by" &&
                                entry.candidate == "FULL_SORT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(sort_it, plan.considered_paths.end());

    auto result = executeSQL(sql);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    const auto rows = resultStrings(result);
    ASSERT_EQ(rows.size(), 128u);
    EXPECT_TRUE(std::is_sorted(rows.begin(), rows.end()));
    EXPECT_EQ(rows.front(), "user1");
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedLimitPublishesOrderedLimitStrategyWithoutSortNode)
{
    ASSERT_TRUE(createDatabase());

    ASSERT_TRUE(executeSQL("CREATE INDEX idx_users_id ON users (id)").success());
    for (int i = 1; i <= 256; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'limit" +
                               std::to_string(i) + "', 'limit" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    const std::string sql =
        "SELECT id FROM users WHERE id >= 200 ORDER BY id LIMIT 5";
    auto bytecode = compileSQL(sql);
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    EXPECT_EQ(plan.root.node_type, "Limit");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_NE(plan.root.children.front().node_type, "Sort");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "UPPER_STAGE_STRATEGY" &&
                                entry.subject == "query:top_n" &&
                                entry.candidate == "ORDERED_LIMIT" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());
}

TEST_F(QueryPlannerIntegrationTest,
       ParallelSeqScanWrapsPlanInGatherAndPublishesRelationMetadata)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 2048; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pseq" +
                               std::to_string(i) + "', 'pseq" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(18 + (i % 5)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT id FROM users WHERE age >= 18");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Gather");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "SeqScan");
    EXPECT_TRUE(plan.root.parallel_enabled);
    EXPECT_GT(plan.root.parallel_workers_planned, 1u);
    ASSERT_FALSE(plan.relations.empty());
    EXPECT_TRUE(plan.relations.front().parallel_enabled);
    EXPECT_EQ(plan.relations.front().parallel_stage, "SCAN");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "PARALLEL" &&
                                entry.candidate == "PARALLEL_SEQ_SCAN" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    auto result = executeSQL("SELECT id FROM users WHERE age >= 18");
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(resultRowCount(result), 2048u);
}

TEST_F(QueryPlannerIntegrationTest,
       ParallelHashJoinWrapsPlanInGatherAndPublishesJoinMetadata)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();
    connection_ctx_->setSessionVariable("OPTIMIZER.JOIN_METHOD", "HASH_JOIN");

    for (int i = 1; i <= 4096; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'phj" +
                               std::to_string(i) + "', 'phj" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
        ASSERT_TRUE(executeSQL("INSERT INTO orders (id, user_id, amount) VALUES (" +
                               std::to_string(i) + ", " + std::to_string(i) +
                               ", 10.0)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());
    ASSERT_TRUE(executeSQL("ANALYZE orders").success());

    auto bytecode = compileSQL(
        "SELECT users.id FROM users JOIN orders ON users.id = orders.user_id");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Gather");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "HashJoin");
    ASSERT_FALSE(plan.join_steps.empty());
    EXPECT_TRUE(plan.join_steps.back().parallel_enabled);
    EXPECT_EQ(plan.join_steps.back().parallel_stage, "HASH_JOIN");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "PARALLEL" &&
                                entry.candidate == "PARALLEL_HASH_JOIN" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    auto result = executeSQL(
        "SELECT users.id FROM users JOIN orders ON users.id = orders.user_id");
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(resultRowCount(result), 4096u);
}

TEST_F(QueryPlannerIntegrationTest,
       ParallelAggregateWrapsPlanInGatherAndPublishesStageMetadata)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 8192; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pagg" +
                               std::to_string(i) + "', 'pagg" +
                               std::to_string(i) + "@example.com', " +
                               std::to_string(20 + (i % 16)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT age, COUNT(*) FROM users GROUP BY age");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "Gather");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Aggregate");
    EXPECT_TRUE(plan.root.children.front().parallel_aware);
    EXPECT_TRUE(plan.root.children.front().parallel_enabled);
    EXPECT_EQ(plan.root.children.front().parallel_stage, "AGGREGATE");

    const auto chosen_it =
        std::find_if(plan.considered_paths.begin(),
                     plan.considered_paths.end(),
                     [](const scratchbird::optimizer::RuntimePlanTraceEntry &entry) {
                         return entry.phase == "PARALLEL" &&
                                entry.candidate == "PARALLEL_AGGREGATE" &&
                                entry.verdict == "CHOSEN";
                     });
    ASSERT_NE(chosen_it, plan.considered_paths.end());

    auto result = executeSQL("SELECT age, COUNT(*) FROM users GROUP BY age");
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_EQ(resultRowCount(result), 16u);
}

TEST_F(QueryPlannerIntegrationTest,
       OrderedParallelPlanUsesGatherMergeAndExplainJsonPublishesParallelFields)
{
    ASSERT_TRUE(createDatabase());
    enableParallelPlanning();

    for (int i = 1; i <= 8192; ++i)
    {
        const int reversed = 2049 - i;
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'pgm" +
                               std::to_string(reversed) + "', 'pgm" +
                               std::to_string(i) + "@example.com', 30)")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto bytecode = compileSQL("SELECT name FROM users ORDER BY name");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    scratchbird::optimizer::RuntimePlan plan;
    ASSERT_TRUE(decodeRuntimePlan(bytecode, plan));
    ASSERT_EQ(plan.root.node_type, "GatherMerge");
    ASSERT_EQ(plan.root.children.size(), 1u);
    EXPECT_EQ(plan.root.children.front().node_type, "Sort");
    EXPECT_TRUE(plan.root.parallel_enabled);
    EXPECT_TRUE(plan.root.gather_merge);
    EXPECT_EQ(plan.root.parallel_stage, "GATHER_MERGE");

    auto explain_result =
        executeSQL("EXPLAIN (FORMAT JSON) SELECT name FROM users ORDER BY name");
    ASSERT_TRUE(explain_result.success()) << explain_result.error();
    const auto explain_lines = resultStrings(explain_result);
    ASSERT_EQ(explain_lines.size(), 1u);
    const auto explain_json = nlohmann::json::parse(explain_lines.front());
    ASSERT_TRUE(explain_json.contains("plan_root"));
    const auto &plan_root = explain_json.at("plan_root");
    EXPECT_EQ(plan_root.value("node_type", std::string()), "GatherMerge");
    EXPECT_TRUE(plan_root.value("parallel_enabled", false));
    EXPECT_TRUE(plan_root.value("gather_merge", false));
    EXPECT_EQ(plan_root.value("parallel_stage", std::string()),
              "GATHER_MERGE");
}

TEST_F(QueryPlannerIntegrationTest, CorrelatedExistsPreservesQualifiedOuterReferenceInBytecode)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL(
        "SELECT u.id FROM users AS u "
        "WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = u.id)");
    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    sblr_v3::Instruction select_inst;
    ASSERT_TRUE(decodeFirstSelect(bytecode, select_inst));

    sblr_v3::Instruction exists_inst;
    ASSERT_TRUE(findFirstOpcode(select_inst, sblr_v3::Opcode::SBLR3_SUBQUERY_EXISTS, exists_inst));

    const auto* exists_obj = std::get_if<sblr_v3::Value::Object>(&exists_inst.payload.data);
    ASSERT_NE(exists_obj, nullptr);
    auto query_it = exists_obj->find("query");
    ASSERT_NE(query_it, exists_obj->end());
    const auto* query_ptr = std::get_if<sblr_v3::Value::InstrPtr>(&query_it->second.data);
    ASSERT_NE(query_ptr, nullptr);
    ASSERT_TRUE(*query_ptr != nullptr);

    const auto refs = collectColumnRefs(**query_ptr);
    EXPECT_NE(std::find(refs.begin(), refs.end(), std::make_pair(std::string("orders"),
                                                                 std::string("user_id"))),
              refs.end());
    EXPECT_NE(std::find(refs.begin(), refs.end(), std::make_pair(std::string("u"),
                                                                 std::string("id"))),
              refs.end());
}

TEST_F(QueryPlannerIntegrationTest, RuntimePlanCapturesAggregateWindowSortAndLimitNodes)
{
    ASSERT_TRUE(createDatabase());

    for (int i = 1; i <= 128; ++i)
    {
        ASSERT_TRUE(executeSQL("INSERT INTO users (id, name, email, age) VALUES (" +
                               std::to_string(i) + ", 'window" + std::to_string(i) +
                               "', 'window" + std::to_string(i) +
                               "@example.com', " + std::to_string(18 + (i % 10)) + ")")
                        .success());
    }
    ASSERT_TRUE(executeSQL("ANALYZE users").success());

    auto aggregate_bytecode = compileSQL("SELECT COUNT(*) FROM users");
    ASSERT_FALSE(aggregate_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan aggregate_plan;
    ASSERT_TRUE(decodeRuntimePlan(aggregate_bytecode, aggregate_plan));
    EXPECT_EQ(aggregate_plan.root.node_type, "Aggregate");

    auto window_bytecode =
        compileSQL("SELECT ROW_NUMBER() OVER (ORDER BY id) FROM users");
    ASSERT_FALSE(window_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan window_plan;
    ASSERT_TRUE(decodeRuntimePlan(window_bytecode, window_plan));
    EXPECT_EQ(window_plan.root.node_type, "Window");

    auto ordered_bytecode =
        compileSQL("SELECT id FROM users ORDER BY id DESC LIMIT 5");
    ASSERT_FALSE(ordered_bytecode.empty()) << last_compile_errors_;
    scratchbird::optimizer::RuntimePlan ordered_plan;
    ASSERT_TRUE(decodeRuntimePlan(ordered_bytecode, ordered_plan));
    EXPECT_EQ(ordered_plan.root.node_type, "Limit");
    ASSERT_EQ(ordered_plan.root.children.size(), 1u);
    EXPECT_EQ(ordered_plan.root.children.front().node_type, "Sort");
}

TEST_F(QueryPlannerIntegrationTest, BytecodeContainsVersionHeader)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    ASSERT_FALSE(bytecode.empty());
    sblr_v3::Container container;
    std::string err;
    ASSERT_TRUE(sblr_v3::decodeContainer(bytecode.data(), bytecode.size(), container, err))
        << err;
    EXPECT_EQ(std::string(container.header.magic, 4), std::string("SBL3"));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users WHERE id > 10");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_GT));
}

TEST_F(QueryPlannerIntegrationTest, NonSelectStatementsBypassPlanner)
{
    ASSERT_TRUE(createDatabase());

    auto insert_bytecode = compileSQL("INSERT INTO users (id) VALUES (1)");
    EXPECT_FALSE(insert_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(insert_bytecode, sblr_v3::Opcode::SBLR3_INSERT));

    auto create_bytecode = compileSQL("CREATE TABLE test2 (id INTEGER)");
    EXPECT_FALSE(create_bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(create_bytecode, sblr_v3::Opcode::SBLR3_CREATE_TABLE));
}

// ===== Stress Tests =====

TEST_F(QueryPlannerIntegrationTest, ComplexQueryWithPlanner)
{
    ASSERT_TRUE(createDatabase());

    std::string complex_sql =
        "SELECT id, name, price * 1.1 "
        "FROM products "
        "WHERE price >= 100.0 AND name LIKE '%sale%'";

    auto bytecode = compileSQL(complex_sql);
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_AND));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_GE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_LIKE));
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_EXPR_MULTIPLY));
}

TEST_F(QueryPlannerIntegrationTest, MultipleSelectStatements)
{
    ASSERT_TRUE(createDatabase());

    std::vector<std::string> queries = {
        "SELECT * FROM users",
        "SELECT id FROM users WHERE id > 10",
        "SELECT name, email FROM users",
        "SELECT COUNT(*) FROM users"
    };

    for (const auto &sql : queries)
    {
        auto bytecode = compileSQL(sql);
        EXPECT_FALSE(bytecode.empty()) << "Failed for query: " << sql << "\n"
                                       << last_compile_errors_;
        EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT))
            << "Missing SELECT opcode for query: " << sql;
    }
}

// ===== Edge Cases =====

TEST_F(QueryPlannerIntegrationTest, EmptySelectList)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT * FROM test");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT_TABLE_STAR) ||
                containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_COLUMN_REF));
}

TEST_F(QueryPlannerIntegrationTest, SelectWithoutWhereClause)
{
    ASSERT_TRUE(createDatabase());

    auto bytecode = compileSQL("SELECT id, name FROM users");
    EXPECT_FALSE(bytecode.empty()) << last_compile_errors_;
    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
    EXPECT_FALSE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_WHERE_CLAUSE));
}

// ===== Diagnostic Test =====

TEST_F(QueryPlannerIntegrationTest, DiagnosticDisassemblyOutput)
{
    ASSERT_TRUE(createDatabase());

    std::string sql = "SELECT id, name FROM users WHERE id = 42";
    auto bytecode = compileSQL(sql);

    ASSERT_FALSE(bytecode.empty()) << last_compile_errors_;

    EXPECT_TRUE(containsOpcode(bytecode, sblr_v3::Opcode::SBLR3_SELECT));
}
