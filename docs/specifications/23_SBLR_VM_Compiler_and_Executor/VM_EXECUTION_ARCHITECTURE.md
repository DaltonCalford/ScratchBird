# VM Execution Architecture

Status: current_authority

## Current authority

Current implementation authority is an integrated compiler and executor path, not a separately proven universal VM subsystem.

Authoritative anchors:
- QueryCompilerV3 parse, emit, and finalize flow
- bytecode_validator.cpp container and validator bridge
- include/scratchbird/sblr/executor.h
- executor.cpp runtime-plan decode, explain, and execution integration
- query_compiler_v3_optimizer_support.cpp compiler-time optimizer and cache coordination

## Canonical execution architecture

The current execution architecture is:
1. parser or front-end builds canonical AST
2. emitter produces SBLR v3 container
3. finalize support attaches planning and plan-profile decisions
4. validator accepts or rejects bytecode
5. executor consumes validated bytecode through the canonical v3 path

The current architecture is not:
- a public standalone VM ABI
- a separately versioned vm_context contract
- a generic bytecode runtime independent from planner, catalog, and transaction context

## Executor front door

The authoritative executor entry surface is the current bytecode execute path.

Rules:
1. executor accepts SBLR v3 container input only on the canonical path
2. legacy stream input is retired as a first-class execution contract even though validator compatibility logic still exists
3. internal canonical execution path is executeCanonicalV3

## Execution state model

Current code-backed executor state includes:
- raw bytecode pointer and bytecode size
- program counter
- execution stack
- connection context
- result or error state

This proves the current model is an integrated bytecode interpreter and executor with direct catalog, connection, security, transaction, and runtime-plan access.

## Result contract

ExecutionResult is the authoritative execution return envelope.

Current code-backed result families:
- success without rowset
- error with Status and optional SQLSTATE
- result-set return

Rules:
1. execution failure is explicit and typed; it is not an implicit empty rowset
2. result-set return is a first-class success path
3. diagnostics remain coupled to execution result construction rather than an external VM status bus

## Statement dispatch model

The current executor is statement-family specific.

Current proven dispatch shape includes dedicated handlers for:
- table, index, schema, tablespace, domain, database, and rename and move DDL
- routine and security families
- show and admin families
- cursor and control-flow families
- job, policy, FDW, synonym, UDR, and comment families

This is the current authoritative model:
- decode instruction stream
- dispatch to statement-specific or expression-specific executor helpers
- interact directly with engine services such as catalog, locking, GC, security, and storage

No alternative message-passing VM microkernel is currently authoritative.

## Runtime-plan and optimizer integration

Runtime-plan payloads and explain-facing artifacts are first-class current evidence.

Rules:
1. the executor is integrated with the planner and runtime-plan layer, not isolated from it
2. plan cache, plan profile, schema epoch, and policy snapshot decisions are part of the current execution pipeline
3. JIT controls are optional execution policy overlays inside the same executor surface; they do not define a separate execution architecture

## Non-guarantees

- no stable standalone vm_module or vm_context ABI is defined here
- no universally audited register-machine contract is defined here
- no universal module or artifact binding metadata is claimed across all future statement families
