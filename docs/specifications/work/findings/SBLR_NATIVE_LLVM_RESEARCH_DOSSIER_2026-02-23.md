# SBLR Native Compilation Research Dossier

Date: 2026-02-23
Goal: Provide implementation research inputs for a hybrid SBLR-native model where SBLR is canonical and native is optional optimization.

## LLVM Research Findings

### ORC/LLJIT is the correct baseline
- LLJIT is a pre-fabricated ORC JIT stack and modern alternative to MCJIT-like usage.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/LLJIT.h:35`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/LLJIT.h:133`

### On-demand/lazy compilation is directly supported
- LLVM provides `CompileOnDemandLayer` for function-level lazy compile behavior.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h:9`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h:51`

### Thread-safe module handling is first-class
- `ThreadSafeModule` and `ThreadSafeContext` support concurrent JIT pipelines safely.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/ThreadSafeModule.h:24`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/ThreadSafeModule.h:66`

### Target portability controls are available
- `JITTargetMachineBuilder` supports target triple, CPU features, relocation/code model, and opt levels.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h:31`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h:74`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h:103`

### Object cache supports predefined/native reuse
- LLVM `ObjectCache` exists specifically to avoid recompiling already-built modules.
- LLJIT object-cache example demonstrates practical usage.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/ObjectCache.h:19`
  - `/home/dcalford/CliWork/llvm-project/llvm/examples/OrcV2Examples/LLJITWithObjectCache/LLJITWithObjectCache.cpp:22`
  - `/home/dcalford/CliWork/llvm-project/llvm/examples/OrcV2Examples/LLJITWithObjectCache/LLJITWithObjectCache.cpp:52`

### C API path exists (but stability caveat)
- `llvm-c/LLJIT.h` provides C bindings and can reduce C++ ABI exposure.
- Header explicitly marks interface as experimental/not stable.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm-c/LLJIT.h:9`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm-c/LLJIT.h:16`

### MCJIT should not be new baseline
- ExecutionEngine APIs include MCJIT-era methods with deprecation notes for certain interfaces.
- Recommendation: build on ORC/LLJIT, not legacy MCJIT patterns.
- Proof:
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/ExecutionEngine.h:237`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/ExecutionEngine.h:347`
  - `/home/dcalford/CliWork/llvm-project/llvm/include/llvm/ExecutionEngine/ExecutionEngine.h:357`

## ScratchBird Research Findings Relevant to Hybrid Model

1. Native artifact catalogs already exist (`sblr_module`, `sblr_plan`, `sblr_artifact`, `sblr_compile_queue`).
   - `include/scratchbird/core/catalog_manager.h:2608`
   - `src/core/catalog_manager.cpp:90511`
2. Parser/emitter coverage for cluster control and many vNext families exists.
   - `src/parser/parser_v3.cpp:15418`
   - `src/parser/v3_emitter.cpp:2801`
3. Executor still uses bridge-reject closure for broad vNext families including cluster control.
   - `src/sblr/executor.cpp:65856`

## Cross-Engine Concept Inputs (Local Clone Study)

These are design inspirations, not direct behavioral replacements:

- Firebird: MGA read consistency and GC horizon discipline for snapshot safety.
- PostgreSQL: replication-slot style retention concept for lag-aware data retention controls.
- Cassandra: strict partition-scoped atomicity and routing by key.
- MySQL Group Replication: single-primary discipline and failover safety checks.

Use these as implementation design checks, while preserving ScratchBird invariants:
- SBLR remains canonical.
- MGA semantics remain authoritative.
- No WAL-centric Alpha recovery replacement.

## Recommended Architecture for ScratchBird Hybrid Native

### Runtime services
1. `NativeCompilerService`
   - Input: canonical SBLR + target profile.
   - Output: artifact row + binary blob + diagnostics.
2. `NativeArtifactSelector`
   - Enforces compatibility key match and policy resolution.
3. `NativeExecutionBridge`
   - Invokes native artifact with deterministic fallback/deopt behavior.
4. `JitQueueService` (disabled by default)
   - Asynchronous compile queue with hotness threshold and deny hints.

### Policy model
- Object policy: `OFF | PREFER | REQUIRE`.
- Runtime mode: `INTERPRETED_ONLY | PREFER_NATIVE | REQUIRE_NATIVE`.
- Compile mode: `EXPLICIT_ONLY` first, then optional `JIT_ALLOWED`.

### Artifact key (minimum)
- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- ABI version
- compiler identity/version
- optimization profile
- security policy version

### Safety controls
- Never bypass VM path.
- Any mismatch/trap/deopt must emit reason code and fallback to VM (unless `REQUIRE_NATIVE`).
- Preserve lock order, MGA visibility, and GC constraints identically between VM/native execution.

## Weaknesses and Risks
1. Runtime closure currently lags schema/parser scaffolding.
2. JIT can add tail latency if introduced before queue controls and suppression hints.
3. C API stability note means direct C API reliance needs version pinning and adapter isolation.

## Recommended Rollout
1. Explicit compile for functions first.
2. Expand to triggers/procedures.
3. Add package members.
4. Enable optional JIT only after latency and fallback metrics pass gates.
5. Keep JIT off by default until production SLO evidence is stable.
