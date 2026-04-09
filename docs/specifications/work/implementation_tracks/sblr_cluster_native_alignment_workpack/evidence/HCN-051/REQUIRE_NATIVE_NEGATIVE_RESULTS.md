# REQUIRE_NATIVE Negative Results - HCN-051

Covered tests:
- `SblrJitFixture.jit_fallback_require_native_errors_when_artifact_missing`
- `SblrJitFixture.jit_policy_require_native_without_artifact_returns_deterministic_error`

Observed behavior:
- Runtime returns `JitDispatchOutcome::Path::ERROR`.
- Error reason code is `REQUIRE_NATIVE_NOT_AVAILABLE`.
- Behavior is deterministic across reruns in the PH5 focused suite.
