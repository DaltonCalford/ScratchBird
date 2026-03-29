#!/usr/bin/env bash
# Section 35 invariant: this required gate is a bounded executable recovery or
# durability evidence surface. It does not by itself certify universal stage
# automation, seamless failover, or WAL-style recovery maturity.
# Section 36 invariant: this gate can bound currently exercised planner or
# rewrite behavior, but it does not certify a mature optimizer stack or
# universal stable-plan identity.
# Section 37 invariant: this gate can also bound currently exercised metadata
# and schema behavior, but it does not certify optimizer-grade statistics
# maturity, global metadata coherence, or universal concurrent DDL guarantees.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${REPO_DIR}/build"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RESULT_DIR="${SCRIPT_DIR}/results/${RUN_ID}"
LOG_DIR="${RESULT_DIR}/logs"
mkdir -p "${LOG_DIR}"

EXAMPLE_ENV_FILE="${SCRATCHBIRD_EXAMPLE_ENV_FILE:-/tmp/scratchbird-example-dynamic/profiles/runtime.env}"

EXAMPLE_MANAGER="${REPO_DIR}/tests/compatibility/scripts/manage_example_db.sh"
REFRESH_FIXTURE_PER_SCRIPT_STEP="${SCRATCHBIRD_REQUIRED_GATE_REFRESH_FIXTURE_PER_SCRIPT_STEP:-1}"
INITIAL_FIXTURE_REFRESH="${SCRATCHBIRD_REQUIRED_GATE_INITIAL_FIXTURE_REFRESH:-0}"

declare -A CATEGORY_PASS=(
  ["wire_protocol"]=0
  ["transaction_semantics"]=0
  ["security_enforcement"]=0
  ["end_to_end_sql"]=0
  ["modal_nosql"]=0
  ["cluster_infra"]=0
)

declare -A CATEGORY_FAIL=(
  ["wire_protocol"]=0
  ["transaction_semantics"]=0
  ["security_enforcement"]=0
  ["end_to_end_sql"]=0
  ["modal_nosql"]=0
  ["cluster_infra"]=0
)

reload_example_env() {
  if [[ -f "${EXAMPLE_ENV_FILE}" ]]; then
    # shellcheck disable=SC1090
    source "${EXAMPLE_ENV_FILE}"
  fi
}

reload_example_env

retry_after_fixture_transport_failure() {
  local log_file="$1"
  if [[ ! -f "${log_file}" ]]; then
    return 1
  fi
  if ! rg -q \
      "Connection failed: recv\\(\\) failed: Connection reset by peer|Connection refused|not reachable with current client/auth settings|not reachable with configured profile" \
      "${log_file}"; then
    return 1
  fi
  if [[ ! -x "${EXAMPLE_MANAGER}" ]]; then
    return 1
  fi
  SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE="${SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE:-0}" \
    "${EXAMPLE_MANAGER}" dynamic-setup > "${RESULT_DIR}/fixture_repair.log" 2>&1 || true
  reload_example_env
  return 0
}

refresh_fixture() {
  if [[ ! -x "${EXAMPLE_MANAGER}" ]]; then
    return 0
  fi
  SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE="${SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE:-0}" \
    "${EXAMPLE_MANAGER}" dynamic-setup > "${RESULT_DIR}/fixture_refresh.log" 2>&1 || true
  reload_example_env
}

if [[ "${INITIAL_FIXTURE_REFRESH}" == "1" ]]; then
  refresh_fixture
fi

