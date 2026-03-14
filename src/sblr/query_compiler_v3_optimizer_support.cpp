#include "scratchbird/sblr/query_compiler_v3_optimizer_support.h"

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/optimizer/index_advisor.h"
#include "scratchbird/optimizer/plan_payload.h"
#include "scratchbird/optimizer/query_profiler.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/vnext_plan_selection.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_plan_cache_key.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
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

            const std::array<SessionSettingAlias, 19> settings{{
                {"WORK_MEM", {"WORK_MEM", "OPTIMIZER.WORK_MEM", "OPTIMIZER_WORK_MEM"}},
                {"SPILL_POLICY", {"OPTIMIZER.SPILL_POLICY", "OPTIMIZER_SPILL_POLICY", "SPILL_POLICY"}},
                {"JOIN_SEARCH", {"OPTIMIZER.JOIN_SEARCH", "OPTIMIZER_JOIN_SEARCH", "JOIN_SEARCH"}},
                {"SEARCH_DEPTH", {"OPTIMIZER.SEARCH_DEPTH", "OPTIMIZER_SEARCH_DEPTH", "SEARCH_DEPTH"}},
                {"JOIN_METHOD", {"OPTIMIZER.JOIN_METHOD", "OPTIMIZER_JOIN_METHOD", "JOIN_METHOD"}},
                {"PLAN_PROFILE", {"OPTIMIZER.PLAN_PROFILE", "OPTIMIZER_PLAN_PROFILE", "PLAN_PROFILE"}},
                {"PLAN_DIRECTIVES", {"OPTIMIZER.PLAN_DIRECTIVES", "OPTIMIZER_PLAN_DIRECTIVES", "PLAN_DIRECTIVES"}},
                {"ENABLE_PARALLEL", {"ENABLE_PARALLEL", "enable_parallel", nullptr}},
                {"ENABLE_PARALLEL_SCAN", {"ENABLE_PARALLEL_SCAN", "enable_parallel_scan", nullptr}},
                {"ENABLE_PARALLEL_HASH", {"ENABLE_PARALLEL_HASH", "enable_parallel_hash", nullptr}},
                {"ENABLE_PARALLEL_AGGREGATE", {"ENABLE_PARALLEL_AGGREGATE", "enable_parallel_aggregate", nullptr}},
                {"ENABLE_PARALLEL_JOIN", {"ENABLE_PARALLEL_JOIN", "enable_parallel_join", nullptr}},
                {"PARALLEL_LEADER_PARTICIPATION", {"PARALLEL_LEADER_PARTICIPATION", "parallel_leader_participation", nullptr}},
                {"MAX_PARALLEL_WORKERS", {"MAX_PARALLEL_WORKERS", "max_parallel_workers", nullptr}},
                {"MAX_PARALLEL_WORKERS_PER_GATHER", {"MAX_PARALLEL_WORKERS_PER_GATHER", "max_parallel_workers_per_gather", nullptr}},
                {"MIN_PARALLEL_ROWS_PER_WORKER", {"MIN_PARALLEL_ROWS_PER_WORKER", "min_parallel_rows_per_worker", nullptr}},
                {"MIN_PARALLEL_TABLE_SCAN_SIZE", {"MIN_PARALLEL_TABLE_SCAN_SIZE", "min_parallel_table_scan_size", nullptr}},
                {"PARALLEL_SETUP_COST", {"PARALLEL_SETUP_COST", "parallel_setup_cost", nullptr}},
                {"PARALLEL_TUPLE_COST", {"PARALLEL_TUPLE_COST", "parallel_tuple_cost", nullptr}},
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

        auto planProfileModeText(QueryCompilerV3PlanProfileMode mode) -> const char *
        {
            switch (mode)
            {
                case QueryCompilerV3PlanProfileMode::GENERIC: return "GENERIC";
                case QueryCompilerV3PlanProfileMode::CUSTOM: return "CUSTOM";
                case QueryCompilerV3PlanProfileMode::AUTO: return "AUTO";
            }
            return "GENERIC";
        }

        auto stableNonZeroHash(std::string_view text) -> uint64_t
        {
            const uint64_t hash = sblr::v3::stableHash64(text);
            return hash == 0 ? 1 : hash;
        }

        auto effectiveSchemaId(const core::ConnectionContext *conn,
                               const core::ID &current_schema_id) -> core::ID
        {
            if (current_schema_id != core::ID{})
            {
                return current_schema_id;
            }
            if (conn != nullptr && conn->getCurrentSchemaId() != core::ID{})
            {
                return conn->getCurrentSchemaId();
            }
            return {};
        }

        auto currentSchemaEpochKey(const core::ConnectionContext *conn,
                                   const core::ID &current_schema_id) -> uint64_t
        {
            const core::ID schema_id = effectiveSchemaId(conn, current_schema_id);
            if (conn != nullptr)
            {
                const core::ID &schema_epoch_uuid = conn->getCurrentSchemaEpochUuid();
                if (schema_epoch_uuid != core::ID{})
                {
                    return stableNonZeroHash(schema_epoch_uuid.toString());
                }
                const core::ID &session_schema_id = conn->getCurrentSchemaId();
                if (session_schema_id != core::ID{})
                {
                    return stableNonZeroHash(session_schema_id.toString());
                }
            }

            if (schema_id != core::ID{})
            {
                return stableNonZeroHash(schema_id.toString());
            }
            return 1;
        }

        auto currentPolicySnapshotId(const core::ConnectionContext *conn) -> std::string
        {
            if (conn == nullptr)
            {
                return "policy:g1:t1";
            }
            return "policy:g" + std::to_string(std::max<uint64_t>(1, conn->policyEpochGlobal())) +
                   ":t" + std::to_string(std::max<uint64_t>(1, conn->policyEpochTable()));
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
                    effective_mode_out = QueryCompilerV3PlanProfileMode::AUTO;
                    control_out.value = "AUTO";
                    control_out.source = source;
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

        auto deriveStatsSnapshotSignature(const optimizer::RuntimePlan &plan) -> std::string
        {
            std::set<uint64_t> snapshot_ids;
            for (const auto &entry : plan.statistics_provenance)
            {
                if (entry.stats_snapshot_id != 0)
                {
                    snapshot_ids.insert(entry.stats_snapshot_id);
                }
            }

            if (snapshot_ids.empty())
            {
                return "NONE";
            }

            std::ostringstream out;
            bool first = true;
            for (uint64_t snapshot_id : snapshot_ids)
            {
                if (!first)
                {
                    out << ';';
                }
                first = false;
                out << snapshot_id;
            }
            return out.str();
        }

        auto collectCostProfiles(const optimizer::RuntimePlanNode &node,
                                 std::set<std::string> &profiles) -> void
        {
            if (!node.formula_profile_id.empty())
            {
                std::ostringstream profile;
                profile << node.formula_profile_id;
                if (node.formula_profile_version != 0)
                {
                    profile << '@' << node.formula_profile_version;
                }
                profiles.insert(profile.str());
            }
            for (const auto &child : node.children)
            {
                collectCostProfiles(child, profiles);
            }
        }

        auto deriveCostProfileId(const optimizer::RuntimePlan &plan) -> std::string
        {
            std::set<std::string> profiles;
            collectCostProfiles(plan.root, profiles);
            if (profiles.empty())
            {
                return "UNSPECIFIED";
            }

            std::ostringstream out;
            bool first = true;
            for (const auto &profile : profiles)
            {
                if (!first)
                {
                    out << '+';
                }
                first = false;
                out << profile;
            }
            return out.str();
        }

        auto countPlanNodes(const optimizer::RuntimePlanNode &node) -> uint32_t
        {
            uint32_t count = 1;
            for (const auto &child : node.children)
            {
                count += countPlanNodes(child);
            }
            return count;
        }

        auto estimatedAvgRowBytes(const optimizer::RuntimePlan &plan) -> uint64_t
        {
            if (plan.root.estimated_rows != 0 && plan.root.estimated_memory_bytes != 0)
            {
                return std::max<uint64_t>(1,
                                          plan.root.estimated_memory_bytes /
                                              plan.root.estimated_rows);
            }

            uint64_t total_rows = 0;
            for (const auto &relation : plan.relations)
            {
                total_rows += relation.estimated_rows;
            }
            if (total_rows != 0 && plan.root.estimated_memory_bytes != 0)
            {
                return std::max<uint64_t>(1, plan.root.estimated_memory_bytes / total_rows);
            }
            return 128;
        }

        auto buildReusablePlanChoiceNode(const optimizer::RuntimePlan &plan)
            -> optimizer::PlanHashNode
        {
            optimizer::PlanHashNode root;
            root.node_symbol = "REUSABLE_PLAN";
            root.attributes.push_back(
                optimizer::PlanHashAttribute::makeString("cache_mode", plan.cache_mode));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "plan_profile_signature", plan.plan_profile_signature));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "root_node_type", plan.root.node_type));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeU64(
                "estimated_rows", plan.root.estimated_rows));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeU64(
                "relation_count", static_cast<uint64_t>(plan.relations.size())));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeU64(
                "join_count", static_cast<uint64_t>(plan.join_steps.size())));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "plan_hash", plan.plan_hash));

            for (const auto &relation : plan.relations)
            {
                optimizer::PlanHashNode relation_node;
                relation_node.node_symbol = "REL";
                relation_node.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                    "alias", relation.alias));
                relation_node.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                    "scan_kind", relation.scan_kind));
                relation_node.attributes.push_back(optimizer::PlanHashAttribute::makeU64(
                    "estimated_rows", relation.estimated_rows));
                root.children.push_back(std::move(relation_node));
            }

            for (const auto &join : plan.join_steps)
            {
                optimizer::PlanHashNode join_node;
                join_node.node_symbol = "JOIN";
                join_node.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                    "method", join.method));
                join_node.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                    "join_type", join.join_type));
                join_node.attributes.push_back(optimizer::PlanHashAttribute::makeU64(
                    "estimated_rows", join.estimated_rows));
                root.children.push_back(std::move(join_node));
            }
            return root;
        }

        auto buildReusablePlanCandidate(const optimizer::RuntimePlan &plan,
                                        uint32_t page_size_bytes)
            -> optimizer::VNextPlanCandidateInput
        {
            optimizer::VNextPlanCandidateInput candidate;
            candidate.track_symbol = optimizer::QueryTrack::RELATIONAL_TRACK;

            uint64_t base_rows = 0;
            double max_selectivity = 0.0;
            for (const auto &relation : plan.relations)
            {
                base_rows += relation.base_rows;
                max_selectivity = std::max(max_selectivity, relation.selectivity);
            }
            if (base_rows == 0)
            {
                base_rows = std::max<uint64_t>(1, plan.root.estimated_rows);
            }

            const double selectivity =
                std::clamp(max_selectivity > 0.0
                               ? max_selectivity
                               : static_cast<double>(std::max<uint64_t>(1, plan.root.estimated_rows)) /
                                     static_cast<double>(std::max<uint64_t>(1, base_rows)),
                           0.0,
                           1.0);
            const uint64_t avg_row_bytes = estimatedAvgRowBytes(plan);
            const uint64_t memory_budget_bytes =
                std::max<uint64_t>(plan.root.memory_budget_bytes, 4ULL * 1024ULL * 1024ULL);
            const uint64_t bridge_shuffle_bytes =
                static_cast<uint64_t>(std::max<uint64_t>(
                    0,
                    plan.root.estimated_rows * avg_row_bytes *
                        std::max<size_t>(1, plan.join_steps.size())));

            candidate.base_cardinality = base_rows;
            candidate.post_filter_selectivity = selectivity;
            candidate.avg_row_bytes = avg_row_bytes;
            candidate.operator_count = countPlanNodes(plan.root);
            candidate.bridge_count = static_cast<uint32_t>(plan.join_steps.size());
            candidate.bridge_shuffle_bytes = bridge_shuffle_bytes;
            candidate.read_amplification_factor = 1.0;
            candidate.latency_budget_ms = 50.0;
            candidate.memory_budget_bytes = memory_budget_bytes;
            candidate.page_size_bytes =
                std::max<uint32_t>(page_size_bytes == 0 ? 16384U : page_size_bytes, 1024U);
            candidate.join_reorder_candidate_count =
                static_cast<uint32_t>(plan.search_summary.considered_state_count);
            candidate.plan_root = buildReusablePlanChoiceNode(plan);
            return candidate;
        }

        auto planProfileSignature(QueryCompilerV3PlanProfileMode mode,
                                  const std::string &bucket_signature) -> std::string
        {
            if (mode == QueryCompilerV3PlanProfileMode::CUSTOM)
            {
                return std::string("CUSTOM:") +
                       (bucket_signature.empty() ? "UNSPECIFIED" : bucket_signature);
            }
            if (mode == QueryCompilerV3PlanProfileMode::AUTO)
            {
                return std::string("AUTO:") +
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
            const optimizer::ParameterBindings *planner_parameter_bindings =
                plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                    ? parameter_bindings
                    : nullptr;
            const auto status = planner.buildSelectPlan(select_stmt,
                                                       pool,
                                                       planned,
                                                       &plan_ctx,
                                                       core::ConnectionContext::getCurrent(),
                                                       current_schema_id,
                                                       planner_parameter_bindings);
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
                               const std::string &plan_profile_signature,
                               const std::string &stats_snapshot_signature,
                               const std::string &cost_profile_id,
                               const std::string &policy_snapshot_id)
            -> sblr::v3::PlanCacheKeyInput
        {
            const auto *conn = core::ConnectionContext::getCurrent();
            const core::ID schema_id = effectiveSchemaId(conn, current_schema_id);
            sblr::v3::PlanCacheKeyInput key;
            key.profile_id = "native";
            key.profile_version = "v3";
            key.payload_format = "SQL_TEXT";
            key.payload_hash = payload_hash;
            key.canonical_opcode_symbol =
                sblr::v3::canonicalOpcodeSymbolForOpcode(root_opcode);
            key.catalog_epoch = currentSchemaEpochKey(conn, current_schema_id);
            key.security_epoch =
                conn != nullptr ? std::max<uint64_t>(1, conn->policyEpochGlobal()) : 1;
            key.capability_set_hash = conn != nullptr ? conn->dialect_tag() : "scratchbird";
            key.module_version = 1;
            key.translation_rule_version = 1;
            key.host_api_abi_version = "sblr-v3";
            key.target_triples_hash = "sblr-only";
            key.artifact_preference = "SBLR_ONLY";
            key.optimization_level = "O2";
            key.normalization_rule_set_id = 0x3901;
            key.object_ref_digest = schema_id.toString();
            key.plan_profile_signature = plan_profile_signature;
            key.statistics_snapshot_signature = stats_snapshot_signature;
            key.cost_profile_id = cost_profile_id;
            key.policy_snapshot_id = policy_snapshot_id;

            std::ostringstream session_sig;
            session_sig << "sql=" << hashTextHex(normalizeSql(sql));
            if (conn != nullptr)
            {
                session_sig << "|schema=" << schema_id.toString();
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

        auto cacheMutationBarrier(uint16_t root_opcode,
                                  const core::ID &current_schema_id) -> void
        {
            const auto *conn = core::ConnectionContext::getCurrent();
            const core::ID schema_id = effectiveSchemaId(conn, current_schema_id);
            const std::string opcode_symbol =
                sblr::v3::canonicalOpcodeSymbolForOpcode(root_opcode);
            if (opcode_symbol.rfind("OP_STMT_DML_", 0) == 0)
            {
                return;
            }
            if (schema_id != core::ID{})
            {
                (void)planCache().invalidateByObjectRefDigest(schema_id.toString());
            }
        }

        struct PlannedCompilationVariant
        {
            QueryCompilerV3PlanProfileMode mode =
                QueryCompilerV3PlanProfileMode::GENERIC;
            std::vector<sblr::v3::Instruction> instructions;
            optimizer::RuntimePlan runtime_plan;
            optimizer::VNextPlanCandidateInput chooser_candidate;
            sblr::v3::PlanCacheKeyInput cache_key;
            std::string stats_snapshot_signature;
            std::string cost_profile_id;
            std::string policy_snapshot_id;
        };

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

        auto indexRecommendationTypeName(
            optimizer::IndexRecommendationType type) -> std::string
        {
            using optimizer::IndexRecommendationType;
            switch (type)
            {
                case IndexRecommendationType::CREATE_BTREE:
                    return "CREATE_BTREE";
                case IndexRecommendationType::CREATE_HASH:
                    return "CREATE_HASH";
                case IndexRecommendationType::CREATE_LSM:
                    return "CREATE_LSM";
                case IndexRecommendationType::CREATE_COMPOSITE:
                    return "CREATE_COMPOSITE";
                case IndexRecommendationType::CREATE_PARTIAL:
                    return "CREATE_PARTIAL";
                case IndexRecommendationType::DROP_UNUSED:
                    return "DROP_UNUSED";
                case IndexRecommendationType::REINDEX:
                    return "REINDEX";
            }
            return "UNKNOWN";
        }

        auto planHasSpillSignal(const optimizer::RuntimePlanNode &node) -> bool
        {
            if (node.spill_expected)
            {
                return true;
            }
            for (const auto &child : node.children)
            {
                if (planHasSpillSignal(child))
                {
                    return true;
                }
            }
            return false;
        }

        auto appendAdvisorSignal(optimizer::RuntimePlan &plan,
                                 std::string signal_name,
                                 std::string severity,
                                 std::string provenance_source,
                                 std::string detail) -> void
        {
            for (const auto &existing : plan.advisor_signals)
            {
                if (existing.signal_name == signal_name &&
                    existing.detail == detail)
                {
                    return;
                }
            }
            plan.advisor_signals.push_back(
                optimizer::RuntimePlanAdvisorSignal{
                    std::move(signal_name),
                    std::move(severity),
                    std::move(provenance_source),
                    std::move(detail)});
        }

        auto annotateAdvisorFeedback(core::Database *db,
                                     const std::string &sql,
                                     std::string_view query_fingerprint,
                                     optimizer::RuntimePlan &plan) -> void
        {
            plan.advisor_signals.clear();
            plan.advisor_recommendations.clear();

            if (db == nullptr || sql.empty())
            {
                return;
            }

            size_t seq_scan_count = 0;
            for (const auto &relation : plan.relations)
            {
                if (relation.scan_kind == "SEQ_SCAN")
                {
                    ++seq_scan_count;
                }
            }
            if (seq_scan_count > 0)
            {
                appendAdvisorSignal(
                    plan,
                    "SEQ_SCAN",
                    seq_scan_count > 1 ? "HIGH" : "MEDIUM",
                    "PLANNER_ACCESS_PATH",
                    "sequential scan selected for " +
                        std::to_string(seq_scan_count) + " relation(s)");
            }

            if (planHasSpillSignal(plan.root))
            {
                appendAdvisorSignal(plan,
                                    "SPILL_RISK",
                                    "HIGH",
                                    "PLANNER_RESOURCE_MODEL",
                                    "spill expected on chosen operator path");
            }

            const auto feedback =
                optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(
                    plan.query_feedback_key);
            if (feedback.has_value() &&
                feedback->available &&
                feedback->estimation_error_ratio > 1.0)
            {
                std::ostringstream detail;
                detail << "estimated_rows=" << feedback->last_estimated_rows
                       << ", actual_rows=" << feedback->last_actual_rows
                       << ", error_ratio=" << feedback->estimation_error_ratio;
                appendAdvisorSignal(plan,
                                    "MIS_ESTIMATE",
                                    feedback->replan_required ? "HIGH" : "MEDIUM",
                                    "ADAPTIVE_CARDINALITY_FEEDBACK",
                                    detail.str());
            }

            optimizer::IndexAdvisor advisor(db);
            std::vector<optimizer::IndexRecommendation> recommendations;
            core::ErrorContext local_ctx;
            if (advisor.suggestIndexesForQuery(sql, &recommendations, &local_ctx) !=
                core::Status::OK)
            {
                return;
            }

            const std::vector<std::string> signal_names = [&]() {
                std::vector<std::string> names;
                names.reserve(plan.advisor_signals.size());
                for (const auto &signal : plan.advisor_signals)
                {
                    names.push_back(signal.signal_name);
                }
                return names;
            }();

            plan.advisor_recommendations.reserve(recommendations.size());
            for (size_t index = 0; index < recommendations.size(); ++index)
            {
                const auto &rec = recommendations[index];
                plan.advisor_recommendations.push_back(
                    optimizer::RuntimePlanAdvisorRecommendation{
                        static_cast<uint32_t>(index + 1),
                        indexRecommendationTypeName(rec.type),
                        rec.table_name,
                        rec.index_name,
                        rec.column_names,
                        rec.create_sql,
                        rec.drop_sql,
                        rec.reason,
                        "INDEX_ADVISOR",
                        std::string(query_fingerprint),
                        signal_names,
                        rec.benefit_score,
                        rec.cost_score,
                        rec.net_benefit,
                        rec.affected_queries,
                        rec.estimated_size_mb,
                        rec.estimated_speedup,
                        rec.priority,
                        rec.confidence});
            }

            if (!plan.advisor_signals.empty() || !plan.advisor_recommendations.empty())
            {
                std::ostringstream detail;
                detail << "signals=" << plan.advisor_signals.size()
                       << ", recommendations="
                       << plan.advisor_recommendations.size();
                plan.statistics_provenance.push_back(
                    optimizer::RuntimePlanStatisticsProvenance{
                        "query",
                        "ADVISOR_FEEDBACK",
                        detail.str()});
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

        auto annotateReusablePlanMetadata(
            optimizer::RuntimePlan &plan,
            const optimizer::RuntimePlanControlEntry &plan_profile_control,
            std::string_view decision_source,
            const std::string &stats_snapshot_signature,
            const std::string &cost_profile_id,
            const std::string &policy_snapshot_id) -> void
        {
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     plan_profile_control.name,
                                     plan_profile_control.value,
                                     plan_profile_control.source);
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_REUSE_DECISION",
                                     plan.cache_mode,
                                     std::string(decision_source));
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_STATS_SNAPSHOT",
                                     stats_snapshot_signature,
                                     "CACHE_KEY");
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_COST_PROFILE",
                                     cost_profile_id,
                                     "CACHE_KEY");
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_POLICY_SNAPSHOT",
                                     policy_snapshot_id,
                                     "CACHE_KEY");
        }

        auto buildPlannedVariant(core::Database *db,
                                 const parser::v3::SelectStmt *select_stmt,
                                 const parser::v3::StringPool &pool,
                                 const core::ID &current_schema_id,
                                 const std::vector<sblr::v3::Instruction> &base_instructions,
                                 size_t root_index,
                                 bool is_explain,
                                 std::vector<std::string> &warnings,
                                 const optimizer::ParameterBindings *parameter_bindings,
                                 const std::string &query_feedback_key,
                                 QueryCompilerV3PlanProfileMode plan_mode,
                                 const std::string &sql,
                                 const parser::v3::Statement *stmt,
                                 const std::string &payload_hash,
                                 const optimizer::RuntimePlanControlEntry &plan_profile_control,
                                 std::string_view decision_source,
                                 PlannedCompilationVariant &variant_out,
                                 std::string &error_out) -> bool
        {
            variant_out = PlannedCompilationVariant{};
            variant_out.mode = plan_mode;
            variant_out.instructions = base_instructions;

            bool plan_ok = true;
            if (!is_explain)
            {
                plan_ok = applyPlannedSelect(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             variant_out.instructions[root_index],
                                             warnings,
                                             error_out,
                                             plan_mode,
                                             plan_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                                                 ? parameter_bindings
                                                 : nullptr,
                                             query_feedback_key,
                                             &variant_out.runtime_plan);
            }
            else
            {
                auto *explain_payload = instructionObject(variant_out.instructions[root_index]);
                if (explain_payload == nullptr)
                {
                    error_out = "EXPLAIN instruction payload is not an object";
                    return false;
                }
                auto query_it = explain_payload->find("query");
                if (query_it == explain_payload->end())
                {
                    error_out = "EXPLAIN payload is missing query";
                    return false;
                }
                auto *query_ptr =
                    std::get_if<sblr::v3::Value::InstrPtr>(&query_it->second.data);
                if (query_ptr == nullptr || !*query_ptr)
                {
                    error_out = "EXPLAIN query payload is invalid";
                    return false;
                }
                plan_ok = applyPlannedSelect(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             **query_ptr,
                                             warnings,
                                             error_out,
                                             plan_mode,
                                             plan_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                                                 ? parameter_bindings
                                                 : nullptr,
                                             query_feedback_key,
                                             &variant_out.runtime_plan);
            }

            if (!plan_ok)
            {
                return false;
            }

            variant_out.stats_snapshot_signature =
                deriveStatsSnapshotSignature(variant_out.runtime_plan);
            variant_out.cost_profile_id =
                deriveCostProfileId(variant_out.runtime_plan);
            variant_out.policy_snapshot_id =
                currentPolicySnapshotId(core::ConnectionContext::getCurrent());

            annotateReusablePlanMetadata(variant_out.runtime_plan,
                                         plan_profile_control,
                                         decision_source,
                                         variant_out.stats_snapshot_signature,
                                         variant_out.cost_profile_id,
                                         variant_out.policy_snapshot_id);
            annotateAdvisorFeedback(
                db,
                sql,
                optimizer::QueryProfiler::getInstance().fingerprintQuery(sql),
                variant_out.runtime_plan);
            variant_out.chooser_candidate =
                buildReusablePlanCandidate(variant_out.runtime_plan,
                                           db != nullptr ? db->page_size() : 16384);
            variant_out.cache_key = buildPlanCacheKey(
                sql,
                stmt,
                current_schema_id,
                variant_out.instructions[root_index].opcode,
                payload_hash,
                variant_out.runtime_plan.plan_profile_signature,
                variant_out.stats_snapshot_signature,
                variant_out.cost_profile_id,
                variant_out.policy_snapshot_id);
            return true;
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

        cacheMutationBarrier(instructions[root_index].opcode, current_schema_id);

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
        const bool has_parameter_bindings =
            parameter_bindings != nullptr && !parameter_bindings->empty();
        const bool chooser_requested =
            cacheable &&
            optimizations_enabled &&
            has_parameter_bindings &&
            effective_plan_profile_mode == QueryCompilerV3PlanProfileMode::AUTO;
        const bool parameter_sensitive_requested =
            cacheable &&
            optimizations_enabled &&
            has_parameter_bindings &&
            effective_plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM;

        result.plan_profile.mode = QueryCompilerV3PlanProfileMode::GENERIC;
        result.plan_profile.parameter_sensitive = false;
        result.plan_profile.signature =
            planProfileSignature(QueryCompilerV3PlanProfileMode::GENERIC, std::string());

        if (cacheable && optimizations_enabled)
        {
            auto fillResultPlanProfile =
                [&](const PlannedCompilationVariant &variant,
                    std::string_view decision_source) -> void {
                    result.plan_profile.mode = variant.mode;
                    result.plan_profile.parameter_sensitive =
                        variant.runtime_plan.parameter_sensitive;
                    result.plan_profile.signature =
                        variant.runtime_plan.plan_profile_signature;
                    result.plan_profile.selectivity_bucket_signature =
                        variant.runtime_plan.selectivity_bucket_signature;
                    result.plan_profile.runtime_plan_hash =
                        variant.runtime_plan.plan_hash;
                    result.plan_profile.decision_source = std::string(decision_source);
                    result.plan_profile.statistics_snapshot_signature =
                        variant.stats_snapshot_signature;
                    result.plan_profile.cost_profile_id = variant.cost_profile_id;
                    result.plan_profile.policy_snapshot_id =
                        variant.policy_snapshot_id;
                };

            auto encodeSelectedVariant = [&](PlannedCompilationVariant &variant) -> bool {
                std::string plan_error;
                std::vector<uint8_t> annotated_plan_bytes;
                if (!optimizer::encodeRuntimePlan(variant.runtime_plan,
                                                  annotated_plan_bytes,
                                                  plan_error))
                {
                    result.errors.push_back(plan_error);
                    return false;
                }

                if (!is_explain)
                {
                    auto *payload = instructionObject(variant.instructions[root_index]);
                    if (payload == nullptr ||
                        !rewriteSelectPayloadForPlan(*payload,
                                                     variant.runtime_plan,
                                                     std::move(annotated_plan_bytes),
                                                     plan_error))
                    {
                        result.errors.push_back(plan_error.empty()
                            ? "SELECT instruction payload is not an object"
                            : plan_error);
                        return false;
                    }
                }
                else
                {
                    auto *explain_payload = instructionObject(variant.instructions[root_index]);
                    if (explain_payload == nullptr)
                    {
                        result.errors.push_back("EXPLAIN instruction payload is not an object");
                        return false;
                    }
                    auto query_it = explain_payload->find("query");
                    if (query_it == explain_payload->end())
                    {
                        result.errors.push_back("EXPLAIN payload is missing query");
                        return false;
                    }
                    auto *query_ptr =
                        std::get_if<sblr::v3::Value::InstrPtr>(&query_it->second.data);
                    if (query_ptr == nullptr || !*query_ptr)
                    {
                        result.errors.push_back("EXPLAIN query payload is invalid");
                        return false;
                    }
                    auto *query_payload = instructionObject(**query_ptr);
                    if (query_payload == nullptr ||
                        !rewriteSelectPayloadForPlan(*query_payload,
                                                     variant.runtime_plan,
                                                     std::move(annotated_plan_bytes),
                                                     plan_error))
                    {
                        result.errors.push_back(plan_error.empty()
                            ? "EXPLAIN query payload is invalid"
                            : plan_error);
                        return false;
                    }
                }

                if (!encodeInstructions(variant.instructions,
                                        container.bytecode_stream,
                                        instruction_error))
                {
                    result.errors.push_back(instruction_error);
                    return false;
                }

                std::string encode_error;
                if (!sblr::v3::encodeContainer(container, result.bytecode, encode_error))
                {
                    result.errors.push_back(encode_error.empty() ?
                        "V3 container encode failed" :
                        encode_error);
                    return false;
                }
                return true;
            };

            auto makeDecisionSource = [&](QueryCompilerV3PlanProfileMode mode) -> std::string {
                if (chooser_requested)
                {
                    return "CHOOSER";
                }
                if (effective_plan_profile_mode == QueryCompilerV3PlanProfileMode::AUTO)
                {
                    return has_parameter_bindings ? "AUTO_FALLBACK" : "AUTO_NO_PARAMETERS";
                }
                return mode == QueryCompilerV3PlanProfileMode::CUSTOM
                    ? "FORCED_CUSTOM"
                    : "FORCED_GENERIC";
            };

            auto buildVariants = [&](std::vector<PlannedCompilationVariant> &variants_out,
                                     size_t &selected_index_out,
                                     std::string &decision_source_out) -> bool {
                variants_out.clear();
                selected_index_out = 0;
                decision_source_out.clear();

                std::string plan_error;
                if (chooser_requested)
                {
                    PlannedCompilationVariant generic_variant;
                    if (!buildPlannedVariant(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             instructions,
                                             root_index,
                                             is_explain,
                                             result.warnings,
                                             parameter_bindings,
                                             query_feedback_key,
                                             QueryCompilerV3PlanProfileMode::GENERIC,
                                             sql,
                                             stmt,
                                             payload_hash,
                                             plan_profile_control,
                                             "CHOOSER",
                                             generic_variant,
                                             plan_error))
                    {
                        result.errors.push_back(plan_error);
                        return false;
                    }
                    variants_out.push_back(std::move(generic_variant));

                    PlannedCompilationVariant custom_variant;
                    if (!buildPlannedVariant(db,
                                             select_stmt,
                                             pool,
                                             current_schema_id,
                                             instructions,
                                             root_index,
                                             is_explain,
                                             result.warnings,
                                             parameter_bindings,
                                             query_feedback_key,
                                             QueryCompilerV3PlanProfileMode::CUSTOM,
                                             sql,
                                             stmt,
                                             payload_hash,
                                             plan_profile_control,
                                             "CHOOSER",
                                             custom_variant,
                                             plan_error))
                    {
                        result.errors.push_back(plan_error);
                        return false;
                    }
                    variants_out.push_back(std::move(custom_variant));

                    auto selection = optimizer::VNextPlanSelection::selectBestPlan(
                        {variants_out[0].chooser_candidate, variants_out[1].chooser_candidate});
                    if (!selection.ok || selection.selected_index >= variants_out.size())
                    {
                        result.errors.push_back(selection.error_message.empty()
                            ? "Reusable-plan chooser failed"
                            : selection.error_message);
                        return false;
                    }
                    selected_index_out = selection.selected_index;
                    decision_source_out = "CHOOSER";
                    return true;
                }

                const QueryCompilerV3PlanProfileMode selected_mode =
                    parameter_sensitive_requested
                        ? QueryCompilerV3PlanProfileMode::CUSTOM
                        : QueryCompilerV3PlanProfileMode::GENERIC;
                PlannedCompilationVariant selected_variant;
                if (!buildPlannedVariant(db,
                                         select_stmt,
                                         pool,
                                         current_schema_id,
                                         instructions,
                                         root_index,
                                         is_explain,
                                         result.warnings,
                                         parameter_bindings,
                                         query_feedback_key,
                                         selected_mode,
                                         sql,
                                         stmt,
                                         payload_hash,
                                         plan_profile_control,
                                         makeDecisionSource(selected_mode),
                                         selected_variant,
                                         plan_error))
                {
                    result.errors.push_back(plan_error);
                    return false;
                }
                variants_out.push_back(std::move(selected_variant));
                selected_index_out = 0;
                decision_source_out = makeDecisionSource(selected_mode);
                return true;
            };

            std::vector<PlannedCompilationVariant> variants;
            size_t selected_variant_index = 0;
            std::string selected_decision_source;
            if (!buildVariants(variants, selected_variant_index, selected_decision_source))
            {
                return result;
            }

            if (adaptive_replan_required)
            {
                (void)planCache().invalidateByPayloadHash(payload_hash);
                const bool adaptive_stats_refreshed =
                    refreshAdaptiveStatistics(db,
                                              variants[selected_variant_index].runtime_plan,
                                              result.warnings);
                (void)optimizer::QueryProfiler::getInstance().acknowledgeCardinalityFeedback(
                    query_feedback_key,
                    adaptive_stats_refreshed);
                if (adaptive_stats_refreshed)
                {
                    if (!buildVariants(variants,
                                       selected_variant_index,
                                       selected_decision_source))
                    {
                        return result;
                    }
                }
            }

            annotateAdaptiveFeedback(
                variants[selected_variant_index].runtime_plan,
                optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(
                    query_feedback_key),
                adaptive_replan_required);
            PlannedCompilationVariant &selected_variant =
                variants[selected_variant_index];
            fillResultPlanProfile(selected_variant, selected_decision_source);

            if (!adaptive_replan_required)
            {
                auto get_result = planCache().get(selected_variant.cache_key);
                if (get_result.ok && get_result.hit)
                {
                    result.success = true;
                    result.cache_hit = true;
                    result.bytecode = std::move(get_result.value.sblr_payload);
                    return result;
                }
            }

            if (!encodeSelectedVariant(selected_variant))
            {
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
            (void)planCache().put(selected_variant.cache_key, cache_value);
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
