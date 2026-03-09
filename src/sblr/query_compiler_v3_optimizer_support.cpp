#include "scratchbird/sblr/query_compiler_v3_optimizer_support.h"

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/query_profiler.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_plan_cache_key.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <unordered_set>

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

        auto trimOptimizerControl(std::string value) -> std::string
        {
            auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            value.erase(value.begin(),
                        std::find_if(value.begin(), value.end(), not_space));
            value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(),
                        value.end());
            return value;
        }

        auto splitOptimizerDirectiveAssignments(const std::string &text)
            -> std::vector<std::string>
        {
            std::vector<std::string> parts;
            std::string current;
            bool in_quote = false;
            for (size_t i = 0; i < text.size(); ++i)
            {
                const char ch = text[i];
                if ((ch == '\'' || ch == '"') &&
                    (i == 0 || text[i - 1] != '\\'))
                {
                    in_quote = !in_quote;
                    current.push_back(ch);
                    continue;
                }
                if (!in_quote && (ch == ';' || ch == ','))
                {
                    const std::string trimmed = trimOptimizerControl(current);
                    if (!trimmed.empty())
                    {
                        parts.push_back(trimmed);
                    }
                    current.clear();
                    continue;
                }
                current.push_back(ch);
            }

            const std::string trimmed = trimOptimizerControl(current);
            if (!trimmed.empty())
            {
                parts.push_back(trimmed);
            }
            return parts;
        }

        auto readOptimizerSessionSetting(const core::ConnectionContext *conn,
                                         std::string &value_out,
                                         std::initializer_list<const char *> names) -> bool
        {
            if (conn == nullptr)
            {
                return false;
            }
            for (const char *name : names)
            {
                if (name != nullptr && conn->getSessionVariable(name, value_out))
                {
                    return true;
                }
            }
            return false;
        }

        auto normalizeOptimizerSignatureValue(const std::string &value) -> std::string
        {
            return core::IdentifierUtils::toUpper(trimOptimizerControl(value));
        }

        auto appendOptimizerSessionSignature(std::ostringstream &session_sig,
                                             const core::ConnectionContext *conn) -> void
        {
            if (conn == nullptr)
            {
                return;
            }

            struct SessionSettingAlias
            {
                const char *canonical_name;
                std::array<const char *, 3> aliases;
            };

            const std::array<SessionSettingAlias, 7> settings{{
                {"WORK_MEM", {"WORK_MEM", "OPTIMIZER.WORK_MEM", "OPTIMIZER_WORK_MEM"}},
                {"SPILL_POLICY", {"OPTIMIZER.SPILL_POLICY", "OPTIMIZER_SPILL_POLICY", "SPILL_POLICY"}},
                {"JOIN_SEARCH", {"OPTIMIZER.JOIN_SEARCH", "OPTIMIZER_JOIN_SEARCH", "JOIN_SEARCH"}},
                {"SEARCH_DEPTH", {"OPTIMIZER.SEARCH_DEPTH", "OPTIMIZER_SEARCH_DEPTH", "SEARCH_DEPTH"}},
                {"JOIN_METHOD", {"OPTIMIZER.JOIN_METHOD", "OPTIMIZER_JOIN_METHOD", "JOIN_METHOD"}},
                {"PLAN_PROFILE", {"OPTIMIZER.PLAN_PROFILE", "OPTIMIZER_PLAN_PROFILE", "PLAN_PROFILE"}},
                {"PLAN_DIRECTIVES", {"OPTIMIZER.PLAN_DIRECTIVES", "OPTIMIZER_PLAN_DIRECTIVES", "PLAN_DIRECTIVES"}},
            }};

            for (const auto &setting : settings)
            {
                std::string value;
                if (readOptimizerSessionSetting(
                        conn,
                        value,
                        {setting.aliases[0], setting.aliases[1], setting.aliases[2]}))
                {
                    session_sig << "|opt." << setting.canonical_name << '='
                                << normalizeOptimizerSignatureValue(value);
                }
            }
        }

        auto upsertRuntimePlanControl(std::vector<optimizer::RuntimePlanControlEntry> &controls,
                                      const std::string &name,
                                      const std::string &value,
                                      const std::string &source) -> void
        {
            for (auto &entry : controls)
            {
                if (entry.name == name)
                {
                    entry.value = value;
                    entry.source = source;
                    entry.enforced = true;
                    return;
                }
            }
            controls.push_back(optimizer::RuntimePlanControlEntry{name, value, source, true});
        }

        auto resolvePlanProfileControl(
            const core::ConnectionContext *conn,
            QueryCompilerV3PlanProfileMode requested_mode,
            const optimizer::ParameterBindings *parameter_bindings,
            QueryCompilerV3PlanProfileMode &effective_mode_out,
            optimizer::RuntimePlanControlEntry &control_out,
            std::vector<std::string> &warnings_out,
            std::string &error_out) -> bool
        {
            effective_mode_out = requested_mode;
            control_out = optimizer::RuntimePlanControlEntry{
                "PLAN_PROFILE",
                requested_mode == QueryCompilerV3PlanProfileMode::CUSTOM ? "CUSTOM"
                                                                        : "GENERIC",
                "DEFAULT",
                true};

            auto applyValue = [&](const std::string &raw_value,
                                  const char *source) -> bool {
                const std::string upper =
                    core::IdentifierUtils::toUpper(trimOptimizerControl(raw_value));
                if (upper.empty() || upper == "AUTO")
                {
                    return true;
                }
                if (upper == "GENERIC")
                {
                    effective_mode_out = QueryCompilerV3PlanProfileMode::GENERIC;
                    control_out.value = "GENERIC";
                    control_out.source = source;
                    return true;
                }
                if (upper == "CUSTOM")
                {
                    if (parameter_bindings == nullptr || parameter_bindings->empty())
                    {
                        effective_mode_out = QueryCompilerV3PlanProfileMode::GENERIC;
                        control_out.value = "GENERIC";
                        control_out.source = source;
                        warnings_out.push_back(
                            "Optimizer control requested CUSTOM plan profile without parameters; using GENERIC");
                        return true;
                    }
                    effective_mode_out = QueryCompilerV3PlanProfileMode::CUSTOM;
                    control_out.value = "CUSTOM";
                    control_out.source = source;
                    return true;
                }
                error_out = "Invalid optimizer plan profile control: " + raw_value;
                return false;
            };

            std::string value;
            if (readOptimizerSessionSetting(conn,
                                            value,
                                            {"OPTIMIZER.PLAN_PROFILE",
                                             "OPTIMIZER_PLAN_PROFILE",
                                             "PLAN_PROFILE"}))
            {
                if (!applyValue(value, "SESSION"))
                {
                    return false;
                }
            }

            if (readOptimizerSessionSetting(conn,
                                            value,
                                            {"OPTIMIZER.PLAN_DIRECTIVES",
                                             "OPTIMIZER_PLAN_DIRECTIVES",
                                             "PLAN_DIRECTIVES"}))
            {
                for (const auto &assignment : splitOptimizerDirectiveAssignments(value))
                {
                    const size_t equals = assignment.find('=');
                    if (equals == std::string::npos || equals == 0 ||
                        equals + 1 >= assignment.size())
                    {
                        error_out = "Malformed optimizer directive: " + assignment;
                        return false;
                    }
                    const std::string key = core::IdentifierUtils::toUpper(
                        trimOptimizerControl(assignment.substr(0, equals)));
                    if (key != "PLAN_PROFILE")
                    {
                        continue;
                    }
                    if (!applyValue(assignment.substr(equals + 1), "DIRECTIVE"))
                    {
                        return false;
                    }
                }
            }

            return true;
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

        auto rowSelectivityBucket(uint64_t base_rows, uint64_t estimated_rows) -> std::string
        {
            if (estimated_rows == 0)
            {
                return "EMPTY";
            }

            const double safe_base = static_cast<double>(
                base_rows == 0 ? std::max<uint64_t>(estimated_rows, 1) : base_rows);
            const double ratio = static_cast<double>(estimated_rows) / safe_base;
            if (ratio <= 0.001) return "TINY";
            if (ratio <= 0.01) return "LOW";
            if (ratio <= 0.10) return "MID";
            if (ratio <= 0.50) return "HIGH";
            return "FULL";
        }

        auto rowMagnitudeBucket(uint64_t estimated_rows) -> std::string
        {
            if (estimated_rows == 0) return "EMPTY";
            if (estimated_rows <= 10) return "TINY";
            if (estimated_rows <= 100) return "LOW";
            if (estimated_rows <= 1000) return "MID";
            if (estimated_rows <= 10000) return "HIGH";
            return "FULL";
        }

        auto buildSelectivityBucketSignature(const optimizer::RuntimePlan &plan) -> std::string
        {
            std::ostringstream signature;
            signature << "rels=";
            for (const auto &relation : plan.relations)
            {
                signature << relation.source_relation_index << ':'
                          << relation.scan_kind << ':'
                          << rowSelectivityBucket(relation.base_rows,
                                                  relation.estimated_rows)
                          << ';';
            }
            signature << "|joins=";
            for (const auto &join : plan.join_steps)
            {
                signature << join.source_join_index << ':'
                          << join.method << ':'
                          << rowMagnitudeBucket(join.estimated_rows)
                          << ';';
            }
            return signature.str();
        }

        auto planProfileSignature(QueryCompilerV3PlanProfileMode mode,
                                  const std::string &bucket_signature) -> std::string
        {
            if (mode == QueryCompilerV3PlanProfileMode::CUSTOM)
            {
                return std::string("CUSTOM:") +
                       (bucket_signature.empty() ? "UNSPECIFIED" : bucket_signature);
            }
            return "GENERIC";
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

            for (size_t relation_index = 0; relation_index < relation_refs.size(); ++relation_index)
            {
                auto *relation_obj =
                    std::get_if<sblr::v3::Value::Object>(&relation_refs[relation_index].data);
                if (relation_obj != nullptr)
                {
                    (*relation_obj)["source_relation_index"] =
                        sblr::v3::Value(static_cast<uint64_t>(relation_index));
                }
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
                                std::string &error_out,
                                QueryCompilerV3PlanProfileMode plan_profile_mode,
                                const optimizer::ParameterBindings *parameter_bindings,
                                const std::string &query_feedback_key,
                                optimizer::RuntimePlan *runtime_plan_out) -> bool
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
                                                       current_schema_id,
                                                       parameter_bindings);
            if (status != core::Status::OK)
            {
                if (status == core::Status::INVALID_ARGUMENT ||
                    status == core::Status::CONFIGURATION_LIMIT_EXCEEDED)
                {
                    error_out = plan_ctx.message.empty()
                        ? "Optimizer control rejected planning request"
                        : plan_ctx.message;
                    return false;
                }
                warnings.push_back(plan_ctx.message.empty() ?
                    "Optimizer planning failed; continuing without runtime plan" :
                    plan_ctx.message);
                return true;
            }

            planned.runtime_plan.cache_mode =
                plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                    ? "CUSTOM"
                    : "GENERIC";
            planned.runtime_plan.query_feedback_key = query_feedback_key;
            planned.runtime_plan.parameter_sensitive =
                plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM &&
                parameter_bindings != nullptr &&
                !parameter_bindings->empty();
            if (planned.runtime_plan.parameter_sensitive)
            {
                planned.runtime_plan.selectivity_bucket_signature =
                    buildSelectivityBucketSignature(planned.runtime_plan);
            }
            planned.runtime_plan.plan_profile_signature = planProfileSignature(
                plan_profile_mode,
                planned.runtime_plan.selectivity_bucket_signature);

            std::vector<uint8_t> plan_bytes;
            if (!optimizer::encodeRuntimePlan(planned.runtime_plan, plan_bytes, error_out))
            {
                return false;
            }
            if (runtime_plan_out != nullptr)
            {
                *runtime_plan_out = planned.runtime_plan;
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
                               const std::string &payload_hash,
                               const std::string &plan_profile_signature)
            -> sblr::v3::PlanCacheKeyInput
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
            key.plan_profile_signature = plan_profile_signature;

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
                appendOptimizerSessionSignature(session_sig, conn);
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

        auto parseUuidText(std::string_view text, core::ID &id_out) -> bool
        {
            core::ID parsed{};
            size_t byte_index = 0;
            bool high_nibble = true;
            uint8_t current_byte = 0;

            auto hex_value = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
                if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
                return -1;
            };

            for (char ch : text)
            {
                if (ch == '-')
                {
                    continue;
                }

                const int value = hex_value(ch);
                if (value < 0 || byte_index >= parsed.bytes.size())
                {
                    return false;
                }

                if (high_nibble)
                {
                    current_byte = static_cast<uint8_t>(value << 4);
                    high_nibble = false;
                }
                else
                {
                    current_byte = static_cast<uint8_t>(current_byte | value);
                    parsed.bytes[byte_index++] = current_byte;
                    high_nibble = true;
                }
            }

            if (!high_nibble || byte_index != parsed.bytes.size())
            {
                return false;
            }

            id_out = parsed;
            return true;
        }

        auto toRuntimeAdaptiveFeedback(
            const optimizer::CardinalityFeedbackSignal &signal)
            -> optimizer::RuntimePlanAdaptiveFeedback
        {
            optimizer::RuntimePlanAdaptiveFeedback feedback;
            feedback.available = signal.available;
            feedback.replan_required = signal.replan_required;
            feedback.stats_refresh_requested = signal.stats_refresh_requested;
            feedback.stats_refresh_applied = signal.stats_refresh_applied;
            feedback.observation_count = signal.observation_count;
            feedback.replan_action_count = signal.replan_action_count;
            feedback.last_estimated_rows = signal.last_estimated_rows;
            feedback.last_actual_rows = signal.last_actual_rows;
            feedback.estimation_error_ratio = signal.estimation_error_ratio;
            feedback.correction_factor = signal.correction_factor;
            feedback.last_plan_hash = signal.last_plan_hash;
            return feedback;
        }

        auto formatAdaptiveFeedbackDetail(
            const optimizer::CardinalityFeedbackSignal &signal) -> std::string
        {
            std::ostringstream detail;
            detail << "observations=" << signal.observation_count
                   << ", estimated_rows=" << signal.last_estimated_rows
                   << ", actual_rows=" << signal.last_actual_rows
                   << ", error_ratio=" << signal.estimation_error_ratio
                   << ", correction_factor=" << signal.correction_factor
                   << ", replan_required=" << (signal.replan_required ? "true" : "false")
                   << ", stats_refresh_applied="
                   << (signal.stats_refresh_applied ? "true" : "false");
            return detail.str();
        }

        auto annotateAdaptiveFeedback(
            optimizer::RuntimePlan &plan,
            const std::optional<optimizer::CardinalityFeedbackSignal> &signal,
            bool cache_bypassed) -> void
        {
            if (!signal.has_value())
            {
                return;
            }

            plan.adaptive_feedback = toRuntimeAdaptiveFeedback(*signal);
            plan.statistics_provenance.push_back(
                optimizer::RuntimePlanStatisticsProvenance{
                    "query",
                    "ADAPTIVE_CARDINALITY_FEEDBACK",
                    formatAdaptiveFeedbackDetail(*signal)});
            if (cache_bypassed)
            {
                plan.rejected_paths.push_back(
                    optimizer::RuntimePlanTraceEntry{
                        "PLAN_CACHE",
                        "query",
                        "CACHE_REUSE",
                        "REJECTED",
                        "adaptive cardinality feedback requested replan",
                        0.0,
                        0.0,
                        plan.root.estimated_rows});
            }
            if (signal->stats_refresh_applied)
            {
                plan.considered_paths.push_back(
                    optimizer::RuntimePlanTraceEntry{
                        "ADAPTIVE_REPLAN",
                        "query",
                        "STATISTICS_REFRESH",
                        "APPLIED",
                        "statistics refreshed and plan rebuilt from cardinality feedback",
                        0.0,
                        0.0,
                        plan.root.estimated_rows});
            }
        }

        auto refreshAdaptiveStatistics(core::Database *db,
                                       const optimizer::RuntimePlan &plan,
                                       std::vector<std::string> &warnings) -> bool
        {
            if (db == nullptr || db->statistics_manager() == nullptr)
            {
                return false;
            }

            std::unordered_set<core::ID, core::IDHash> refreshed_tables;
            bool refreshed_any = false;
            for (const auto &relation : plan.relations)
            {
                core::ID table_id{};
                if (relation.table_id_text.empty() ||
                    !parseUuidText(relation.table_id_text, table_id) ||
                    table_id == core::ID{} ||
                    refreshed_tables.find(table_id) != refreshed_tables.end())
                {
                    continue;
                }

                core::ErrorContext analyze_ctx;
                const auto status =
                    db->statistics_manager()->analyzeTable(table_id, 0.10f, &analyze_ctx);
                if (status == core::Status::OK)
                {
                    refreshed_tables.insert(table_id);
                    refreshed_any = true;
                    continue;
                }

                warnings.push_back(analyze_ctx.message.empty()
                    ? "Adaptive statistics refresh failed"
                    : analyze_ctx.message);
            }

            return refreshed_any;
        }
    } // namespace

    auto finalizeQueryCompilerV3Compilation(core::Database *db,
                                            const std::string &sql,
                                            const parser::v3::Statement *stmt,
                                            const parser::v3::StringPool &pool,
                                            const core::ID &current_schema_id,
                                            bool optimizations_enabled,
                                            sblr::v3::Container &container,
                                            const optimizer::ParameterBindings *parameter_bindings,
                                            QueryCompilerV3PlanProfileMode plan_profile_mode)
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
        const std::string query_feedback_key =
            hashTextHex(optimizer::QueryProfiler::getInstance().fingerprintQuery(sql));
        const auto adaptive_feedback_before =
            optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(
                query_feedback_key);
        const bool adaptive_replan_required =
            adaptive_feedback_before.has_value() &&
            adaptive_feedback_before->replan_required;
        QueryCompilerV3PlanProfileMode effective_plan_profile_mode = plan_profile_mode;
        optimizer::RuntimePlanControlEntry plan_profile_control;
        std::string plan_profile_control_error;
        if (!resolvePlanProfileControl(core::ConnectionContext::getCurrent(),
                                       plan_profile_mode,
                                       parameter_bindings,
                                       effective_plan_profile_mode,
                                       plan_profile_control,
                                       result.warnings,
                                       plan_profile_control_error))
        {
            result.errors.push_back(plan_profile_control_error);
            return result;
        }
        const bool parameter_sensitive_requested =
            cacheable &&
            optimizations_enabled &&
            parameter_bindings != nullptr &&
            !parameter_bindings->empty() &&
            effective_plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM;

        result.plan_profile.mode = parameter_sensitive_requested
            ? QueryCompilerV3PlanProfileMode::CUSTOM
            : QueryCompilerV3PlanProfileMode::GENERIC;
        result.plan_profile.parameter_sensitive = parameter_sensitive_requested;
        result.plan_profile.signature =
            planProfileSignature(result.plan_profile.mode, std::string());

        if (cacheable && optimizations_enabled)
        {
            if (!parameter_sensitive_requested && !adaptive_replan_required)
            {
                const auto key = buildPlanCacheKey(sql,
                                                   stmt,
                                                   current_schema_id,
                                                   instructions[root_index].opcode,
                                                   payload_hash,
                                                   result.plan_profile.signature);
                auto get_result = planCache().get(key);
                if (get_result.ok && get_result.hit)
                {
                    result.success = true;
                    result.cache_hit = true;
                    result.bytecode = std::move(get_result.value.sblr_payload);
                    return result;
                }
            }

            std::string plan_error;
            bool plan_ok = true;
            optimizer::RuntimePlan planned_runtime_plan;
            if (!is_explain)
            {
                plan_ok = applyPlannedSelect(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             instructions[root_index],
                                             result.warnings,
                                             plan_error,
                                             effective_plan_profile_mode,
                                             parameter_bindings,
                                             query_feedback_key,
                                             &planned_runtime_plan);
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
                                             plan_error,
                                             effective_plan_profile_mode,
                                             parameter_bindings,
                                             query_feedback_key,
                                             &planned_runtime_plan);
            }

            if (!plan_ok)
            {
                result.errors.push_back(plan_error);
                return result;
            }

            if (adaptive_replan_required)
            {
                (void)planCache().invalidateByPayloadHash(payload_hash);
                const bool adaptive_stats_refreshed =
                    refreshAdaptiveStatistics(db, planned_runtime_plan, result.warnings);
                (void)optimizer::QueryProfiler::getInstance().acknowledgeCardinalityFeedback(
                    query_feedback_key,
                    adaptive_stats_refreshed);
                if (adaptive_stats_refreshed)
                {
                    plan_error.clear();
                    if (!is_explain)
                    {
                        plan_ok = applyPlannedSelect(db,
                                                     select_stmt,
                                                     pool,
                                                     current_schema_id,
                                                     instructions[root_index],
                                                     result.warnings,
                                                     plan_error,
                                                     effective_plan_profile_mode,
                                                     parameter_bindings,
                                                     query_feedback_key,
                                                     &planned_runtime_plan);
                    }
                    else
                    {
                        auto *explain_payload = instructionObject(instructions[root_index]);
                        if (explain_payload == nullptr)
                        {
                            result.errors.push_back(
                                "EXPLAIN instruction payload is not an object");
                            return result;
                        }
                        auto query_it = explain_payload->find("query");
                        if (query_it == explain_payload->end())
                        {
                            result.errors.push_back("EXPLAIN payload is missing query");
                            return result;
                        }
                        auto *query_ptr =
                            std::get_if<sblr::v3::Value::InstrPtr>(&query_it->second.data);
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
                                                     plan_error,
                                                     effective_plan_profile_mode,
                                                     parameter_bindings,
                                                     query_feedback_key,
                                                     &planned_runtime_plan);
                    }
                    if (!plan_ok)
                    {
                        result.errors.push_back(plan_error);
                        return result;
                    }
                }
            }

            annotateAdaptiveFeedback(
                planned_runtime_plan,
                optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(
                    query_feedback_key),
                adaptive_replan_required);
            upsertRuntimePlanControl(planned_runtime_plan.optimizer_controls,
                                     plan_profile_control.name,
                                     plan_profile_control.value,
                                     plan_profile_control.source);

            {
                std::vector<uint8_t> annotated_plan_bytes;
                if (!optimizer::encodeRuntimePlan(planned_runtime_plan,
                                                  annotated_plan_bytes,
                                                  plan_error))
                {
                    result.errors.push_back(plan_error);
                    return result;
                }

                if (!is_explain)
                {
                    auto *payload = instructionObject(instructions[root_index]);
                    if (payload == nullptr ||
                        !rewriteSelectPayloadForPlan(*payload,
                                                     planned_runtime_plan,
                                                     std::move(annotated_plan_bytes),
                                                     plan_error))
                    {
                        result.errors.push_back(plan_error.empty()
                            ? "SELECT instruction payload is not an object"
                            : plan_error);
                        return result;
                    }
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
                    auto *query_ptr =
                        std::get_if<sblr::v3::Value::InstrPtr>(&query_it->second.data);
                    if (query_ptr == nullptr || !*query_ptr)
                    {
                        result.errors.push_back("EXPLAIN query payload is invalid");
                        return result;
                    }
                    auto *query_payload = instructionObject(**query_ptr);
                    if (query_payload == nullptr ||
                        !rewriteSelectPayloadForPlan(*query_payload,
                                                     planned_runtime_plan,
                                                     std::move(annotated_plan_bytes),
                                                     plan_error))
                    {
                        result.errors.push_back(plan_error.empty()
                            ? "EXPLAIN query payload is invalid"
                            : plan_error);
                        return result;
                    }
                }
            }

            if (!planned_runtime_plan.plan_hash.empty())
            {
                result.plan_profile.runtime_plan_hash = planned_runtime_plan.plan_hash;
            }
            result.plan_profile.parameter_sensitive =
                planned_runtime_plan.parameter_sensitive;
            result.plan_profile.selectivity_bucket_signature =
                planned_runtime_plan.selectivity_bucket_signature;
            if (!planned_runtime_plan.plan_profile_signature.empty())
            {
                result.plan_profile.signature =
                    planned_runtime_plan.plan_profile_signature;
            }
            if (parameter_sensitive_requested && !adaptive_replan_required)
            {
                const auto custom_key = buildPlanCacheKey(sql,
                                                          stmt,
                                                          current_schema_id,
                                                          instructions[root_index].opcode,
                                                          payload_hash,
                                                          result.plan_profile.signature);
                auto get_result = planCache().get(custom_key);
                if (get_result.ok && get_result.hit)
                {
                    result.success = true;
                    result.cache_hit = true;
                    result.bytecode = std::move(get_result.value.sblr_payload);
                    return result;
                }
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
            const auto key = buildPlanCacheKey(sql,
                                               stmt,
                                               current_schema_id,
                                               instructions[root_index].opcode,
                                               payload_hash,
                                               result.plan_profile.signature);
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
