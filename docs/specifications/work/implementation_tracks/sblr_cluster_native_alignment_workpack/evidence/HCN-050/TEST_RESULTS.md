# Test Results - HCN-050

Commands:
```bash
cmake --build build --target scratchbird_tests -j8
build/tests/scratchbird_tests --gtest_filter='SblrJitFixture.*:LanguageUdrCompileApiMatrixContractTest.*:CatalogSblrArtifactExtensionContractTest.*'
```

Results:
- Focused PH5 run: 40 passed, 0 failed.
- Explicit-scope coverage:
  - `SblrJitFixture.jit_scope_unknown_surface_never_enters_native_selection` passed.
  - `SblrJitFixture.jit_scope_compile_explicit_rejects_non_routine_surfaces` passed.
- Routine/member equivalence coverage:
  - `jit_functions_vm_native_equivalence` passed.
  - `jit_procedures_vm_native_equivalence` passed.
  - `jit_triggers_vm_native_equivalence` passed.
  - `jit_package_members_vm_native_equivalence` passed.

Log reference:
- `/tmp/hcn_ph5_tests3.log`
