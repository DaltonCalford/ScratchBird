#include "scratchbird/sblr/query_compiler_v3_optimizer_support.h"

// Section 36 invariant: this file is a bounded plan-shaping and planner-support
// surface. Named transformations and strategy choices here must not be widened
// into claims of a mature multi-phase optimizer, broad decorrelation, or
// statistics-complete cost-model behavior without stronger proof.

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
#include <cmath>
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
                const std::string family_name =
                    relation.scan_family.empty() ? relation.scan_kind
                                                 : relation.scan_family;
                signature << relation.source_relation_index << ':'
                          << family_name << ':'
                          << rowSelectivityBucket(relation.base_rows,
                                                  relation.estimated_rows)
                          << ':'
                          << rowMagnitudeBucket(relation.candidate_budget)
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

        auto currentRelationFamilyIdentitySignature(
            const optimizer::RuntimePlanRelation &relation) -> std::string
        {
            const std::string family_name =
                relation.scan_family.empty() ? relation.scan_kind
                                             : relation.scan_family;
            std::ostringstream out;
            out << relation.source_relation_index << ':'
                << relation.taxonomy_version << ':'
                << family_name << ':'
                << static_cast<uint32_t>(relation.scan_family_kind) << ':'
                << accessPathExactnessClassName(relation.exactness_class) << ':'
                << accessPathVisibilityEnforcementName(
                       relation.visibility_enforcement)
                << ':'
                << (relation.requires_recheck ? 1 : 0) << ':'
                << accessPathQueryabilityStateName(
                       relation.queryability_state) << ':'
                << relation.path_name;
            return out.str();
        }

        auto currentRelationFamilyStatisticsSignature(
            const optimizer::RuntimePlanRelation &relation) -> std::string
        {
            const std::string family_name =
                relation.scan_family.empty() ? relation.scan_kind
                                             : relation.scan_family;
            std::ostringstream out;
            out << relation.source_relation_index << ':'
                << family_name << ':'
                << relation.family_metrics_version << ':'
                << relation.metrics_confidence_class << ':'
                << accessPathQueryabilityStateName(
                       relation.queryability_state) << ':'
                << relation.formula_profile_id << '@'
                << relation.formula_profile_version << ':'
                << relation.calibration_profile_id;
            return out.str();
        }

        auto deriveIndexFamilySignature(const optimizer::RuntimePlan &plan) -> std::string
        {
            if (plan.relations.empty())
            {
                return "NONE";
            }

            std::ostringstream out;
            bool first = true;
            for (const auto &relation : plan.relations)
            {
                std::vector<std::string> entries =
                    relation.candidate_family_identity_signatures;
                entries.push_back(currentRelationFamilyIdentitySignature(relation));
                std::sort(entries.begin(), entries.end());
                entries.erase(std::unique(entries.begin(), entries.end()),
                              entries.end());

                for (const auto &entry : entries)
                {
                    if (!first)
                    {
                        out << ';';
                    }
                    first = false;
                    out << entry;
                }
            }
            return out.str();
        }

        auto deriveFamilyStatisticsSignature(const optimizer::RuntimePlan &plan)
            -> std::string
        {
            if (plan.relations.empty())
            {
                return "NONE";
            }

            std::ostringstream out;
            bool first = true;
            for (const auto &relation : plan.relations)
            {
                std::vector<std::string> entries =
                    relation.candidate_family_statistics_signatures;
                entries.push_back(
                    currentRelationFamilyStatisticsSignature(relation));
                std::sort(entries.begin(), entries.end());
                entries.erase(std::unique(entries.begin(), entries.end()),
                              entries.end());

                for (const auto &entry : entries)
                {
                    if (!first)
                    {
                        out << ';';
                    }
                    first = false;
                    out << entry;
                }
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
                if (!node.calibration_profile_id.empty())
                {
                    profile << ':' << node.calibration_profile_id;
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
                "chosen_reuse_mode", plan.chosen_reuse_mode));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "join_search_contract_id", plan.join_search_contract_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "join_search_frontier_mode", plan.join_search_frontier_mode));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "join_search_mode_source", plan.join_search_mode_source));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "base_candidate_bundle_contract_id",
                plan.base_candidate_bundle_contract_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "base_candidate_bundle_owner_pass_id",
                plan.base_candidate_bundle_owner_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "base_candidate_bundle_consumer_pass_id",
                plan.base_candidate_bundle_consumer_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeU64(
                "base_candidate_bundle_rejection_count",
                plan.base_candidate_bundle_rejection_count));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "plan_profile_signature", plan.plan_profile_signature));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "normalized_request_digest", plan.normalized_request_digest));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "execution_intent_class", plan.execution_intent_class));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "storage_layer_shape", plan.storage_layer_shape));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "publication_state_summary", plan.publication_state_summary));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "collector_specialization_id",
                plan.collector_specialization_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "continuation_token_contract",
                plan.continuation_token_contract));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "rewrite_before_search_contract_id",
                plan.rewrite_before_search_contract_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "rewrite_before_search_owner_pass_id",
                plan.rewrite_before_search_owner_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "rewrite_before_search_terminal_pass_id",
                plan.rewrite_before_search_terminal_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeBool(
                "rewrite_before_search_frozen",
                plan.rewrite_before_search_frozen));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "tagging_contract_id",
                plan.tagging_contract_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "tagging_owner_pass_id",
                plan.tagging_owner_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "join_search_owner_pass_id",
                plan.join_search_owner_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "result_shape_finalize_pass_id",
                plan.result_shape_finalize_pass_id));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "index_family_signature", plan.index_family_signature));
            root.attributes.push_back(optimizer::PlanHashAttribute::makeString(
                "family_statistics_signature",
                plan.family_statistics_signature));
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
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "scan_family",
                        relation.scan_family.empty() ? relation.scan_kind
                                                     : relation.scan_family));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeU64(
                        "family_metrics_version",
                        relation.family_metrics_version));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "pruning_granularity_class",
                        relation.pruning_granularity_class));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "projection_layout_id",
                        relation.projection_layout_id));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "storage_layer_shape",
                        relation.storage_layer_shape));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "collector_specialization_id",
                        relation.collector_specialization_id));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "clustered_lookup_shape",
                        relation.clustered_lookup_shape));
                relation_node.attributes.push_back(
                    optimizer::PlanHashAttribute::makeString(
                        "parallel_property_signature",
                        relation.parallel_property_signature));
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
                payload["plan_text"] = sblr::v3::Value(runtime_plan.explain_text);
                payload["plan_hash"] = sblr::v3::Value(runtime_plan.plan_hash);
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
            payload["plan_text"] = sblr::v3::Value(runtime_plan.explain_text);
            payload["plan_hash"] = sblr::v3::Value(runtime_plan.plan_hash);
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
            core::ErrorContext plan_ctx;
            const optimizer::ParameterBindings *planner_parameter_bindings =
                plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                    ? parameter_bindings
                    : nullptr;
            const auto *conn = core::ConnectionContext::getCurrent();
            optimizer::StatementPlanRequest plan_request;
            plan_request.statement_kind =
                optimizer::PlannerStatementKind::SELECT;
            plan_request.normalized_statement_id =
                query_feedback_key.empty()
                    ? std::string("SBLR_SELECT")
                    : query_feedback_key;
            plan_request.normalized_statement_payload = select_stmt;
            plan_request.string_pool = &pool;
            plan_request.string_pool_snapshot_id = "parser_v3/live";
            plan_request.parameter_bindings = planner_parameter_bindings;
            plan_request.current_schema_id = current_schema_id;
            plan_request.security_snapshot_id =
                conn != nullptr
                    ? std::to_string(conn->policyEpochGlobal())
                    : std::string();
            plan_request.planner_policy_snapshot_id =
                conn != nullptr
                    ? conn->dialect_tag()
                    : std::string();
            plan_request.cache_mode =
                plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                    ? "CUSTOM"
                    : "GENERIC";
            plan_request.reuse_mode =
                plan_profile_mode == QueryCompilerV3PlanProfileMode::CUSTOM
                    ? "PARAMETER_SENSITIVE"
                    : "GENERIC_REUSE";
            plan_request.collector_specialization_request = "SBLR_SELECT";
            plan_request.execution_intent_class = "EXECUTE";

            optimizer::StatementPlanResult plan_result;
            const auto status = planner.planStatement(plan_request,
                                                      plan_result,
                                                      &plan_ctx,
                                                      const_cast<core::ConnectionContext *>(
                                                          conn));
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

            optimizer::PlannedSelectQuery planned =
                std::move(plan_result.planned_select);
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
                               const std::string &index_family_signature,
                               const std::string &family_statistics_signature,
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
            key.taxonomy_contract_id = optimizer::kRuntimePlanContractId;
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
            key.index_family_signature = index_family_signature;
            key.family_statistics_signature = family_statistics_signature;
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
            feedback.replan_suppressed = signal.replan_suppressed;
            feedback.stats_refresh_requested = signal.stats_refresh_requested;
            feedback.stats_refresh_applied = signal.stats_refresh_applied;
            feedback.calibration_bundle_proposed =
                signal.calibration_bundle_proposed;
            feedback.observation_count = signal.observation_count;
            feedback.replan_action_count = signal.replan_action_count;
            feedback.last_estimated_rows = signal.last_estimated_rows;
            feedback.last_actual_rows = signal.last_actual_rows;
            feedback.estimation_error_ratio = signal.estimation_error_ratio;
            feedback.correction_factor = signal.correction_factor;
            feedback.cost_reweight_factor = signal.cost_reweight_factor;
            feedback.calibration_profile_version =
                signal.calibration_profile_version;
            feedback.last_plan_hash = signal.last_plan_hash;
            feedback.calibration_profile_id = signal.calibration_profile_id;
            feedback.calibration_profile_delta_id =
                signal.calibration_profile_delta_id;
            feedback.calibration_evidence_id = signal.calibration_evidence_id;
            feedback.guardrail_reason = signal.guardrail_reason;
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
                   << ", cost_reweight_factor=" << signal.cost_reweight_factor
                   << ", calibration_bundle_proposed="
                   << (signal.calibration_bundle_proposed ? "true" : "false")
                   << ", calibration_profile_id=" << signal.calibration_profile_id
                   << ", calibration_profile_delta_id="
                   << signal.calibration_profile_delta_id
                   << ", calibration_evidence_id="
                   << signal.calibration_evidence_id
                   << ", replan_required=" << (signal.replan_required ? "true" : "false")
                   << ", replan_suppressed="
                   << (signal.replan_suppressed ? "true" : "false")
                   << ", guardrail_reason=" << signal.guardrail_reason
                   << ", stats_refresh_applied="
                   << (signal.stats_refresh_applied ? "true" : "false");
            return detail.str();
        }

        auto scaleEstimatedRows(uint64_t current_rows,
                                double correction_factor) -> uint64_t
        {
            if (!std::isfinite(correction_factor) || correction_factor <= 0.0)
            {
                return current_rows;
            }

            const double scaled =
                std::round(static_cast<double>(current_rows) * correction_factor);
            if (scaled <= 0.0)
            {
                return 0;
            }
            if (scaled >= static_cast<double>(std::numeric_limits<uint64_t>::max()))
            {
                return std::numeric_limits<uint64_t>::max();
            }
            return static_cast<uint64_t>(scaled);
        }

        auto clearAdaptiveFeedbackAnnotations(optimizer::RuntimePlan &plan) -> void
        {
            auto is_adaptive_source =
                [](const optimizer::RuntimePlanStatisticsProvenance &entry) {
                    return entry.source == "ADAPTIVE_CARDINALITY_FEEDBACK" ||
                           entry.source == "ADAPTIVE_CARDINALITY_CORRECTION" ||
                           entry.source == "ADAPTIVE_COST_CALIBRATION";
                };
            plan.statistics_provenance.erase(
                std::remove_if(plan.statistics_provenance.begin(),
                               plan.statistics_provenance.end(),
                               is_adaptive_source),
                plan.statistics_provenance.end());

            auto is_adaptive_phase =
                [](const optimizer::RuntimePlanTraceEntry &entry) {
                    return entry.phase == "PLAN_CACHE" ||
                           entry.phase == "ADAPTIVE_REPLAN";
                };
            plan.considered_paths.erase(
                std::remove_if(plan.considered_paths.begin(),
                               plan.considered_paths.end(),
                               is_adaptive_phase),
                plan.considered_paths.end());
            plan.rejected_paths.erase(
                std::remove_if(plan.rejected_paths.begin(),
                               plan.rejected_paths.end(),
                               is_adaptive_phase),
                plan.rejected_paths.end());
            plan.optimizer_controls.erase(
                std::remove_if(
                    plan.optimizer_controls.begin(),
                    plan.optimizer_controls.end(),
                    [](const optimizer::RuntimePlanControlEntry &entry) {
                        return entry.name == "ADAPTIVE_CALIBRATION_PROFILE" ||
                               entry.name == "ADAPTIVE_CALIBRATION_DELTA" ||
                               entry.name == "ADAPTIVE_CALIBRATION_EVIDENCE";
                    }),
                plan.optimizer_controls.end());
        }

        auto applyAdaptiveEstimateCorrection(
            optimizer::RuntimePlan &plan,
            const optimizer::CardinalityFeedbackSignal &signal,
            bool record_provenance = true) -> bool
        {
            if (!signal.available || signal.estimation_error_ratio <= 1.0 ||
                signal.last_actual_rows == 0)
            {
                return false;
            }

            const double correction_factor =
                static_cast<double>(signal.last_actual_rows) /
                static_cast<double>(std::max<uint64_t>(1, plan.root.estimated_rows));
            if (!std::isfinite(correction_factor) || correction_factor <= 0.0 ||
                std::abs(correction_factor - 1.0) < 0.0001)
            {
                return false;
            }

            const auto original_root_rows = plan.root.estimated_rows;
            for (auto &relation : plan.relations)
            {
                relation.estimated_rows =
                    scaleEstimatedRows(relation.estimated_rows,
                                       correction_factor);
            }
            for (auto &join : plan.join_steps)
            {
                join.estimated_rows =
                    scaleEstimatedRows(join.estimated_rows,
                                       correction_factor);
            }
            auto apply_to_node =
                [&](auto &&self, optimizer::RuntimePlanNode &node) -> void {
                    node.estimated_rows =
                        scaleEstimatedRows(node.estimated_rows,
                                           correction_factor);
                    for (auto &child : node.children)
                    {
                        self(self, child);
                    }
                };
            apply_to_node(apply_to_node, plan.root);
            for (auto &entry : plan.considered_paths)
            {
                entry.estimated_rows =
                    scaleEstimatedRows(entry.estimated_rows,
                                       correction_factor);
            }
            for (auto &entry : plan.rejected_paths)
            {
                entry.estimated_rows =
                    scaleEstimatedRows(entry.estimated_rows,
                                       correction_factor);
            }

            const bool changed = plan.root.estimated_rows != original_root_rows;
            if (changed && record_provenance)
            {
                std::ostringstream detail;
                detail << "correction_factor=" << correction_factor
                       << ", root_estimated_rows=" << original_root_rows
                       << "->" << plan.root.estimated_rows;
                plan.statistics_provenance.push_back(
                    optimizer::RuntimePlanStatisticsProvenance{
                        "query",
                        "ADAPTIVE_CARDINALITY_CORRECTION",
                        detail.str()});
            }
            return changed;
        }

        auto applyAdaptiveCostCalibration(
            optimizer::RuntimePlan &plan,
            const optimizer::CardinalityFeedbackSignal &signal,
            bool record_provenance = true) -> bool
        {
            if (!signal.available ||
                !signal.calibration_bundle_proposed ||
                signal.calibration_profile_id.empty())
            {
                return false;
            }

            const double factor =
                std::clamp(signal.cost_reweight_factor, 0.5, 2.0);
            const bool costs_change = std::abs(factor - 1.0) >= 0.0001;
            bool changed = false;

            auto upsert_input =
                [](std::vector<optimizer::RuntimePlanCostInputEstimate> &inputs,
                   const std::string &name,
                   double value,
                   const std::string &unit) {
                    for (auto &entry : inputs)
                    {
                        if (entry.name == name)
                        {
                            entry.value = value;
                            entry.unit = unit;
                            return;
                        }
                    }
                    inputs.push_back(
                        optimizer::RuntimePlanCostInputEstimate{name, value, unit});
                };

            auto upsert_term =
                [](std::vector<optimizer::RuntimePlanCostTerm> &terms,
                   const std::string &name,
                   double coefficient,
                   double input_value,
                   double contribution,
                   const std::string &unit) {
                    for (auto &entry : terms)
                    {
                        if (entry.name == name)
                        {
                            entry.coefficient = coefficient;
                            entry.input_value = input_value;
                            entry.contribution = contribution;
                            entry.unit = unit;
                            return;
                        }
                    }
                    terms.push_back(optimizer::RuntimePlanCostTerm{
                        name,
                        coefficient,
                        input_value,
                        contribution,
                        unit});
                };

            auto apply_profile =
                [&](double &startup_cost,
                    double &total_cost,
                    std::string &calibration_profile_id) -> double {
                    const double previous_total = total_cost;
                    const double previous_startup = startup_cost;
                    if (costs_change)
                    {
                        startup_cost *= factor;
                        total_cost *= factor;
                    }
                    if (calibration_profile_id != signal.calibration_profile_id)
                    {
                        calibration_profile_id = signal.calibration_profile_id;
                        changed = true;
                    }
                    if (costs_change &&
                        (std::abs(total_cost - previous_total) >= 0.0001 ||
                         std::abs(startup_cost - previous_startup) >= 0.0001))
                    {
                        changed = true;
                    }
                    return previous_total;
                };

            auto apply_to_node =
                [&](auto &&self, optimizer::RuntimePlanNode &node) -> void {
                    const double previous_total =
                        apply_profile(node.startup_cost,
                                      node.total_cost,
                                      node.calibration_profile_id);
                    upsert_input(node.input_estimates,
                                 "adaptive_cost_reweight_factor",
                                 factor,
                                 "ratio");
                    upsert_input(node.input_estimates,
                                 "adaptive_feedback_error_ratio",
                                 signal.estimation_error_ratio,
                                 "ratio");
                    upsert_term(node.expanded_cost_terms,
                                "adaptive_feedback_multiplier",
                                factor,
                                previous_total,
                                node.total_cost - previous_total,
                                "cost");
                    for (auto &child : node.children)
                    {
                        self(self, child);
                    }
                };

            apply_to_node(apply_to_node, plan.root);
            for (auto &relation : plan.relations)
            {
                apply_profile(relation.startup_cost,
                              relation.total_cost,
                              relation.calibration_profile_id);
            }
            std::string join_calibration_profile_id =
                !plan.root.calibration_profile_id.empty()
                    ? plan.root.calibration_profile_id
                    : plan.adaptive_feedback.calibration_profile_id;
            for (auto &join : plan.join_steps)
            {
                apply_profile(join.startup_cost,
                              join.total_cost,
                              join_calibration_profile_id);
            }
            for (auto &entry : plan.considered_paths)
            {
                if (costs_change)
                {
                    entry.startup_cost *= factor;
                    entry.total_cost *= factor;
                    changed = true;
                }
            }
            for (auto &entry : plan.rejected_paths)
            {
                if (costs_change)
                {
                    entry.startup_cost *= factor;
                    entry.total_cost *= factor;
                    changed = true;
                }
            }

            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "ADAPTIVE_CALIBRATION_PROFILE",
                                     signal.calibration_profile_id,
                                     "RUNTIME_FEEDBACK");
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "ADAPTIVE_CALIBRATION_DELTA",
                                     signal.calibration_profile_delta_id,
                                     "RUNTIME_FEEDBACK");
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "ADAPTIVE_CALIBRATION_EVIDENCE",
                                     signal.calibration_evidence_id,
                                     "RUNTIME_FEEDBACK");

            if (changed && record_provenance)
            {
                std::ostringstream detail;
                detail << "profile=" << signal.calibration_profile_id
                       << ", delta=" << signal.calibration_profile_delta_id
                       << ", evidence=" << signal.calibration_evidence_id
                       << ", factor=" << factor;
                plan.statistics_provenance.push_back(
                    optimizer::RuntimePlanStatisticsProvenance{
                        "query",
                        "ADAPTIVE_COST_CALIBRATION",
                        detail.str()});
                plan.considered_paths.push_back(
                    optimizer::RuntimePlanTraceEntry{
                        "ADAPTIVE_REPLAN",
                        "query",
                        "COST_CALIBRATION_BUNDLE",
                        "APPLIED",
                        detail.str(),
                        plan.root.startup_cost,
                        plan.root.total_cost,
                        plan.root.estimated_rows});
            }

            return changed;
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

            const bool correction_already_applied =
                plan.adaptive_feedback.correction_applied;
            const bool calibration_already_applied =
                plan.adaptive_feedback.calibration_applied;
            plan.adaptive_feedback = toRuntimeAdaptiveFeedback(*signal);
            plan.adaptive_feedback.correction_applied =
                correction_already_applied
                    ? true
                    : applyAdaptiveEstimateCorrection(plan, *signal);
            plan.adaptive_feedback.calibration_applied =
                calibration_already_applied
                    ? true
                    : applyAdaptiveCostCalibration(plan, *signal);
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
            if (signal->replan_suppressed)
            {
                plan.rejected_paths.push_back(
                    optimizer::RuntimePlanTraceEntry{
                        "ADAPTIVE_REPLAN",
                        "query",
                        "REPLAN_GUARDRAIL",
                        "REJECTED",
                        signal->guardrail_reason.empty()
                            ? "adaptive replan suppressed by guardrail"
                            : signal->guardrail_reason,
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

        auto refreshCachedAdaptiveFeedbackPayload(
            const std::vector<uint8_t> &bytecode,
            const std::optional<optimizer::CardinalityFeedbackSignal> &signal,
            std::vector<uint8_t> &bytecode_out,
            std::string &error_out) -> bool
        {
            bytecode_out = bytecode;
            if (!signal.has_value())
            {
                return true;
            }

            sblr::v3::Container container;
            if (!sblr::v3::decodeContainer(bytecode.data(),
                                           bytecode.size(),
                                           container,
                                           error_out))
            {
                return false;
            }

            std::vector<sblr::v3::Instruction> instructions;
            if (!decodeInstructions(container.bytecode_stream, instructions, error_out))
            {
                return false;
            }

            size_t root_index = std::numeric_limits<size_t>::max();
            for (size_t index = 0; index < instructions.size(); ++index)
            {
                const auto opcode =
                    static_cast<sblr::v3::Opcode>(instructions[index].opcode);
                if (opcode != sblr::v3::Opcode::SBLR3_VERSION &&
                    opcode != sblr::v3::Opcode::SBLR3_END)
                {
                    root_index = index;
                    break;
                }
            }
            if (root_index == std::numeric_limits<size_t>::max())
            {
                error_out = "Cached V3 container did not contain a root statement";
                return false;
            }

            auto patch_payload_plan =
                [&](sblr::v3::Value::Object &payload) -> bool {
                    auto plan_it = payload.find("plan");
                    if (plan_it == payload.end())
                    {
                        error_out = "Cached SELECT payload is missing runtime plan";
                        return false;
                    }

                    auto *plan_bytes =
                        std::get_if<sblr::v3::Value::Bytes>(&plan_it->second.data);
                    if (plan_bytes == nullptr)
                    {
                        error_out = "Cached SELECT runtime plan payload is invalid";
                        return false;
                    }

                    optimizer::RuntimePlan runtime_plan;
                    if (!optimizer::decodeRuntimePlan(*plan_bytes,
                                                     runtime_plan,
                                                     error_out))
                    {
                        return false;
                    }

                    clearAdaptiveFeedbackAnnotations(runtime_plan);
                    annotateAdaptiveFeedback(runtime_plan, signal, false);

                    std::vector<uint8_t> refreshed_plan_bytes;
                    if (!optimizer::encodeRuntimePlan(runtime_plan,
                                                      refreshed_plan_bytes,
                                                      error_out))
                    {
                        return false;
                    }
                    plan_it->second = sblr::v3::Value(std::move(refreshed_plan_bytes));
                    return true;
                };

            auto *payload = instructionObject(instructions[root_index]);
            if (payload == nullptr)
            {
                error_out = "Cached root instruction payload is not an object";
                return false;
            }

            const auto root_opcode =
                static_cast<sblr::v3::Opcode>(instructions[root_index].opcode);
            if (root_opcode == sblr::v3::Opcode::SBLR3_SELECT)
            {
                if (!patch_payload_plan(*payload))
                {
                    return false;
                }
            }
            else if (root_opcode == sblr::v3::Opcode::SBLR3_EXPLAIN_PLAN)
            {
                auto query_it = payload->find("query");
                if (query_it == payload->end())
                {
                    error_out = "Cached EXPLAIN payload is missing query";
                    return false;
                }
                auto *query_ptr =
                    std::get_if<sblr::v3::Value::InstrPtr>(&query_it->second.data);
                if (query_ptr == nullptr || !*query_ptr)
                {
                    error_out = "Cached EXPLAIN query payload is invalid";
                    return false;
                }
                auto *query_payload = instructionObject(**query_ptr);
                if (query_payload == nullptr || !patch_payload_plan(*query_payload))
                {
                    if (error_out.empty())
                    {
                        error_out = "Cached EXPLAIN query payload is invalid";
                    }
                    return false;
                }
                auto refreshed_plan_it = query_payload->find("plan");
                if (refreshed_plan_it == query_payload->end())
                {
                    error_out = "Cached EXPLAIN query payload is missing runtime plan";
                    return false;
                }
                (*payload)["plan"] = refreshed_plan_it->second;
                auto refreshed_plan_text_it = query_payload->find("plan_text");
                if (refreshed_plan_text_it != query_payload->end())
                {
                    (*payload)["plan_text"] = refreshed_plan_text_it->second;
                }
                auto refreshed_plan_hash_it = query_payload->find("plan_hash");
                if (refreshed_plan_hash_it != query_payload->end())
                {
                    (*payload)["plan_hash"] = refreshed_plan_hash_it->second;
                }
            }

            if (!encodeInstructions(instructions,
                                    container.bytecode_stream,
                                    error_out))
            {
                return false;
            }
            return sblr::v3::encodeContainer(container, bytecode_out, error_out);
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
                optimizer::RuntimePlanAdvisorRecommendation runtime_rec{
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
                    rec.confidence};
                runtime_rec.what_if_replanned = rec.what_if.replanned;
                runtime_rec.baseline_access_family =
                    rec.what_if.baseline_access_family;
                runtime_rec.baseline_index_name = rec.what_if.baseline_index_name;
                runtime_rec.baseline_total_cost = rec.what_if.baseline_total_cost;
                runtime_rec.baseline_estimated_rows =
                    rec.what_if.baseline_estimated_rows;
                runtime_rec.hypothetical_access_family =
                    rec.what_if.hypothetical_access_family;
                runtime_rec.hypothetical_index_name =
                    rec.what_if.hypothetical_index_name;
                runtime_rec.hypothetical_total_cost =
                    rec.what_if.hypothetical_total_cost;
                runtime_rec.hypothetical_estimated_rows =
                    rec.what_if.hypothetical_estimated_rows;
                runtime_rec.estimated_cost_delta =
                    rec.what_if.estimated_cost_delta;
                runtime_rec.estimated_speedup_ratio =
                    rec.what_if.estimated_speedup_ratio;
                runtime_rec.ordering_improved =
                    rec.what_if.ordering_improved;
                runtime_rec.covering_improved =
                    rec.what_if.covering_improved;
                runtime_rec.evidence_detail = rec.what_if.evidence_detail;
                plan.advisor_recommendations.push_back(std::move(runtime_rec));
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
            const std::string &index_family_signature,
            const std::string &family_statistics_signature,
            const std::string &stats_snapshot_signature,
            const std::string &cost_profile_id,
            const std::string &policy_snapshot_id) -> void
        {
            plan.index_family_signature = index_family_signature;
            plan.family_statistics_signature = family_statistics_signature;
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     plan_profile_control.name,
                                     plan_profile_control.value,
                                     plan_profile_control.source);
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_REUSE_DECISION",
                                     plan.cache_mode,
                                     std::string(decision_source));
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_FAMILY_IDENTITY",
                                     index_family_signature,
                                     "CACHE_KEY");
            upsertRuntimePlanControl(plan.optimizer_controls,
                                     "PLAN_FAMILY_STATS",
                                     family_statistics_signature,
                                     "CACHE_KEY");
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

            const auto adaptive_feedback =
                optimizer::QueryProfiler::getInstance().latestCardinalityFeedback(
                    query_feedback_key);
            if (adaptive_feedback.has_value())
            {
                variant_out.runtime_plan.adaptive_feedback =
                    toRuntimeAdaptiveFeedback(*adaptive_feedback);
                variant_out.runtime_plan.adaptive_feedback.correction_applied =
                    applyAdaptiveEstimateCorrection(variant_out.runtime_plan,
                                                   *adaptive_feedback,
                                                   false);
                variant_out.runtime_plan.adaptive_feedback.calibration_applied =
                    applyAdaptiveCostCalibration(variant_out.runtime_plan,
                                                *adaptive_feedback,
                                                false);
            }

            variant_out.stats_snapshot_signature =
                deriveStatsSnapshotSignature(variant_out.runtime_plan);
            variant_out.cost_profile_id =
                deriveCostProfileId(variant_out.runtime_plan);
            variant_out.policy_snapshot_id =
                currentPolicySnapshotId(core::ConnectionContext::getCurrent());
            variant_out.runtime_plan.index_family_signature =
                deriveIndexFamilySignature(variant_out.runtime_plan);
            variant_out.runtime_plan.family_statistics_signature =
                deriveFamilyStatisticsSignature(variant_out.runtime_plan);

            annotateReusablePlanMetadata(variant_out.runtime_plan,
                                         plan_profile_control,
                                         decision_source,
                                         variant_out.runtime_plan.index_family_signature,
                                         variant_out.runtime_plan
                                             .family_statistics_signature,
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
                variant_out.runtime_plan.index_family_signature,
                variant_out.runtime_plan.family_statistics_signature,
                variant_out.stats_snapshot_signature,
                variant_out.cost_profile_id,
                variant_out.policy_snapshot_id);
            if (is_explain)
            {
                auto *explain_payload = instructionObject(variant_out.instructions[root_index]);
                if (explain_payload == nullptr)
                {
                    error_out = "EXPLAIN instruction payload is not an object";
                    return false;
                }
                std::vector<uint8_t> explain_plan_bytes;
                if (!optimizer::encodeRuntimePlan(variant_out.runtime_plan,
                                                  explain_plan_bytes,
                                                  error_out))
                {
                    return false;
                }
                (*explain_payload)["plan"] =
                    sblr::v3::Value(std::move(explain_plan_bytes));
                (*explain_payload)["plan_text"] =
                    sblr::v3::Value(variant_out.runtime_plan.explain_text);
                (*explain_payload)["plan_hash"] =
                    sblr::v3::Value(variant_out.runtime_plan.plan_hash);
            }
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
        const auto root_opcode =
            static_cast<sblr::v3::Opcode>(instructions[root_index].opcode);
        const std::string root_opcode_symbol =
            sblr::v3::canonicalOpcodeSymbolForOpcode(instructions[root_index].opcode);
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

        if ((root_opcode == sblr::v3::Opcode::SBLR3_SELECT ||
             root_opcode == sblr::v3::Opcode::SBLR3_EXPLAIN_PLAN) &&
            (!cacheable || !optimizations_enabled))
        {
            std::ostringstream warning;
            warning << "planner finalizer bypassed root_opcode=" << root_opcode_symbol
                    << " stmt_kind=";
            if (stmt != nullptr)
            {
                warning << static_cast<int>(stmt->kind());
            }
            else
            {
                warning << "null";
            }
            warning << " cacheable=" << (cacheable ? "true" : "false")
                    << " optimizations_enabled="
                    << (optimizations_enabled ? "true" : "false")
                    << " is_explain=" << (is_explain ? "true" : "false");
            result.warnings.push_back(warning.str());
        }

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
                    result.plan_profile.index_family_signature =
                        variant.runtime_plan.index_family_signature;
                    result.plan_profile.family_statistics_signature =
                        variant.runtime_plan.family_statistics_signature;
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
                    auto plan_it = query_payload->find("plan");
                    if (plan_it == query_payload->end())
                    {
                        result.errors.push_back(
                            "EXPLAIN query payload is missing rewritten runtime plan");
                        return false;
                    }
                    (*explain_payload)["plan"] = plan_it->second;
                    auto plan_text_it = query_payload->find("plan_text");
                    if (plan_text_it != query_payload->end())
                    {
                        (*explain_payload)["plan_text"] = plan_text_it->second;
                    }
                    auto plan_hash_it = query_payload->find("plan_hash");
                    if (plan_hash_it != query_payload->end())
                    {
                        (*explain_payload)["plan_hash"] = plan_hash_it->second;
                    }
                    std::ostringstream explain_warning;
                    explain_warning << "explain payload stamped"
                                    << " root_has_plan="
                                    << (explain_payload->find("plan") != explain_payload->end()
                                            ? "true"
                                            : "false")
                                    << " root_has_plan_text="
                                    << (explain_payload->find("plan_text") != explain_payload->end()
                                            ? "true"
                                            : "false")
                                    << " query_has_plan="
                                    << (query_payload->find("plan") != query_payload->end()
                                            ? "true"
                                            : "false")
                                    << " query_has_plan_text="
                                    << (query_payload->find("plan_text") != query_payload->end()
                                            ? "true"
                                            : "false");
                    result.warnings.push_back(explain_warning.str());
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
                    if (!refreshCachedAdaptiveFeedbackPayload(
                            get_result.value.sblr_payload,
                            optimizer::QueryProfiler::getInstance()
                                .latestCardinalityFeedback(query_feedback_key),
                            result.bytecode,
                            instruction_error))
                    {
                        result.errors.push_back(instruction_error.empty()
                            ? "Cached runtime plan adaptive feedback refresh failed"
                            : instruction_error);
                        return result;
                    }
                    result.success = true;
                    result.cache_hit = true;
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

    auto invalidateAllQueryCompilerV3PlanCache() -> uint64_t
    {
        return planCache().invalidateAll();
    }

} // namespace scratchbird::sblr::detail
