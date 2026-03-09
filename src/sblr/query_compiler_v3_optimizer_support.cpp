#include "scratchbird/sblr/query_compiler_v3_optimizer_support.h"

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_plan_cache_key.h"

#include <atomic>
#include <cctype>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>

namespace scratchbird::sblr::detail
{
    namespace
    {
        optimizer::VNextPlanCache &planCache()
        {
            static optimizer::VNextPlanCache cache;
            return cache;
        }

        std::atomic<uint64_t> &compilerCatalogEpoch()
        {
            static std::atomic<uint64_t> epoch{1};
            return epoch;
        }

        std::atomic<uint64_t> &compilerSecurityEpoch()
        {
            static std::atomic<uint64_t> epoch{1};
            return epoch;
        }

        auto normalizeSql(std::string_view sql) -> std::string
        {
            std::string out;
            out.reserve(sql.size());

            bool in_single_quote = false;
            bool in_double_quote = false;
            bool prev_space = false;

            for (char c : sql)
            {
                if (c == '\'' && !in_double_quote)
                {
                    in_single_quote = !in_single_quote;
                    out.push_back(c);
                    prev_space = false;
                    continue;
                }
                if (c == '"' && !in_single_quote)
                {
                    in_double_quote = !in_double_quote;
                    out.push_back(c);
                    prev_space = false;
                    continue;
                }

                if (!in_single_quote && !in_double_quote)
                {
                    if (std::isspace(static_cast<unsigned char>(c)) != 0)
                    {
                        if (!prev_space)
                        {
                            out.push_back(' ');
                            prev_space = true;
                        }
                        continue;
                    }
                    out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                    prev_space = false;
                }
                else
                {
                    out.push_back(c);
                    prev_space = false;
                }
            }

            if (!out.empty() && out.back() == ' ')
            {
                out.pop_back();
            }
            return out;
        }

        auto hashTextHex(const std::string &text) -> std::string
        {
            return std::to_string(sblr::v3::stableHash64(text));
        }

        auto hashBytesHex(const std::vector<uint8_t> &bytes) -> std::string
        {
            return std::to_string(
                sblr::v3::stableHash64(std::string_view(
                    reinterpret_cast<const char *>(bytes.data()),
                    bytes.size())));
        }

        auto joinTypeToCode(const std::string &join_type) -> uint64_t
        {
            if (join_type == "LEFT") return 2;
            if (join_type == "RIGHT") return 3;
            if (join_type == "FULL") return 4;
            if (join_type == "CROSS") return 5;
            if (join_type == "NATURAL") return 6;
            if (join_type == "NATURAL_LEFT") return 7;
            if (join_type == "NATURAL_RIGHT") return 8;
            if (join_type == "NATURAL_FULL") return 9;
            return 1;
        }

        auto decodeInstructions(const std::vector<uint8_t> &stream,
                                std::vector<sblr::v3::Instruction> &instructions_out,
                                std::string &error_out) -> bool
        {
            instructions_out.clear();
            size_t offset = 0;
            sblr::v3::DecodeError decode_error;
            while (offset < stream.size())
            {
                sblr::v3::Instruction inst;
                if (!sblr::v3::decodeInstructionWithSchema(stream.data(),
                                                           stream.size(),
                                                           offset,
                                                           inst,
                                                           decode_error))
                {
                    error_out = decode_error.message.empty() ?
                        "Failed to decode V3 instruction stream" :
                        decode_error.message;
                    return false;
                }
                instructions_out.push_back(std::move(inst));
            }
            return true;
        }

        auto encodeInstructions(const std::vector<sblr::v3::Instruction> &instructions,
                                std::vector<uint8_t> &stream_out,
                                std::string &error_out) -> bool
        {
            stream_out.clear();
            sblr::v3::DecodeError encode_error;
            for (const auto &inst : instructions)
            {
                if (!sblr::v3::encodeInstructionWithSchema(inst, stream_out, encode_error))
                {
                    error_out = encode_error.message.empty() ?
                        "Failed to encode V3 instruction stream" :
                        encode_error.message;
                    return false;
                }
            }
            return true;
        }

        auto valueObject(sblr::v3::Value &value) -> sblr::v3::Value::Object *
        {
            return std::get_if<sblr::v3::Value::Object>(&value.data);
        }

        auto instructionObject(sblr::v3::Instruction &inst) -> sblr::v3::Value::Object *
        {
            return valueObject(inst.payload);
        }

