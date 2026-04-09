# Section 17 Outline

## 17.1 Stored routine catalog and execution authority
- function registration and replacement
- procedure registration and replacement
- routine lookup, listing, drop, and dependency handling
- executor routine invocation and permission enforcement
- `SQL SECURITY DEFINER` and `INVOKER` handling

## 17.2 Stored code persistence model
- shared procedure-record storage for procedures and functions
- TOAST-backed source, bytecode, and return-type payloads
- parameter record storage

## 17.3 Language UDR runtime boundary
- module registration and status transitions
- active-module resolution and capability hash checks
- compile preflight validation, sandbox checks, and quota fencing
- UDR compile opcode and dispatcher surfaces

## 17.4 Emulated-engine package inventory
- builtin parser packages
- builtin compiler-UDR packages
- builtin emulation-UDR packages
- current package inventory is builtin-manifest backed, not operator-managed in this section

## 17.5 Remote-engine connector boundary
- connector-side procedure metadata introspection
- remote connector catalog extensions and state models
- fail-closed boundary where broad runtime-orchestration guarantees are unproven

## 17.6 Blob-filter boundary
- blob-filter catalog rows are implemented
- runtime filter execution path is not proven in this audit pass

## 17.7 ScratchBird cluster-fabric boundary
- catalog rows and state enums are implemented
- live fabric transport or task execution guarantees remain unproven in this section

## 17.8 Test authority
- parser or emitter UDR compile tests
- executor routine and vnext dispatch tests
- remote connector factory and catalog extension tests
- blob-filter catalog extension tests
- cluster-fabric catalog extension tests
