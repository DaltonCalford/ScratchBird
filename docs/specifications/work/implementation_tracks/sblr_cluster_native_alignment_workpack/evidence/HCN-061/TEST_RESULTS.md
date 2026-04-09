# Test Results - HCN-061

Commands:
```bash
build/tests/scratchbird_tests --gtest_filter='SblrJitFixture.jit_performance_envelope:SblrJitFixture.jit_soak_mixed_policy_no_semantic_divergence:SblrJitFixture.jit_queue_stress_retains_vm_correctness:SblrJitFixture.jit_tiering_hotness_threshold_promotion_queues_compile:SblrJitFixture.jit_tiering_queue_saturation_retains_deterministic_vm_execution:FollowerApplyPipelineTest.*:CommittedWatermarkPublisherTest.*:TelemetryReportingContractTest.*'
build/tests/scratchbird_tests --gtest_filter='ClusterWriteFencingTest.*:DeterministicShardRouterTest.*:SessionEpochPinsTest.*:MultiShardWriteGuardTest.*:GtxidOrderingTest.*:ShardCommitLogPipelineTest.*:FollowerApplyPipelineTest.*:SnapshotRegistryTest.*:CommittedWatermarkPublisherTest.*:GcSafeHorizonCalculatorTest.*:DomainControlPlaneReplicaCatalogTest.*:MetricContractPolicyTest.*:SqlObservabilityViewBuilderTest.*:HealthReadinessContractTest.*:StructuredEventStreamTest.*:SblrJitFixture.*:LanguageUdrCompileApiMatrixContractTest.*:CatalogSblrArtifactExtensionContractTest.*:TelemetryReportingContractTest.*'
```

Results:
- Perf/latency subset: 12 passed, 0 failed.
- Extended regression subset: 74 passed, 0 failed.

Log references:
- `/tmp/hcn061_tests.log`
- `/tmp/hcn_full_regression.log`