        auto isCacheableSelect(const parser::v3::Statement *stmt,
                               const parser::v3::SelectStmt *&select_out,
                               bool &is_explain) -> bool
        {
            is_explain = false;
            select_out = nullptr;
            if (stmt == nullptr)
            {
                return false;
            }

            if (stmt->kind() == parser::v3::ASTKind::SelectStmt)
            {
                select_out = static_cast<const parser::v3::SelectStmt *>(stmt);
                return true;
            }

            if (stmt->kind() == parser::v3::ASTKind::ExplainStmt)
            {
                const auto *explain = static_cast<const parser::v3::ExplainStmt *>(stmt);
                if (explain->query != nullptr &&
                    explain->query->kind() == parser::v3::ASTKind::SelectStmt)
                {
                    select_out = static_cast<const parser::v3::SelectStmt *>(explain->query);
                    is_explain = true;
                    return true;
                }
            }

            return false;
        }

        auto selectPayloadRelationRefs(const sblr::v3::Value::Object &payload,
                                       std::vector<sblr::v3::Value> &relations_out,
                                       std::vector<sblr::v3::Value::Object> &joins_out) -> bool
        {
            relations_out.clear();
            joins_out.clear();

            auto from_it = payload.find("from");
            if (from_it == payload.end() || from_it->second.isNull())
            {
                return true;
            }
            relations_out.push_back(from_it->second);

            auto joins_it = payload.find("joins");
            if (joins_it == payload.end())
            {
                return true;
            }

            const auto *join_list = std::get_if<sblr::v3::Value::List>(&joins_it->second.data);
            if (join_list == nullptr)
            {
                return false;
            }

            for (const auto &join_value : *join_list)
            {
                const auto *join_obj =
                    std::get_if<sblr::v3::Value::Object>(&join_value.data);
                if (join_obj == nullptr)
                {
                    return false;
                }
                joins_out.push_back(*join_obj);
                auto right_it = join_obj->find("right");
                if (right_it == join_obj->end())
                {
                    return false;
                }
                relations_out.push_back(right_it->second);
            }

            return true;
        }

        auto rewriteSelectPayloadForPlan(sblr::v3::Value::Object &payload,
                                         const optimizer::RuntimePlan &runtime_plan,
                                         std::vector<uint8_t> plan_bytes,
                                         std::string &error_out) -> bool
        {
            std::vector<sblr::v3::Value> relation_refs;
            std::vector<sblr::v3::Value::Object> original_joins;
            if (!selectPayloadRelationRefs(payload, relation_refs, original_joins))
            {
                error_out = "SELECT payload relation layout is invalid";
                return false;
            }

            if (runtime_plan.relations.empty())
            {
                payload["plan"] = sblr::v3::Value(std::move(plan_bytes));
                return true;
            }

            if (relation_refs.size() != runtime_plan.relations.size())
            {
                error_out = "SELECT payload relation count does not match optimizer plan";
                return false;
            }

            const size_t first_relation_index = runtime_plan.relations.front().source_relation_index;
            if (first_relation_index >= relation_refs.size())
            {
                error_out = "Optimizer plan relation index is out of range";
                return false;
            }
            payload["from"] = relation_refs[first_relation_index];

            sblr::v3::Value::List rewritten_joins;
            for (const auto &join_step : runtime_plan.join_steps)
            {
                if (join_step.right_relation_index >= relation_refs.size())
                {
                    error_out = "Optimizer join relation index is out of range";
                    return false;
                }

                sblr::v3::Value::Object join_obj;
                join_obj["type"] = sblr::v3::Value(joinTypeToCode(join_step.join_type));
                join_obj["right"] = relation_refs[join_step.right_relation_index];

                if (join_step.source_join_index < original_joins.size())
                {
                    const auto &original_join = original_joins[join_step.source_join_index];
                    auto condition_it = original_join.find("condition");
                    if (condition_it != original_join.end())
                    {
                        join_obj["condition"] = condition_it->second;
                    }
                    auto using_it = original_join.find("using");
                    if (using_it != original_join.end())
                    {
                        join_obj["using"] = using_it->second;
                    }
                }
                else
                {
                    join_obj["using"] = sblr::v3::Value(sblr::v3::Value::List{});
                }

                rewritten_joins.push_back(sblr::v3::Value(std::move(join_obj)));
            }

            payload["joins"] = sblr::v3::Value(std::move(rewritten_joins));
            payload["plan"] = sblr::v3::Value(std::move(plan_bytes));
            return true;
        }

