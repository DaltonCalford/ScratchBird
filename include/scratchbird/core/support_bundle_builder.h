#pragma once

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/workload_governance.h"

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::core
{

class Database;

enum class ReadinessHealthState : uint8_t
{
    READY = 0,
    DEGRADED = 1,
    BLOCKED = 2
};

struct AlertReadinessRequest
{
    uint64_t now_time = 0;
    uint32_t window_minutes = 0;
    uint64_t info_ack_sla_ms = 86400000ULL;
    uint64_t warning_ack_sla_ms = 14400000ULL;
    uint64_t critical_ack_sla_ms = 900000ULL;
    uint64_t warning_vulnerability_sla_ms = 2592000000ULL;
    uint64_t critical_vulnerability_sla_ms = 604800000ULL;
};

struct AlertReadinessRow
{
    ID event_id{};
    ID rule_id{};
    std::string rule_name;
    CatalogManager::AlertSeverity severity = CatalogManager::AlertSeverity::INFO;
    CatalogManager::AlertEventState event_state = CatalogManager::AlertEventState::OPEN;
    uint64_t event_time = 0;
    bool silenced = false;
    bool acked = false;
    bool vulnerability_signal = false;
    bool ack_overdue = false;
    bool remediation_overdue = false;
    uint64_t ack_deadline_time = 0;
    uint64_t remediation_deadline_time = 0;
    uint64_t route_count = 0;
    uint64_t target_count = 0;
    std::string target_summary;
};

struct ReadinessHealthSummary
{
    ReadinessHealthState state = ReadinessHealthState::READY;
    uint64_t evaluation_time = 0;
    uint64_t open_event_count = 0;
    uint64_t visible_event_count = 0;
    uint64_t silenced_event_count = 0;
    uint64_t actionable_event_count = 0;
    uint64_t ack_overdue_count = 0;
    uint64_t remediation_overdue_count = 0;
    uint64_t critical_open_count = 0;
    uint64_t active_healing_run_count = 0;
    uint64_t failed_healing_run_count = 0;
    uint64_t high_burn_count = 0;
    uint64_t critical_burn_count = 0;
    std::string summary_text;
};

struct SupportBundleSafetySummary
{
    ReadinessHealthSummary readiness;
    uint64_t slo_status_count = 0;
    uint64_t error_budget_status_count = 0;
    uint64_t autoscale_action_count = 0;
    uint64_t admission_tuning_count = 0;
    uint64_t shadow_capture_manifest_count = 0;
    uint64_t page_audit_finding_count = 0;
    uint64_t wal_after_segment_count = 0;
    uint64_t audit_export_segment_count = 0;
    uint64_t redacted_field_count = 0;
    bool redaction_enforced = false;
};

struct SupportBundleRequest
{
    std::string output_path;
    AlertReadinessRequest readiness;
    bool include_slo_status = true;
    bool include_error_budget_status = true;
    bool include_autoscale_actions = true;
    bool include_admission_tuning = true;
    bool include_shadow_capture_manifests = true;
    bool include_page_audit_findings = true;
    bool include_wal_after_segments = true;
    bool include_audit_export_segments = true;
};

struct SupportBundleResult
{
    ID bundle_id{};
    std::string output_path;
    SupportBundleSafetySummary safety;
    uint64_t alert_row_count = 0;
    uint64_t redacted_field_count = 0;
    bool redaction_enforced = false;
    std::string manifest_preview;
};

class SupportBundleBuilder
{
public:
    explicit SupportBundleBuilder(Database* db);

    auto snapshotAlertReadiness(const AlertReadinessRequest& request,
                                std::vector<AlertReadinessRow>& rows_out,
                                ReadinessHealthSummary& summary_out,
                                ErrorContext* ctx = nullptr) const -> Status;

    auto snapshotSupportBundleSafety(const SupportBundleRequest& request,
                                     SupportBundleSafetySummary& summary_out,
                                     ErrorContext* ctx = nullptr) const -> Status;

    auto generateSupportBundle(const SupportBundleRequest& request,
                               SupportBundleResult& result_out,
                               ErrorContext* ctx = nullptr) const -> Status;

    static auto readinessHealthStateName(ReadinessHealthState state) -> const char*;

private:
    Database* db_ = nullptr;
};

} // namespace scratchbird::core