run_step() {
  local category="$1"
  local step_id="$2"
  local retries="$3"
  shift 3

  local log_file="${LOG_DIR}/${step_id}.log"
  local attempt=1
  local ok=0

  while [[ "${attempt}" -le "${retries}" ]]; do
    {
      echo "[required-gate] utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
      echo "[required-gate] category=${category}"
      echo "[required-gate] step=${step_id}"
      echo "[required-gate] attempt=${attempt}/${retries}"
      echo "[required-gate] command=$*"
      echo
      "$@"
    } > "${log_file}" 2>&1 && {
      ok=1
      break
    }

    if [[ "${attempt}" -lt "${retries}" ]]; then
      # The required-gate script steps share a dynamic example fixture. A failed
      # attempt can leave that fixture in a listener-only or parser-depleted
      # state even when the nested compatibility script never emits a classic
      # transport failure into the outer step log. Always refresh before the
      # final retry so the second attempt does not inherit stale control
      # sockets, parser pools, or an exited example engine. Preserve the
      # transport-specific repair hook as an additional signal-driven refresh.
      retry_after_fixture_transport_failure "${log_file}" || true
      refresh_fixture || true
      attempt=$((attempt + 1))
      continue
    fi
    break
  done

  if [[ "${ok}" -eq 1 ]]; then
    CATEGORY_PASS["${category}"]=$((CATEGORY_PASS["${category}"] + 1))
    printf 'STEP_RESULT|%s|%s|PASS|%s\n' "${category}" "${step_id}" "${log_file}" >> "${RESULT_DIR}/step_results.txt"
    return 0
  fi

  CATEGORY_FAIL["${category}"]=$((CATEGORY_FAIL["${category}"] + 1))
  printf 'STEP_RESULT|%s|%s|FAIL|%s\n' "${category}" "${step_id}" "${log_file}" >> "${RESULT_DIR}/step_results.txt"
  return 1
}

run_ctest_exact() {
  local category="$1"
  local step_id="$2"
  local test_name="$3"
  local retries="${4:-1}"
  run_step "${category}" "${step_id}" "${retries}" \
    ctest --test-dir "${BUILD_DIR}" -R "^${test_name}$" --output-on-failure
}

run_script_step() {
  local category="$1"
  local step_id="$2"
  local retries="${3:-1}"
  local script_path="$4"
  local refresh_before="${5:-0}"
  if [[ "${REFRESH_FIXTURE_PER_SCRIPT_STEP}" == "1" || "${refresh_before}" == "1" ]]; then
    refresh_fixture
  fi
  run_step "${category}" "${step_id}" "${retries}" bash "${script_path}"
}

# ------------------------------------------------------------------------------
# Category 1 + 5: wire protocol correctness + end-to-end SQL correctness
# ------------------------------------------------------------------------------
run_script_step "wire_protocol" "compat_postgresql" 2 "${REPO_DIR}/tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh" || true
run_script_step "wire_protocol" "compat_mysql" 2 "${REPO_DIR}/tests/compatibility/mysql/scripts/run_mysql_ctest.sh" || true
run_script_step "wire_protocol" "compat_firebird" 2 "${REPO_DIR}/tests/compatibility/firebird/scripts/run_firebird_ctest.sh" || true
run_ctest_exact "wire_protocol" "pg_frame_conformance" "PGFrameConformance" 1 || true
run_ctest_exact "wire_protocol" "mysql_frame_conformance" "MySQLFrameConformance" 1 || true
run_ctest_exact "wire_protocol" "firebird_frame_conformance" "FirebirdFrameConformance" 1 || true
run_ctest_exact "wire_protocol" "generic_protocol_frame_conformance" "ProtocolFrameConformance" 1 || true

run_script_step "end_to_end_sql" "compat_scratchbird_native" 2 "${REPO_DIR}/tests/compatibility/scratchbird/scripts/run_scratchbird_native_ctest.sh" || true
run_script_step "end_to_end_sql" "v3_native_inet_suite" 2 "${REPO_DIR}/tests/conformance/v3_native_inet/run_v3_native_inet_ctest.sh" 1 || true