        auto applyPlannedSelect(core::Database *db,
                                const parser::v3::SelectStmt *select_stmt,
                                const parser::v3::StringPool &pool,
                                const core::ID &current_schema_id,
                                sblr::v3::Instruction &select_inst,
                                std::vector<std::string> &warnings,
                                std::string &error_out) -> bool
        {
            optimizer::QueryPlanner planner(db,
                                            optimizer::CostModel(),
                                            db != nullptr ? db->statistics_manager() : nullptr);
            optimizer::PlannedSelectQuery planned;
            core::ErrorContext plan_ctx;
            const auto status = planner.buildSelectPlan(select_stmt,
                                                       pool,
                                                       planned,
                                                       &plan_ctx,
                                                       core::ConnectionContext::getCurrent(),
                                                       current_schema_id);
            if (status != core::Status::OK)
            {
                warnings.push_back(plan_ctx.message.empty() ?
                    "Optimizer planning failed; continuing without runtime plan" :
                    plan_ctx.message);
                return true;
            }

            std::vector<uint8_t> plan_bytes;
            if (!optimizer::encodeRuntimePlan(planned.runtime_plan, plan_bytes, error_out))
            {
                return false;
            }

            auto *payload = instructionObject(select_inst);
            if (payload == nullptr)
            {
                error_out = "SELECT instruction payload is not an object";
                return false;
            }
            return rewriteSelectPayloadForPlan(*payload, planned.runtime_plan, std::move(plan_bytes), error_out);
        }

        auto buildPlanCacheKey(const std::string &sql,
                               const parser::v3::Statement *stmt,
                               const core::ID &current_schema_id,
                               uint16_t root_opcode,
                               const std::string &payload_hash) -> sblr::v3::PlanCacheKeyInput
        {
            const auto *conn = core::ConnectionContext::getCurrent();
            sblr::v3::PlanCacheKeyInput key;
            key.profile_id = "native";
            key.profile_version = "v3";
            key.payload_format = "SQL_TEXT";
            key.payload_hash = payload_hash;
            key.canonical_opcode_symbol =
                sblr::v3::canonicalOpcodeSymbolForOpcode(root_opcode);
            key.catalog_epoch = compilerCatalogEpoch().load();
            key.security_epoch = compilerSecurityEpoch().load();
            key.capability_set_hash = conn != nullptr ? conn->dialect_tag() : "scratchbird";
            key.module_version = 1;
            key.translation_rule_version = 1;
            key.host_api_abi_version = "sblr-v3";
            key.target_triples_hash = "sblr-only";
            key.artifact_preference = "SBLR_ONLY";
            key.optimization_level = "O2";
            key.normalization_rule_set_id = 0x3901;
            key.object_ref_digest = current_schema_id.toString();

            std::ostringstream session_sig;
            session_sig << "sql=" << hashTextHex(normalizeSql(sql));
            if (conn != nullptr)
            {
                session_sig << "|schema=" << conn->getCurrentSchemaId().toString();
                session_sig << "|dialect=" << conn->dialect_tag();
                session_sig << "|search=";
                for (const auto &entry : conn->search_path())
                {
                    session_sig << entry << ';';
                }
            }
            key.session_option_signature = session_sig.str();

            std::ostringstream role_sig;
            if (conn != nullptr)
            {
                role_sig << conn->getCurrentUserId().toString()
                         << "|super=" << (conn->isSuperuser() ? 1 : 0);
            }
            else
            {
                role_sig << "anonymous";
            }
            key.role_context_signature = role_sig.str();
            (void)stmt;
            return key;
        }

        auto cacheMutationBarrier(const parser::v3::Statement *stmt) -> void
        {
            const parser::v3::SelectStmt *select_stmt = nullptr;
            bool is_explain = false;
            if (isCacheableSelect(stmt, select_stmt, is_explain))
            {
                return;
            }

            planCache().invalidateAll();
            ++compilerCatalogEpoch();
            ++compilerSecurityEpoch();
        }
    } // namespace

