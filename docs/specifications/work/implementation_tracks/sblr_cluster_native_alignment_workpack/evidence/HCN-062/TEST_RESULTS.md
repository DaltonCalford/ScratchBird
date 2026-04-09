# Test Results - HCN-062

Primary closure command:
```bash
build/tests/scratchbird_tests --gtest_filter='ClusterWriteFencingTest.*:DeterministicShardRouterTest.*:SessionEpochPinsTest.*:MultiShardWriteGuardTest.*:GtxidOrderingTest.*:ShardCommitLogPipelineTest.*:FollowerApplyPipelineTest.*:SnapshotRegistryTest.*:CommittedWatermarkPublisherTest.*:GcSafeHorizonCalculatorTest.*:DomainControlPlaneReplicaCatalogTest.*:MetricContractPolicyTest.*:SqlObservabilityViewBuilderTest.*:HealthReadinessContractTest.*:StructuredEventStreamTest.*:SblrJitFixture.*:LanguageUdrCompileApiMatrixContractTest.*:CatalogSblrArtifactExtensionContractTest.*:TelemetryReportingContractTest.*'
```

Result:
- 74 tests ran.
- 74 passed, 0 failed.

Log reference:
- `/tmp/hcn_full_regression.log`
