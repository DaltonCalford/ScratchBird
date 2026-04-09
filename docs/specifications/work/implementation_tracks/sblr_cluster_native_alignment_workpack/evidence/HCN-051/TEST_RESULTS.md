# Test Results - HCN-051

Command:
```bash
build/tests/scratchbird_tests --gtest_filter='SblrJitFixture.*:CatalogSblrArtifactExtensionContractTest.*'
```

Results:
- PH5 focused suite: 40 passed, 0 failed.
- Artifact-key mismatch coverage passed:
  - `jit_artifact_key_target_triple_mismatch_forces_deopt`
  - `jit_artifact_key_cpu_profile_mismatch_forces_deopt`
  - `jit_artifact_key_native_abi_mismatch_forces_deopt`
  - `jit_artifact_key_compiler_identity_mismatch_forces_deopt`
  - `jit_artifact_key_compiler_version_mismatch_forces_deopt`
  - `jit_artifact_key_optimization_profile_mismatch_forces_deopt`
  - `jit_artifact_non_ready_state_rejects_selection`
- `REQUIRE_NATIVE` negative tests passed:
  - `jit_fallback_require_native_errors_when_artifact_missing`
  - `jit_policy_require_native_without_artifact_returns_deterministic_error`

Log reference:
- `/tmp/hcn_ph5_tests3.log`