# ------------------------------------------------------------------------------
# Category 2: transaction boundaries + error semantics
# ------------------------------------------------------------------------------
run_script_step "transaction_semantics" "transaction_truth_matrix" 2 "${REPO_DIR}/tests/conformance/transactions/run_transaction_truth_matrix.sh" 1 || true
run_ctest_exact "transaction_semantics" "transaction_truth_native" "TransactionTruthNative" 1 || true
run_ctest_exact "transaction_semantics" "mga_basic_update" "MGABackVersioningTest.BasicUpdate" 1 || true
run_ctest_exact "transaction_semantics" "mga_mvcc_visibility" "MGABackVersioningTest.MVCCVisibilityAcrossVersions" 1 || true
run_ctest_exact "transaction_semantics" "storage_tx_backversion" "StorageTransactionTest.UpdateCreatesBackVersion" 1 || true
run_ctest_exact "transaction_semantics" "txn_startup_failpoint_replay" "MgaFailpointReplayStandaloneTest.StartupFailpointIsReplayable" 1 || true
run_ctest_exact "transaction_semantics" "txn_commit_pre_tip_restart" "MgaFailpointReplayTest.CommitPreTipFailpointAbortsInsertedRowAcrossUncleanRestart" 1 || true
run_ctest_exact "transaction_semantics" "txn_commit_post_tip_restart" "MgaFailpointReplayTest.CommitPostTipFailpointKeepsInsertedRowCommittedAcrossRestart" 1 || true
run_ctest_exact "transaction_semantics" "txn_prepare_catalog_restart" "MgaFailpointReplayTest.PrepareCatalogOnlyFailpointPromotesToPreparedAcrossRestart" 1 || true
run_ctest_exact "transaction_semantics" "txn_writeback_diskfull_fence" "ExecutorTransactionPayloadTest.SyncDiskFullPersistsWritebackFenceAndBlocksGrowth" 1 || true
run_ctest_exact "transaction_semantics" "txn_commit_fence_rejects_open_incident" "ExecutorTransactionPayloadTest.CommitFenceRejectsWhileWritebackIncidentIsOpen" 1 || true
run_ctest_exact "transaction_semantics" "txn_restart_reloads_write_fence" "ExecutorTransactionPayloadTest.ReopenReloadsWritebackFenceUntilIncidentClears" 1 || true
run_ctest_exact "transaction_semantics" "txn_cleanup_blocked_recovery" "ExecutorTransactionPayloadTest.StartupReconciliationCapturesCleanupBlockedChainFindings" 1 || true
run_ctest_exact "transaction_semantics" "txn_checksum_quarantine_recovery" "ExecutorTransactionPayloadTest.StartupCorruptionPolicyQuarantinesChecksumCorruption" 1 || true
run_ctest_exact "transaction_semantics" "txn_copy_write_fence_boundary" "CopyExecutorTest.CopyFromPreservesWriteFenceStatusAtExecutorBoundary" 1 || true
run_ctest_exact "transaction_semantics" "txn_sweep_resume_generation_reuse" "GarbageCollectorTest.SweepResumeStateSurvivesRestartAndReusesGeneration" 1 || true
run_ctest_exact "transaction_semantics" "txn_sweep_dirty_restart_rewind" "GarbageCollectorTest.SweepDirtyRestartRewindsPersistedCursorAndStartsFreshGeneration" 1 || true
run_ctest_exact "transaction_semantics" "txn_durability_observability_surface" "MgaObservabilityLiveViewsTest.BuildsDurabilityRowsFromCatalogHistoryAndRuntimeState" 1 || true