    auto finalizeQueryCompilerV3Compilation(core::Database *db,
                                            const std::string &sql,
                                            const parser::v3::Statement *stmt,
                                            const parser::v3::StringPool &pool,
                                            const core::ID &current_schema_id,
                                            bool optimizations_enabled,
                                            sblr::v3::Container &container)
        -> QueryCompilerV3FinalizeResult
    {
        QueryCompilerV3FinalizeResult result;
        if (db == nullptr)
        {
            result.errors.push_back("Database context is required for QueryCompilerV3");
            return result;
        }

        cacheMutationBarrier(stmt);

        std::vector<sblr::v3::Instruction> instructions;
        std::string instruction_error;
        if (!decodeInstructions(container.bytecode_stream, instructions, instruction_error))
        {
            result.errors.push_back(instruction_error);
            return result;
        }

        size_t root_index = std::numeric_limits<size_t>::max();
        for (size_t index = 0; index < instructions.size(); ++index)
        {
            const auto opcode = static_cast<sblr::v3::Opcode>(instructions[index].opcode);
            if (opcode != sblr::v3::Opcode::SBLR3_VERSION &&
                opcode != sblr::v3::Opcode::SBLR3_END)
            {
                root_index = index;
                break;
            }
        }

        if (root_index == std::numeric_limits<size_t>::max())
        {
            result.errors.push_back("V3 container did not contain a root statement");
            return result;
        }

        const parser::v3::SelectStmt *select_stmt = nullptr;
        bool is_explain = false;
        const bool cacheable = isCacheableSelect(stmt, select_stmt, is_explain);
        const std::string normalized_sql = normalizeSql(sql);
        const std::string payload_hash = hashTextHex(normalized_sql);

        if (cacheable && optimizations_enabled)
        {
            const auto key = buildPlanCacheKey(sql,
                                               stmt,
                                               current_schema_id,
                                               instructions[root_index].opcode,
                                               payload_hash);
            auto get_result = planCache().get(key);
            if (get_result.ok && get_result.hit)
            {
                result.success = true;
                result.cache_hit = true;
                result.bytecode = std::move(get_result.value.sblr_payload);
                return result;
            }

            std::string plan_error;
            bool plan_ok = true;
            if (!is_explain)
            {
                plan_ok = applyPlannedSelect(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             instructions[root_index],
                                             result.warnings,
                                             plan_error);
            }
            else
            {
                auto *explain_payload = instructionObject(instructions[root_index]);
                if (explain_payload == nullptr)
                {
                    result.errors.push_back("EXPLAIN instruction payload is not an object");
                    return result;
                }
                auto query_it = explain_payload->find("query");
                if (query_it == explain_payload->end())
                {
                    result.errors.push_back("EXPLAIN payload is missing query");
                    return result;
                }
                auto *query_ptr = std::get_if<sblr::v3::Value::InstrPtr>(&query_it->second.data);
                if (query_ptr == nullptr || !*query_ptr)
                {
                    result.errors.push_back("EXPLAIN query payload is invalid");
                    return result;
                }
                plan_ok = applyPlannedSelect(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             **query_ptr,
                                             result.warnings,
                                             plan_error);
            }

            if (!plan_ok)
            {
                result.errors.push_back(plan_error);
                return result;
            }

            if (!encodeInstructions(instructions, container.bytecode_stream, instruction_error))
            {
                result.errors.push_back(instruction_error);
                return result;
            }

            std::string encode_error;
            if (!sblr::v3::encodeContainer(container, result.bytecode, encode_error))
            {
                result.errors.push_back(encode_error.empty() ?
                    "V3 container encode failed" :
                    encode_error);
                return result;
            }

            optimizer::VNextPlanCacheValue cache_value;
            cache_value.native_feature_key = is_explain
                ? "feature.optimizer.v3_explain_select"
                : "feature.optimizer.v3_select";
            cache_value.normalized_payload_hash = payload_hash;
            cache_value.native_ast_hash =
                hashTextHex(sblr::v3::canonicalOpcodeSymbolForOpcode(instructions[root_index].opcode));
            cache_value.sblr_hash = hashBytesHex(result.bytecode);
            cache_value.sblr_payload = result.bytecode;
            cache_value.compile_module_id = core::generateUuidV7();
            cache_value.native_artifact_status =
                optimizer::NativeArtifactStatus::FALLBACK_SBLR_ONLY;
            cache_value.fallback_reason_code = "SBLR_ONLY";
            (void)planCache().put(key, cache_value);
        }
        else
        {
            std::string encode_error;
            if (!sblr::v3::encodeContainer(container, result.bytecode, encode_error))
            {
                result.errors.push_back(encode_error.empty() ?
                    "V3 container encode failed" :
                    encode_error);
                return result;
            }
        }

        result.success = true;
        return result;
    }

    auto queryCompilerV3PlanCacheStats() -> optimizer::VNextPlanCacheStats
    {
        return planCache().getStats();
    }

    auto resetQueryCompilerV3PlanCacheStats() -> void
    {
        planCache().resetStats();
    }

} // namespace scratchbird::sblr::detail
