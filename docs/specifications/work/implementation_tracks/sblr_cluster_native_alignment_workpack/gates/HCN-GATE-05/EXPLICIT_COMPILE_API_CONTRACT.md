# Explicit Compile API Contract - HCN-050

Contract implemented in `JitRuntime::compileExplicit`:
1. Request surface MUST be one of FUNCTION/TRIGGER/PROCEDURE/PACKAGE_MEMBER.
2. Canonical SBLR payload MUST be non-empty.
3. `object_uuid`, `module_id`, and `plan_id` MUST be non-zero UUIDs.
4. Compatibility profile MUST include object UUID, canonical hash, target triple, ABI version, compiler identity/version, and optimization profile.
5. On compile backend unsupported-opcode response, status maps to `NOT_SUPPORTED`.
6. On backend unavailable, status maps to `NOT_IMPLEMENTED`.
7. On compile failure, status maps to `INTERNAL_ERROR`.

Verification:
- Scope rejection contract test: `jit_scope_compile_explicit_rejects_non_routine_surfaces`.
- Unsupported opcode mapping test: `jit_tiering_unsupported_opcode_family_forces_vm_fallback`.
