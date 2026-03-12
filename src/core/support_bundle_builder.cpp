#include "scratchbird/core/support_bundle_builder.h"

#include "scratchbird/core/database.h"
#include "scratchbird/core/secure_diagnostics.h"
#include "scratchbird/core/sweep_manager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace scratchbird::core
{

namespace
{

constexpr std::array<CatalogManager::ClusterNodeRole, 15> kAllNodeRoles = {
    CatalogManager::ClusterNodeRole::METADATA,
    CatalogManager::ClusterNodeRole::OLTP_DATA,
    CatalogManager::ClusterNodeRole::ROUTER,
    CatalogManager::ClusterNodeRole::PARSER,
    CatalogManager::ClusterNodeRole::LISTENER,
    CatalogManager::ClusterNodeRole::BACKUP,
    CatalogManager::ClusterNodeRole::SCHEDULER,
    CatalogManager::ClusterNodeRole::METRICS,
    CatalogManager::ClusterNodeRole::OLAP_INGEST,
    CatalogManager::ClusterNodeRole::OLAP_STORAGE,
    CatalogManager::ClusterNodeRole::OLAP_COMPUTE,
    CatalogManager::ClusterNodeRole::VECTOR_INDEX,
    CatalogManager::ClusterNodeRole::SEARCH_INDEX,
    CatalogManager::ClusterNodeRole::GRAPH_COMPUTE,
    CatalogManager::ClusterNodeRole::CACHE,
};

auto formatNodeRole(CatalogManager::ClusterNodeRole role) -> std::string
{
    switch (role)
    {
        case CatalogManager::ClusterNodeRole::METADATA:
            return "METADATA";
        case CatalogManager::ClusterNodeRole::OLTP_DATA:
            return "OLTP_DATA";
        case CatalogManager::ClusterNodeRole::ROUTER:
            return "ROUTER";
        case CatalogManager::ClusterNodeRole::PARSER:
            return "PARSER";
        case CatalogManager::ClusterNodeRole::LISTENER:
            return "LISTENER";
        case CatalogManager::ClusterNodeRole::BACKUP:
            return "BACKUP";
        case CatalogManager::ClusterNodeRole::SCHEDULER:
            return "SCHEDULER";
        case CatalogManager::ClusterNodeRole::METRICS:
            return "METRICS";
        case CatalogManager::ClusterNodeRole::OLAP_INGEST:
            return "OLAP_INGEST";
        case CatalogManager::ClusterNodeRole::OLAP_STORAGE:
            return "OLAP_STORAGE";
        case CatalogManager::ClusterNodeRole::OLAP_COMPUTE:
            return "OLAP_COMPUTE";
        case CatalogManager::ClusterNodeRole::VECTOR_INDEX:
            return "VECTOR_INDEX";
        case CatalogManager::ClusterNodeRole::SEARCH_INDEX:
            return "SEARCH_INDEX";
        case CatalogManager::ClusterNodeRole::GRAPH_COMPUTE:
            return "GRAPH_COMPUTE";
        case CatalogManager::ClusterNodeRole::CACHE:
            return "CACHE";
    }
    return "UNKNOWN";
}

auto formatAlertSeverity(CatalogManager::AlertSeverity severity) -> const char*
{
    switch (severity)
    {
        case CatalogManager::AlertSeverity::INFO:
            return "INFO";
        case CatalogManager::AlertSeverity::WARNING:
            return "WARNING";
        case CatalogManager::AlertSeverity::CRITICAL:
            return "CRITICAL";
    }
    return "UNKNOWN";
}

auto formatAlertEventState(CatalogManager::AlertEventState state) -> const char*
{
    switch (state)
    {
        case CatalogManager::AlertEventState::OPEN:
            return "OPEN";
        case CatalogManager::AlertEventState::ACKED:
            return "ACKED";
        case CatalogManager::AlertEventState::RESOLVED:
            return "RESOLVED";
        case CatalogManager::AlertEventState::SUPPRESSED:
            return "SUPPRESSED";
    }
    return "UNKNOWN";
}

auto formatSloBurnSeverity(CatalogManager::SloBurnSeverity severity) -> const char*
{
    switch (severity)
    {
        case CatalogManager::SloBurnSeverity::NONE:
            return "NONE";
        case CatalogManager::SloBurnSeverity::MODERATE:
            return "MODERATE";
        case CatalogManager::SloBurnSeverity::HIGH:
            return "HIGH";
        case CatalogManager::SloBurnSeverity::CRITICAL:
            return "CRITICAL";
    }
    return "UNKNOWN";
}

auto formatHealingRunState(CatalogManager::HealingRunState state) -> const char*
{
    switch (state)
    {
        case CatalogManager::HealingRunState::QUEUED:
            return "QUEUED";
        case CatalogManager::HealingRunState::RUNNING:
            return "RUNNING";
        case CatalogManager::HealingRunState::COMPLETED:
            return "COMPLETED";
        case CatalogManager::HealingRunState::FAILED:
            return "FAILED";
        case CatalogManager::HealingRunState::CANCELLED:
            return "CANCELLED";
    }
    return "UNKNOWN";
}

auto isVulnerabilityRule(const std::string& rule_name) -> bool
{
    const std::string normalized = IdentifierUtils::toUpper(rule_name);
    return normalized.rfind("VULN_", 0) == 0 ||
           normalized.rfind("CVE_", 0) == 0 ||
           normalized.find("VULNERABILITY") != std::string::npos;
}

auto ackSlaFor(const AlertReadinessRequest& request,
               CatalogManager::AlertSeverity severity) -> uint64_t
{
    switch (severity)
    {
        case CatalogManager::AlertSeverity::INFO:
            return request.info_ack_sla_ms;
        case CatalogManager::AlertSeverity::WARNING:
            return request.warning_ack_sla_ms;
        case CatalogManager::AlertSeverity::CRITICAL:
            return request.critical_ack_sla_ms;
    }
    return request.warning_ack_sla_ms;
}

auto vulnerabilitySlaFor(const AlertReadinessRequest& request,
                         CatalogManager::AlertSeverity severity) -> uint64_t
{
    switch (severity)
    {
        case CatalogManager::AlertSeverity::CRITICAL:
            return request.critical_vulnerability_sla_ms;
        case CatalogManager::AlertSeverity::INFO:
        case CatalogManager::AlertSeverity::WARNING:
            return request.warning_vulnerability_sla_ms;
    }
    return request.warning_vulnerability_sla_ms;
}

auto withinWindow(uint64_t value, uint64_t anchor_time, uint32_t window_minutes) -> bool
{
    if (window_minutes == 0 || anchor_time == 0)
    {
        return true;
    }
    const uint64_t window_micros = static_cast<uint64_t>(window_minutes) * 60ULL * 1000000ULL;
    if (anchor_time < window_micros)
    {
        return true;
    }
    return value >= (anchor_time - window_micros);
}

auto sanitizeText(const std::string& text, uint64_t* redacted_counter) -> std::string
{
    const std::string sanitized = redactSensitiveDiagnosticText(text);
    if (redacted_counter != nullptr && sanitized != text)
    {
        *redacted_counter += 1;
    }
    return sanitized;
}

auto sanitizeField(std::string_view key,
                   const std::string& text,
                   uint64_t* redacted_counter) -> std::string
{
    const std::string sanitized = redactSensitiveDiagnosticField(key, text);
    if (redacted_counter != nullptr && sanitized != text)
    {
        *redacted_counter += 1;
    }
    return sanitized;
}

auto nowTicks() -> uint64_t
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void appendKeyValueLine(std::ostringstream& out,
                        const std::string& key,
                        const std::string& value)
{
    out << key << "=" << value << "\n";
}

} // namespace

SupportBundleBuilder::SupportBundleBuilder(Database* db)
    : db_(db)
{
}

auto SupportBundleBuilder::readinessHealthStateName(ReadinessHealthState state) -> const char*
{
    switch (state)
    {
        case ReadinessHealthState::READY:
            return "READY";
        case ReadinessHealthState::DEGRADED:
            return "DEGRADED";
        case ReadinessHealthState::BLOCKED:
            return "BLOCKED";
    }
    return "UNKNOWN";
}

auto SupportBundleBuilder::snapshotAlertReadiness(const AlertReadinessRequest& request,
                                                  std::vector<AlertReadinessRow>& rows_out,
                                                  ReadinessHealthSummary& summary_out,
                                                  ErrorContext* ctx) const -> Status
{
    rows_out.clear();
    summary_out = ReadinessHealthSummary{};
    if (db_ == nullptr || !db_->is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "support bundle builder requires an open database");
        return Status::INVALID_ARGUMENT;
    }

    auto* catalog = db_->catalog_manager();
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "catalog manager is not available");
        return Status::INTERNAL_ERROR;
    }

    if (request.info_ack_sla_ms == 0 || request.warning_ack_sla_ms == 0 ||
        request.critical_ack_sla_ms == 0 || request.warning_vulnerability_sla_ms == 0 ||
        request.critical_vulnerability_sla_ms == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "alert readiness SLA values must be non-zero");
        return Status::INVALID_ARGUMENT;
    }

    const uint64_t now_time = request.now_time == 0 ? nowTicks() : request.now_time;
    summary_out.evaluation_time = now_time;

    std::vector<CatalogManager::AlertRuleCatalogInfo> rules;
    Status status = catalog->listAlertRuleCatalogEntries(rules, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }

    std::vector<CatalogManager::AlertSilenceCatalogInfo> silences;
    status = catalog->listAlertSilenceCatalogEntries(silences, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }

    std::vector<CatalogManager::AlertTargetCatalogInfo> targets;
    status = catalog->listAlertTargetCatalogEntries(targets, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    std::unordered_map<ID, CatalogManager::AlertTargetCatalogInfo, IDHash> target_by_id;
    for (const auto& target : targets)
    {
        if (target.is_valid)
        {
            target_by_id[target.target_id] = target;
        }
    }

    uint64_t anchor_time = 0;
    std::vector<AlertReadinessRow> all_rows;
    for (const auto& rule : rules)
    {
        if (!rule.is_valid || !rule.is_enabled)
        {
            continue;
        }

        std::vector<CatalogManager::AlertEventCatalogInfo> events;
        status = catalog->listAlertEventCatalogEntries(rule.rule_id, events, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        std::vector<CatalogManager::AlertRouteCatalogInfo> routes;
        status = catalog->listAlertRouteCatalogEntries(rule.rule_id, routes, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        for (const auto& event : events)
        {
            if (!event.is_valid ||
                event.event_state == CatalogManager::AlertEventState::RESOLVED ||
                event.event_state == CatalogManager::AlertEventState::SUPPRESSED)
            {
                continue;
            }

            summary_out.open_event_count += 1;

            AlertReadinessRow row{};
            row.event_id = event.event_id;
            row.rule_id = rule.rule_id;
            row.rule_name = rule.rule_name;
            row.severity = event.severity;
            row.event_state = event.event_state;
            row.event_time = event.event_time;
            row.vulnerability_signal = isVulnerabilityRule(rule.rule_name);
            row.ack_deadline_time = event.event_time + ackSlaFor(request, event.severity);
            row.remediation_deadline_time =
                event.event_time + vulnerabilitySlaFor(request, event.severity);

            std::vector<CatalogManager::AlertAckCatalogInfo> acks;
            status = catalog->listAlertAckCatalogEntries(event.event_id, acks, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            row.acked = event.event_state == CatalogManager::AlertEventState::ACKED || !acks.empty();
            row.ack_overdue = !row.acked && now_time > row.ack_deadline_time;
            row.remediation_overdue =
                row.vulnerability_signal && now_time > row.remediation_deadline_time;

            bool silenced = false;
            std::vector<std::string> target_summaries;
            for (const auto& route : routes)
            {
                if (!route.is_valid || !route.is_enabled)
                {
                    continue;
                }
                const bool severity_in_range =
                    static_cast<uint8_t>(event.severity) >= static_cast<uint8_t>(route.severity_min) &&
                    static_cast<uint8_t>(event.severity) <= static_cast<uint8_t>(route.severity_max);
                if (!severity_in_range)
                {
                    continue;
                }

                row.route_count += 1;
                row.target_count += 1;
                auto target_it = target_by_id.find(route.target_id);
                if (target_it != target_by_id.end())
                {
                    target_summaries.push_back(
                        target_it->second.target_name + "@" +
                        redactSensitiveDiagnosticText(target_it->second.endpoint));
                }

                for (const auto& silence : silences)
                {
                    if (!silence.is_valid || !silence.is_enabled ||
                        now_time < silence.starts_time || now_time > silence.ends_time)
                    {
                        continue;
                    }
                    if (silence.scope_kind == CatalogManager::AlertSilenceScope::CLUSTER)
                    {
                        silenced = true;
                    }
                    else if (silence.scope_kind == CatalogManager::AlertSilenceScope::RULE &&
                             silence.has_scope_uuid && silence.scope_uuid == rule.rule_id)
                    {
                        silenced = true;
                    }
                    else if (silence.scope_kind == CatalogManager::AlertSilenceScope::TARGET &&
                             silence.has_scope_uuid && silence.scope_uuid == route.target_id)
                    {
                        silenced = true;
                    }
                }
            }
            row.silenced = silenced;
            if (!target_summaries.empty())
            {
                std::ostringstream summary;
                for (size_t i = 0; i < target_summaries.size(); ++i)
                {
                    if (i != 0)
                    {
                        summary << ",";
                    }
                    summary << target_summaries[i];
                }
                row.target_summary = summary.str();
            }

            if (row.silenced)
            {
                summary_out.silenced_event_count += 1;
                continue;
            }

            if (row.ack_overdue || row.remediation_overdue)
            {
                summary_out.actionable_event_count += 1;
            }
            if (row.ack_overdue)
            {
                summary_out.ack_overdue_count += 1;
            }
            if (row.remediation_overdue)
            {
                summary_out.remediation_overdue_count += 1;
            }
            if (row.severity == CatalogManager::AlertSeverity::CRITICAL)
            {
                summary_out.critical_open_count += 1;
            }

            anchor_time = std::max(anchor_time, row.event_time);
            all_rows.push_back(std::move(row));
        }
    }

    for (const auto& row : all_rows)
    {
        if (!withinWindow(row.event_time, anchor_time, request.window_minutes))
        {
            continue;
        }
        summary_out.visible_event_count += 1;
        rows_out.push_back(row);
    }

    std::vector<CatalogManager::HealingRunCatalogInfo> healing_runs;
    status = catalog->listHealingRunCatalogEntries(ID{}, healing_runs, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    for (const auto& run : healing_runs)
    {
        if (!run.is_valid)
        {
            continue;
        }
        if (run.state == CatalogManager::HealingRunState::QUEUED ||
            run.state == CatalogManager::HealingRunState::RUNNING)
        {
            summary_out.active_healing_run_count += 1;
        }
        if (run.state == CatalogManager::HealingRunState::FAILED)
        {
            summary_out.failed_healing_run_count += 1;
        }
    }

    std::vector<CatalogManager::SloBurnEventCatalogInfo> burn_rows;
    status = catalog->listSloBurnEventCatalogEntries(ID{}, burn_rows, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    for (const auto& burn : burn_rows)
    {
        if (!burn.is_valid)
        {
            continue;
        }
        if (burn.has_resolved_time && burn.resolved_time <= now_time)
        {
            continue;
        }
        if (burn.burn_severity == CatalogManager::SloBurnSeverity::HIGH)
        {
            summary_out.high_burn_count += 1;
        }
        else if (burn.burn_severity == CatalogManager::SloBurnSeverity::CRITICAL)
        {
            summary_out.critical_burn_count += 1;
        }
    }

    if (summary_out.failed_healing_run_count > 0 ||
        summary_out.critical_open_count > 0 ||
        summary_out.remediation_overdue_count > 0 ||
        summary_out.critical_burn_count > 0)
    {
        summary_out.state = ReadinessHealthState::BLOCKED;
        summary_out.summary_text = "Critical incident pressure or failed healing blocks readiness";
    }
    else if (summary_out.visible_event_count > 0 ||
             summary_out.ack_overdue_count > 0 ||
             summary_out.active_healing_run_count > 0 ||
             summary_out.high_burn_count > 0)
    {
        summary_out.state = ReadinessHealthState::DEGRADED;
        summary_out.summary_text = "Open incidents or high SLO burn degrade readiness";
    }
    else
    {
        summary_out.state = ReadinessHealthState::READY;
        summary_out.summary_text = "No blocking alerting or readiness conditions are active";
    }

    return Status::OK;
}

auto SupportBundleBuilder::snapshotSupportBundleSafety(const SupportBundleRequest& request,
                                                       SupportBundleSafetySummary& summary_out,
                                                       ErrorContext* ctx) const -> Status
{
    summary_out = SupportBundleSafetySummary{};
    if (db_ == nullptr || !db_->is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "support bundle builder requires an open database");
        return Status::INVALID_ARGUMENT;
    }

    auto* catalog = db_->catalog_manager();
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "catalog manager is not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<AlertReadinessRow> readiness_rows;
    Status status = snapshotAlertReadiness(request.readiness,
                                           readiness_rows,
                                           summary_out.readiness,
                                           ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint64_t redacted_field_count = 0;
    for (const auto& row : readiness_rows)
    {
        (void)sanitizeText(row.rule_name, &redacted_field_count);
        (void)sanitizeText(row.target_summary, &redacted_field_count);
    }

    std::vector<CatalogManager::AlertTargetCatalogInfo> targets;
    status = catalog->listAlertTargetCatalogEntries(targets, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    for (const auto& target : targets)
    {
        if (target.is_valid)
        {
            (void)sanitizeField("endpoint", target.endpoint, &redacted_field_count);
        }
    }

    std::vector<CatalogManager::AlertAckCatalogInfo> acks;
    status = catalog->listAlertAckCatalogEntries(ID{}, acks, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    for (const auto& ack : acks)
    {
        if (ack.is_valid && ack.has_comment)
        {
            (void)sanitizeText(ack.comment, &redacted_field_count);
        }
    }

    std::vector<CatalogManager::AlertSilenceCatalogInfo> silences;
    status = catalog->listAlertSilenceCatalogEntries(silences, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    for (const auto& silence : silences)
    {
        if (silence.is_valid && silence.has_reason)
        {
            (void)sanitizeText(silence.reason, &redacted_field_count);
        }
    }

    auto* workload_governance = db_->workload_governance();
    if (workload_governance != nullptr && request.include_slo_status)
    {
        std::vector<WorkloadGovernance::SloStatusRow> rows;
        status = workload_governance->snapshotSloStatus(rows, request.readiness.now_time, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        summary_out.slo_status_count = static_cast<uint64_t>(rows.size());
    }
    if (workload_governance != nullptr && request.include_error_budget_status)
    {
        std::vector<WorkloadGovernance::ErrorBudgetStatusRow> rows;
        status = workload_governance->snapshotErrorBudgetStatus(rows, request.readiness.now_time, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        summary_out.error_budget_status_count = static_cast<uint64_t>(rows.size());
    }

    if (request.include_autoscale_actions)
    {
        for (CatalogManager::ClusterNodeRole role : kAllNodeRoles)
        {
            std::vector<CatalogManager::AutoscaleActionCatalogInfo> rows;
            status = catalog->listAutoscaleActionCatalogEntries(role, rows, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            summary_out.autoscale_action_count += static_cast<uint64_t>(rows.size());
            for (const auto& row : rows)
            {
                (void)sanitizeText(row.trigger_reason, &redacted_field_count);
                (void)sanitizeText(row.failure_code, &redacted_field_count);
            }
        }
    }

    if (request.include_admission_tuning)
    {
        for (CatalogManager::ClusterNodeRole role : kAllNodeRoles)
        {
            std::vector<CatalogManager::AdmissionTuningEventCatalogInfo> rows;
            status = catalog->listAdmissionTuningEventCatalogEntries(role, rows, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            summary_out.admission_tuning_count += static_cast<uint64_t>(rows.size());
            for (const auto& row : rows)
            {
                (void)sanitizeText(row.reason, &redacted_field_count);
            }
        }
    }

    if (request.include_shadow_capture_manifests)
    {
        std::vector<CatalogManager::ShadowCaptureManifestCatalogInfo> rows;
        status = catalog->listShadowCaptureManifestCatalogEntries(rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        summary_out.shadow_capture_manifest_count = static_cast<uint64_t>(rows.size());
        for (const auto& row : rows)
        {
            (void)sanitizeText(row.payload_manifest, &redacted_field_count);
        }
    }

    if (request.include_page_audit_findings)
    {
        std::vector<CatalogManager::PageAuditFindingCatalogInfo> rows;
        status = catalog->listPageAuditFindingCatalogEntries(rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        summary_out.page_audit_finding_count = static_cast<uint64_t>(rows.size());
        for (const auto& row : rows)
        {
            (void)sanitizeText(row.details_json, &redacted_field_count);
        }
    }

    if (request.include_wal_after_segments)
    {
        if (auto* sweep_manager = db_->sweep_manager(); sweep_manager != nullptr)
        {
            std::vector<SweepWalAfterLogSegment> rows;
            status = sweep_manager->listWalAfterLogSegments(rows, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            summary_out.wal_after_segment_count = static_cast<uint64_t>(rows.size());
            for (const auto& row : rows)
            {
                (void)sanitizeText(row.segment_path, &redacted_field_count);
                (void)sanitizeText(row.statement_hashes_csv, &redacted_field_count);
            }
        }
    }

    if (request.include_audit_export_segments)
    {
        std::vector<CatalogManager::AuditExportSegmentCatalogInfo> rows;
        status = catalog->listAuditExportSegmentCatalogEntries(ID{}, rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        summary_out.audit_export_segment_count = static_cast<uint64_t>(rows.size());
        for (const auto& row : rows)
        {
            (void)sanitizeText(row.payload_manifest, &redacted_field_count);
        }
    }

    summary_out.redacted_field_count = redacted_field_count;
    summary_out.redaction_enforced = true;
    return Status::OK;
}

auto SupportBundleBuilder::generateSupportBundle(const SupportBundleRequest& request,
                                                 SupportBundleResult& result_out,
                                                 ErrorContext* ctx) const -> Status
{
    result_out = SupportBundleResult{};
    if (request.output_path.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "support bundle output_path is required");
        return Status::INVALID_ARGUMENT;
    }
    if (db_ == nullptr || !db_->is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "support bundle builder requires an open database");
        return Status::INVALID_ARGUMENT;
    }

    auto* catalog = db_->catalog_manager();
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "catalog manager is not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<AlertReadinessRow> readiness_rows;
    Status status = snapshotAlertReadiness(request.readiness,
                                           readiness_rows,
                                           result_out.safety.readiness,
                                           ctx);
    if (status != Status::OK)
    {
        return status;
    }
    status = snapshotSupportBundleSafety(request, result_out.safety, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    result_out.bundle_id = generateUuidV7();
    result_out.output_path = request.output_path;
    result_out.alert_row_count = static_cast<uint64_t>(readiness_rows.size());
    result_out.redacted_field_count = result_out.safety.redacted_field_count;
    result_out.redaction_enforced = result_out.safety.redaction_enforced;

    std::ostringstream manifest;
    appendKeyValueLine(manifest, "manifest_version", "1");
    appendKeyValueLine(manifest, "bundle_id", result_out.bundle_id.toString());
    appendKeyValueLine(manifest, "created_time",
                       std::to_string(request.readiness.now_time == 0
                                          ? nowTicks()
                                          : request.readiness.now_time));
    appendKeyValueLine(manifest, "readiness_state",
                       readinessHealthStateName(result_out.safety.readiness.state));
    appendKeyValueLine(manifest, "readiness_summary",
                       sanitizeText(result_out.safety.readiness.summary_text,
                                    &result_out.redacted_field_count));
    appendKeyValueLine(manifest, "redaction_enforced",
                       result_out.redaction_enforced ? "1" : "0");
    appendKeyValueLine(manifest, "redacted_field_count",
                       std::to_string(result_out.redacted_field_count));
    appendKeyValueLine(manifest, "open_event_count",
                       std::to_string(result_out.safety.readiness.open_event_count));
    appendKeyValueLine(manifest, "visible_event_count",
                       std::to_string(result_out.safety.readiness.visible_event_count));
    appendKeyValueLine(manifest, "actionable_event_count",
                       std::to_string(result_out.safety.readiness.actionable_event_count));
    appendKeyValueLine(manifest, "critical_open_count",
                       std::to_string(result_out.safety.readiness.critical_open_count));
    appendKeyValueLine(manifest, "shadow_capture_manifest_count",
                       std::to_string(result_out.safety.shadow_capture_manifest_count));
    appendKeyValueLine(manifest, "page_audit_finding_count",
                       std::to_string(result_out.safety.page_audit_finding_count));
    appendKeyValueLine(manifest, "wal_after_segment_count",
                       std::to_string(result_out.safety.wal_after_segment_count));
    appendKeyValueLine(manifest, "audit_export_segment_count",
                       std::to_string(result_out.safety.audit_export_segment_count));
    appendKeyValueLine(manifest, "slo_status_count",
                       std::to_string(result_out.safety.slo_status_count));
    appendKeyValueLine(manifest, "error_budget_status_count",
                       std::to_string(result_out.safety.error_budget_status_count));

    manifest << "[alerts]\n";
    for (const auto& row : readiness_rows)
    {
        manifest << "event_id=" << row.event_id.toString()
                 << " rule_id=" << row.rule_id.toString()
                 << " rule_name=" << sanitizeText(row.rule_name, &result_out.redacted_field_count)
                 << " severity=" << formatAlertSeverity(row.severity)
                 << " state=" << formatAlertEventState(row.event_state)
                 << " event_time=" << row.event_time
                 << " silenced=" << (row.silenced ? 1 : 0)
                 << " acked=" << (row.acked ? 1 : 0)
                 << " ack_overdue=" << (row.ack_overdue ? 1 : 0)
                 << " remediation_overdue=" << (row.remediation_overdue ? 1 : 0)
                 << " routes=" << row.route_count
                 << " targets=" << row.target_count
                 << " target_summary=" << sanitizeText(row.target_summary, &result_out.redacted_field_count)
                 << "\n";
    }

    if (request.include_slo_status)
    {
        if (auto* workload_governance = db_->workload_governance(); workload_governance != nullptr)
        {
            std::vector<WorkloadGovernance::SloStatusRow> rows;
            status = workload_governance->snapshotSloStatus(rows, request.readiness.now_time, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            manifest << "[slo_status]\n";
            for (const auto& row : rows)
            {
                manifest << "role=" << sanitizeText(row.role, &result_out.redacted_field_count)
                         << " node_name=" << sanitizeText(row.node_name, &result_out.redacted_field_count)
                         << " profile_name=" << sanitizeText(row.profile_name, &result_out.redacted_field_count)
                         << " burn_severity=" << sanitizeText(row.burn_severity, &result_out.redacted_field_count)
                         << " short_burn_rate=" << row.short_burn_rate
                         << " long_burn_rate=" << row.long_burn_rate
                         << " availability_sli_pct=" << row.availability_sli_pct
                         << " error_rate_sli_pct=" << row.error_rate_sli_pct
                         << "\n";
            }
        }
    }

    if (request.include_error_budget_status)
    {
        if (auto* workload_governance = db_->workload_governance(); workload_governance != nullptr)
        {
            std::vector<WorkloadGovernance::ErrorBudgetStatusRow> rows;
            status = workload_governance->snapshotErrorBudgetStatus(rows, request.readiness.now_time, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            manifest << "[error_budget_status]\n";
            for (const auto& row : rows)
            {
                manifest << "role=" << sanitizeText(row.role, &result_out.redacted_field_count)
                         << " node_name=" << sanitizeText(row.node_name, &result_out.redacted_field_count)
                         << " profile_name=" << sanitizeText(row.profile_name, &result_out.redacted_field_count)
                         << " remaining_budget_pct=" << row.remaining_budget_pct
                         << " burn_severity=" << sanitizeText(row.burn_severity, &result_out.redacted_field_count)
                         << "\n";
            }
        }
    }

    if (request.include_shadow_capture_manifests)
    {
        std::vector<CatalogManager::ShadowCaptureManifestCatalogInfo> rows;
        status = catalog->listShadowCaptureManifestCatalogEntries(rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        manifest << "[shadow_capture_manifests]\n";
        for (const auto& row : rows)
        {
            manifest << "manifest_id=" << row.manifest_id.toString()
                     << " tx_uuid=" << row.tx_uuid.toString()
                     << " object_uuid=" << row.object_uuid.toString()
                     << " capture_scope=" << sanitizeText(row.capture_scope, &result_out.redacted_field_count)
                     << " capture_format=" << sanitizeText(row.capture_format, &result_out.redacted_field_count)
                     << " payload_manifest="
                     << sanitizeText(row.payload_manifest, &result_out.redacted_field_count)
                     << "\n";
        }
    }

    if (request.include_page_audit_findings)
    {
        std::vector<CatalogManager::PageAuditFindingCatalogInfo> rows;
        status = catalog->listPageAuditFindingCatalogEntries(rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        manifest << "[page_audit_findings]\n";
        for (const auto& row : rows)
        {
            manifest << "finding_id=" << row.finding_id.toString()
                     << " page_id=" << row.page_id
                     << " severity=" << sanitizeText(row.severity, &result_out.redacted_field_count)
                     << " error_code=" << sanitizeText(row.error_code, &result_out.redacted_field_count)
                     << " details_json="
                     << sanitizeText(row.details_json, &result_out.redacted_field_count)
                     << "\n";
        }
    }

    if (request.include_wal_after_segments)
    {
        if (auto* sweep_manager = db_->sweep_manager(); sweep_manager != nullptr)
        {
            std::vector<SweepWalAfterLogSegment> rows;
            status = sweep_manager->listWalAfterLogSegments(rows, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            manifest << "[wal_after_segments]\n";
            for (const auto& row : rows)
            {
                manifest << "segment_id=" << row.segment_id.toString()
                         << " tx_uuid=" << row.tx_uuid.toString()
                         << " stream_seq=" << row.stream_seq
                         << " commit_time=" << row.commit_time
                         << " shipping_mode=" << sanitizeText(row.shipping_mode, &result_out.redacted_field_count)
                         << " segment_path=" << sanitizeText(row.segment_path, &result_out.redacted_field_count)
                         << "\n";
            }
        }
    }

    if (request.include_audit_export_segments)
    {
        std::vector<CatalogManager::AuditExportSegmentCatalogInfo> rows;
        status = catalog->listAuditExportSegmentCatalogEntries(ID{}, rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        manifest << "[audit_export_segments]\n";
        for (const auto& row : rows)
        {
            manifest << "segment_id=" << row.audit_export_segment_id.toString()
                     << " evidence_class=" << sanitizeText(row.evidence_class, &result_out.redacted_field_count)
                     << " delivery_state=" << sanitizeText(row.delivery_state, &result_out.redacted_field_count)
                     << " segment_seq=" << row.segment_seq
                     << " payload_manifest=" << sanitizeText(row.payload_manifest, &result_out.redacted_field_count)
                     << "\n";
        }
    }

    if (request.include_autoscale_actions)
    {
        manifest << "[autoscale_actions]\n";
        for (CatalogManager::ClusterNodeRole role : kAllNodeRoles)
        {
            std::vector<CatalogManager::AutoscaleActionCatalogInfo> rows;
            status = catalog->listAutoscaleActionCatalogEntries(role, rows, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            for (const auto& row : rows)
            {
                manifest << "role=" << formatNodeRole(row.role)
                         << " action_time=" << row.action_time
                         << " requested_delta=" << row.requested_count_delta
                         << " applied_delta=" << row.applied_count_delta
                         << " reason=" << sanitizeText(row.trigger_reason, &result_out.redacted_field_count)
                         << " failure_code=" << sanitizeText(row.failure_code, &result_out.redacted_field_count)
                         << "\n";
            }
        }
    }

    if (request.include_admission_tuning)
    {
        manifest << "[admission_tuning]\n";
        for (CatalogManager::ClusterNodeRole role : kAllNodeRoles)
        {
            std::vector<CatalogManager::AdmissionTuningEventCatalogInfo> rows;
            status = catalog->listAdmissionTuningEventCatalogEntries(role, rows, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
            for (const auto& row : rows)
            {
                manifest << "role=" << formatNodeRole(row.role)
                         << " event_time=" << row.event_time
                         << " reason=" << sanitizeText(row.reason, &result_out.redacted_field_count)
                         << " old_q=" << row.old_max_concurrent_queries
                         << " new_q=" << row.new_max_concurrent_queries
                         << "\n";
            }
        }
    }

    std::vector<CatalogManager::HealingRunCatalogInfo> healing_runs;
    status = catalog->listHealingRunCatalogEntries(ID{}, healing_runs, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    manifest << "[healing_runs]\n";
    for (const auto& run : healing_runs)
    {
        manifest << "run_id=" << run.run_id.toString()
                 << " policy_id=" << run.policy_id.toString()
                 << " state=" << formatHealingRunState(run.state)
                 << " started_time=" << run.started_time
                 << " completed_time=" << (run.has_completed_time ? run.completed_time : 0)
                 << " error_message="
                 << sanitizeText(run.has_error_message ? run.error_message : std::string{},
                                 &result_out.redacted_field_count)
                 << "\n";
    }

    result_out.manifest_preview = manifest.str();
    result_out.safety.redacted_field_count = result_out.redacted_field_count;
    result_out.safety.redaction_enforced = true;

    std::filesystem::path output_path(request.output_path);
    if (output_path.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "failed to create support bundle directory");
            return Status::IO_ERROR;
        }
    }

    std::ofstream out(request.output_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "failed to open support bundle output file");
        return Status::IO_ERROR;
    }
    out << result_out.manifest_preview;
    if (!out.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "failed to write support bundle output file");
        return Status::IO_ERROR;
    }

    return Status::OK;
}

} // namespace scratchbird::core
