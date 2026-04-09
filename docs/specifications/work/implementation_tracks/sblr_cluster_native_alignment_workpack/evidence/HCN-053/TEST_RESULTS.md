# Test Results - HCN-053

Command:
```bash
build/tests/scratchbird_tests --gtest_filter='SblrJitFixture.*:LanguageUdrCompileApiMatrixContractTest.*:CatalogSblrArtifactExtensionContractTest.*'
```

Results:
- PH5 focused suite: 40 passed, 0 failed.
- Target transition coverage passed:
  - `jit_rebase_cross_target_rebase_keeps_canonical_sblr_executable`
  - `jit_target_mismatch_cross_target_mismatch_falls_back_to_vm`

Log reference:
- `/tmp/hcn_ph5_tests3.log`