# ------------------------------------------------------------------------------
# Memory-model section 31 expansion (G6-BC / G7-BC)
# ------------------------------------------------------------------------------
# Audit contract:
# - These steps make the segmented-memory architecture explicit in the
#   required public beta gate instead of relying on indirect coverage through
#   broader transaction or storage suites.
# - The gate names the canonical memory-model proof surfaces directly:
#   config loading, admission/ghost reuse, domain isolation, prefetch fairness,
#   checkpoint-bound dirty handling, restart queue rebuild, ownership transfer,
#   SQL-visible observability, and the scan-resistance benchmark.
# - TSAN ownership coverage remains required under the sanitizer lifecycle, but
#   it is not part of the default public-beta build because that artifact is
#   only registered in sanitizer builds.
run_ctest_exact "transaction_semantics" "memory_domain_config_surface" "HotPathRuntimeFixture.DatabaseOpenLoadsCanonicalSegmentedBufferDomainControls" 1 || true
run_ctest_exact "transaction_semantics" "memory_admission_second_touch" "BufferPoolMgaPolicyTest.DefaultDemandReadEntersProbationaryAndSecondTouchPromotesToProtected" 1 || true
run_ctest_exact "transaction_semantics" "memory_ghost_history_reuse" "BufferPoolMgaPolicyTest.GhostHistoryPromotesReloadedPageBackToProtectedResidency" 1 || true
run_ctest_exact "transaction_semantics" "memory_domain_isolation_version_undo" "BufferPoolMgaPolicyTest.SweepGcVersionUndoPagesStayProtectedOutsideTheScanRing" 1 || true
run_ctest_exact "transaction_semantics" "memory_domain_reservation_floor" "BufferPoolMgaPolicyTest.HardReservedDomainsDoNotEvictAtMinimumReservation" 1 || true
run_ctest_exact "transaction_semantics" "memory_prefetch_fairness_budget" "BufferPoolMgaPolicyTest.PrefetchDebtCapsSpeculativeAdmissionsAndCancelsExcessWork" 1 || true
run_ctest_exact "transaction_semantics" "memory_prefetch_thrash_guard" "BufferPoolMgaPolicyTest.UsefulnessCollapseCancelsFurtherSpeculativePrefetch" 1 || true
run_ctest_exact "transaction_semantics" "memory_checkpoint_capture_debt" "ExecutorTransactionPayloadTest.CleanShutdownCheckpointControlCapturesDirtyGenerationBoundariesAndDebt" 1 || true
run_ctest_exact "transaction_semantics" "memory_queue_rebuild_restart" "ExecutorTransactionPayloadTest.RestartQueueRebuildSeedsDirtyGenerationFloorAndRepublishesCheckpointDebt" 1 || true
run_ctest_exact "transaction_semantics" "memory_partition_ownership_transfer" "SegmentedOwnershipConcurrencyTest.SegmentedMissTransfersDonorFreeFramesToUnderprovisionedPartitions" 1 || true
run_ctest_exact "transaction_semantics" "memory_buffer_policy_observability" "MgaObservabilityLiveViewsTest.BuildsLiveBufferPolicyRowsFromSegmentedSnapshots" 1 || true
run_ctest_exact "transaction_semantics" "memory_scan_resistance_benchmark" "CacheBufferBenchmarkTest.ScanResistanceBenchmark" 1 || true
run_ctest_exact "transaction_semantics" "memory_mixed_workload_benchmark" "StoragePerformanceTest.MixedWorkloadBenchmark" 1 || true

# ------------------------------------------------------------------------------
# Category 3: security runtime enforcement
# ------------------------------------------------------------------------------
run_script_step "security_enforcement" "security_parity_matrix" 2 "${REPO_DIR}/tests/conformance/security/run_security_parity_matrix.sh" 1 || true
run_ctest_exact "security_enforcement" "security_phase3_column" "SecurityPhase3_3_ColumnPermissions" 1 || true
run_ctest_exact "security_enforcement" "security_phase3_rls_dml" "SecurityPhase3_5_RLS_DML" 1 || true
run_ctest_exact "security_enforcement" "domain_security" "DomainSecurity" 1 || true
run_ctest_exact "security_enforcement" "domain_encryption" "DomainEncryption" 1 || true
run_ctest_exact "security_enforcement" "domain_e2e" "DomainE2EScenarios" 1 || true
run_ctest_exact "security_enforcement" "audit_compliance_trail" "AuditLoggerTest.ComplianceAuditTrail" 1 || true
run_ctest_exact "security_enforcement" "auth_policy_protocol_parity" "AuthPolicyProtocolParityTest.StrictScramPolicyAllowsAllProtocolProfiles" 1 || true

# ------------------------------------------------------------------------------
# Category 8: modal / NoSQL support and infrastructure
# ------------------------------------------------------------------------------
run_ctest_exact "modal_nosql" "parser_search_dsl_surface" "ParserV3NativeExtensionSurfaceTest.ParsesSearchDslStatementAndClauseForms" 1 || true
run_ctest_exact "modal_nosql" "parser_search_dsl_negative" "ParserV3NativeExtensionSurfaceTest.RejectsInvalidSearchDslScorer" 1 || true
run_ctest_exact "modal_nosql" "parser_vector_surface" "ParserV3NativeExtensionSurfaceTest.ParsesCanonicalVectorAnnStatementForm" 1 || true
run_ctest_exact "modal_nosql" "parser_vector_negative" "ParserV3NativeExtensionSurfaceTest.RejectsInvalidVectorAnnMetric" 1 || true
run_ctest_exact "modal_nosql" "parser_redis_surface" "ParserV3NativeExtensionSurfaceTest.ParsesCanonicalRedisKvAndStreamSurfaces" 1 || true
run_ctest_exact "modal_nosql" "parser_redis_negative" "ParserV3NativeExtensionSurfaceTest.RejectsRemovedRedisAliasSurfaces" 1 || true
run_ctest_exact "modal_nosql" "nosql_emitter_mapping" "ParserV3NoSqlEmitterContractTest.MapsCanonicalRedisKvAndStreamCommandsToBridgeOpcodes" 1 || true
run_ctest_exact "modal_nosql" "nosql_emitter_alias_reject" "ParserV3NoSqlEmitterContractTest.RejectsRemovedEnginePrefixedAliasesBeforeEmission" 1 || true
run_ctest_exact "modal_nosql" "nosql_virtual_catalog_contract" "VirtualCatalogCanonicalBackingContractTest.RedisCommandsUseCanonicalParserCapabilities" 1 || true

