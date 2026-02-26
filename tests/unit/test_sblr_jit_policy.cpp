#include "test_sblr_jit_test_utils.h"

namespace scratchbird::sblr::jit::test
{
    TEST_F(SblrJitFixture, jit_policy_resolver_applies_strictest_compile_and_execution_controls)
    {
        JitRuntime runtime(db_->catalog_manager());
        JitPolicyEnvelope policy{};
        policy.database_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        policy.database_execution_policy = JitExecutionPolicy::INTERPRETED_ONLY;
        policy.session_compile_mode = JitCompileMode::JIT_ALLOWED;
        policy.session_execution_policy = JitExecutionPolicy::REQUIRE_NATIVE;
        policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;

        const JitEffectivePolicy effective = runtime.resolvePolicy(policy);
        EXPECT_EQ(effective.compile_mode, JitCompileMode::EXPLICIT_ONLY);
        EXPECT_EQ(effective.execution_policy, JitExecutionPolicy::INTERPRETED_ONLY);
    }

    TEST_F(SblrJitFixture, jit_policy_resolver_escalates_to_require_native_when_not_interpreted_only)
    {
        JitRuntime runtime(db_->catalog_manager());
        JitPolicyEnvelope policy{};
        policy.database_compile_mode = JitCompileMode::JIT_ALLOWED;
        policy.database_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        policy.session_compile_mode = JitCompileMode::JIT_ALLOWED;
        policy.session_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        policy.object_execution_policy = JitExecutionPolicy::REQUIRE_NATIVE;

        const JitEffectivePolicy effective = runtime.resolvePolicy(policy);
        EXPECT_EQ(effective.compile_mode, JitCompileMode::JIT_ALLOWED);
        EXPECT_EQ(effective.execution_policy, JitExecutionPolicy::REQUIRE_NATIVE);
    }

    TEST_F(SblrJitFixture, jit_policy_hint_disable_compile_suppresses_queue_promotion)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        request.policy.hints.disable_compile = true;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::HINT_DISABLE_COMPILE);
        EXPECT_FALSE(outcome.compile_queued);
    }

    TEST_F(SblrJitFixture, jit_policy_hint_disable_execute_forces_vm_path)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_compile_mode = JitCompileMode::JIT_ALLOWED;
        request.policy.object_execution_policy = JitExecutionPolicy::PREFER_NATIVE;
        request.policy.hints.disable_execute = true;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::VM);
        EXPECT_EQ(outcome.reason, JitReasonCode::HINT_DISABLE_EXECUTE);
    }

    TEST_F(SblrJitFixture, jit_policy_require_native_without_artifact_returns_deterministic_error)
    {
        const auto binding = createModulePlanBinding();
        const auto canonical = compileCanonicalRoutine();
        auto request = makeRuntimeRequest(binding, canonical);
        request.policy.object_execution_policy = JitExecutionPolicy::REQUIRE_NATIVE;

        JitRuntime runtime(db_->catalog_manager());
        runtime.setCompileBackend(createLlvmBackend());
        core::ErrorContext ctx;
        const auto outcome = runtime.selectPath(request, &ctx);
        EXPECT_EQ(outcome.path, JitDispatchOutcome::Path::ERROR);
        EXPECT_EQ(outcome.reason, JitReasonCode::REQUIRE_NATIVE_NOT_AVAILABLE);
    }
}
