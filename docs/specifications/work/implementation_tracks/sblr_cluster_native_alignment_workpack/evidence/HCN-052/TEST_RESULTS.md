# Test Results - HCN-052

Commands:
```bash
build/tests/scratchbird_tests --gtest_filter='SblrJitFixture.*:LanguageUdrCompileApiMatrixContractTest.*:CatalogSblrArtifactExtensionContractTest.*'
build/tests/scratchbird_tests --gtest_filter='SblrJitFixture.jit_performance_envelope:SblrJitFixture.jit_soak_mixed_policy_no_semantic_divergence:SblrJitFixture.jit_queue_stress_retains_vm_correctness:SblrJitFixture.jit_tiering_hotness_threshold_promotion_queues_compile:SblrJitFixture.jit_tiering_queue_saturation_retains_deterministic_vm_execution:FollowerApplyPipelineTest.*:CommittedWatermarkPublisherTest.*:TelemetryReportingContractTest.*'
```

Results:
- PH5 focused run: 40 passed, 0 failed.
- Perf/latency subset: 12 passed, 0 failed.
- Queue/hint test highlights passed:
  - `jit_tiering_hotness_threshold_promotion_queues_compile`
  - `jit_tiering_queue_saturation_retains_deterministic_vm_execution`
  - `jit_policy_hint_disable_compile_suppresses_queue_promotion`
  - `jit_policy_hint_disable_execute_forces_vm_path`

Log references:
- `/tmp/hcn_ph5_tests3.log`
- `/tmp/hcn061_tests.log`