# ------------------------------------------------------------------------------
# Category 9: cluster support and infrastructure
# ------------------------------------------------------------------------------
run_ctest_exact "cluster_infra" "cluster_fencing_term" "ClusterWriteFencingTest.StaleLeaderWritesAreRejectedByTokenAndLeaderIdentity" 1 || true
run_ctest_exact "cluster_infra" "cluster_fencing_epoch" "ClusterWriteFencingTest.RoutingEpochMustMatchPinnedWritePathEpoch" 1 || true
run_ctest_exact "cluster_infra" "cluster_identity_default" "DatabaseClusterIdentityTest.DefaultsToStandaloneIdentity" 1 || true
run_ctest_exact "cluster_infra" "cluster_identity_persist" "DatabaseClusterIdentityTest.PersistsClusterIdentityAcrossRestart" 1 || true
run_ctest_exact "cluster_infra" "cluster_replication_pipeline" "FollowerApplyPipelineTest.InOrderApplyUpdatesReplicationWatermark" 1 || true
run_ctest_exact "cluster_infra" "cluster_replication_conflict_contracts" "CatalogReplicationRuntimeConflictExtensionContractTest.ReplicationRuntimeConflictCatalogContracts" 1 || true
run_ctest_exact "cluster_infra" "cluster_observability_rows" "SqlObservabilityViewBuilderTest.BuildsClusterShardAndSnapshotRows" 1 || true
run_ctest_exact "cluster_infra" "cluster_parser_control_surface" "ParserV3NativeExtensionSurfaceTest.ParsesAdminClusterAndServiceControlSurfaces" 1 || true
run_ctest_exact "cluster_infra" "cluster_parser_replication_surface" "ParserV3NativeExtensionSurfaceTest.ParsesReplicationChannelAndResyncSurfaces" 1 || true
run_ctest_exact "cluster_infra" "cluster_parser_replication_negative" "ParserV3NativeExtensionSurfaceTest.RejectsInvalidReplicationChannelDirectionSurfaces" 1 || true

{
  for category in wire_protocol transaction_semantics security_enforcement end_to_end_sql modal_nosql cluster_infra; do
    printf 'CATEGORY_SUMMARY|%s|%s|%s\n' \
      "${category}" "${CATEGORY_PASS[${category}]}" "${CATEGORY_FAIL[${category}]}"
  done
} > "${RESULT_DIR}/category_summary.txt"

{
  echo "# Required Public Beta Gate"
  echo
  echo "- Generated (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "- Result directory: ${RESULT_DIR}"
  echo
  echo "## Category Summary"
  echo
  echo "| Category | Passed Steps | Failed Steps |"
  echo "|---|---:|---:|"
  while IFS='|' read -r _ category pass fail; do
    echo "| \`${category}\` | ${pass} | ${fail} |"
  done < "${RESULT_DIR}/category_summary.txt"
  echo
  echo "## Step Results"
  echo
  echo "| Category | Step | Result | Log |"
  echo "|---|---|---|---|"
  while IFS='|' read -r _ category step result log; do
    echo "| \`${category}\` | \`${step}\` | \`${result}\` | \`${log}\` |"
  done < "${RESULT_DIR}/step_results.txt"
} > "${RESULT_DIR}/SUMMARY.md"

cat "${RESULT_DIR}/category_summary.txt"

total_fail=0
for category in wire_protocol transaction_semantics security_enforcement end_to_end_sql modal_nosql cluster_infra; do
  total_fail=$((total_fail + CATEGORY_FAIL["${category}"]))
done

echo "RESULT_DIR=${RESULT_DIR}"
if [[ "${total_fail}" -ne 0 ]]; then
  exit 1
fi
