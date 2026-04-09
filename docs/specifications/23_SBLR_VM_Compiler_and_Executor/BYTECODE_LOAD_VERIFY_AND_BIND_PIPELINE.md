# Bytecode Load Verify and Bind Pipeline

Status: current_authority

## Current authority

- `include/scratchbird/sblr/query_compiler_v3.h`
- `src/sblr/query_compiler_v3_optimizer_support.cpp`
- `src/sblr/bytecode_validator.cpp`
- section `22` `SBL3` transport and validator surfaces

## Canonical compile pipeline

The authoritative native compile path is `QueryCompilerV3::compileInternal(...)`.

Algorithm:
1. Require a live `Database*`; otherwise compilation fails immediately.
2. Parse SQL with parser V3.
3. If parse fails, return line and column addressed parse errors.
4. Emit canonical SBLR V3 container using `V3Emitter`.
5. Set emitted module metadata `module_name` to `scratchbird_native`.
6. Call `detail::finalizeQueryCompilerV3Compilation(...)` with:
   - database
   - original SQL text
   - parsed statement
   - parser string pool
   - current schema
   - optimization-enabled flag
   - mutable V3 container
   - optional parameter bindings
   - requested plan profile mode
7. Collect warnings.
8. If finalize reports errors, compilation fails.
9. On success, return final bytecode plus plan-profile metadata.
10. If compiler stats are enabled, record parser time and final bytecode size.

## Bind meaning in the current implementation

Bind is not a separate VM linker. Current bind behavior means:
1. plan-profile selection
2. schema-sensitive planning using current schema context
3. parameter-sensitive planning when bindings are supplied
4. plan-cache key derivation
5. final bytecode materialization
6. validator acceptance before execution

## Load and verify path

The authoritative validator bridge is `validateBytecode(...)`.

Rules:
1. V3 containers are detected by exact `SBL3` magic.
2. V3 containers are validated through `v3::validateContainerDetailed(...)`.
3. Validator failures surface stable validator codes, messages, opcode symbols where available, and instruction offsets where available.
4. Only validated bytecode may proceed into executor consumption.

## Negative requirements

- do not invent a separate bind phase outside `finalizeQueryCompilerV3Compilation(...)`
- do not execute bytecode before validator acceptance
- treat missing database context as a hard compile failure
- keep parse failures distinct from validator or executor failures
