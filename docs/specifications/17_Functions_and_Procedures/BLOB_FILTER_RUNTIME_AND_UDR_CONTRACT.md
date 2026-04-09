# Blob Filter Runtime and UDR Contract

## Capability vocabulary used in this section
- `supported`
- `bounded`
- `unproven`
- `fail_closed`

## Stored routine authority

### Capability states
- function catalog registration: `supported`
- procedure catalog registration: `supported`
- function or procedure lookup: `supported`
- function or procedure listing: `supported`
- procedure execution: `supported`
- function execution by loaded metadata: `supported`
- routine parser front doors: `supported`
- routine DDL dispatch: `bounded`
- overload or polymorphic dispatch matrix: `unproven`

### Current code-backed truth
Audited anchors:
- `catalog_manager.cpp:31082` `registerFunction(...)`
- `catalog_manager.cpp:31167` `registerProcedure(...)`
- `catalog_manager.cpp:31252` `getFunction(...)`
- `catalog_manager.cpp:31286` `getProcedure(...)`
- `catalog_manager.cpp:31434` `listFunctions(...)`
- `catalog_manager.cpp:31450` `listProcedures(...)`
- `catalog_manager.cpp:41062` `writeProcedureRecord(...)`
- `catalog_manager.cpp:41484` `readProcedureRecords(...)`
- `executor.cpp:44245` `executeProcedure()`
- `executor.cpp:93911` `callProcedureByName(...)`
- `executor.cpp:94544` `callFunctionByInfo(...)`
- `executor.cpp:96192` bounded routine DDL registration or alter flow
- `parser_v3.cpp:2030`
- `parser_v3.cpp:8271`

Current guaranteed behavior in this pass:
- routines require compiled bytecode for registration
- source text and execution payloads are persisted with catalog-backed storage
- parameter records are persisted and replaced during routine replacement
- routine execution enforces `EXECUTE` permission
- routine execution applies `SQL SECURITY DEFINER` or `INVOKER`
- procedure and function argument binding relies on runtime conversion rather than a separate routine-local type system

### Fail-closed boundary
Do not document one closed overload or polymorphic dispatch contract from the current routine anchors. This pass did not prove that matrix.

## Language UDR boundary

### Capability states
- module registration: `supported`
- module status transitions: `supported`
- active-module resolution: `supported`
- capability-hash consistency: `supported`
- feature-key enablement: `supported`
- compile preflight schema or sandbox or quota fencing: `supported`
- builtin emulation-package inventory: `supported`
- operator-managed package lifecycle: `unproven`
- full engine runtime parity matrix: `unproven`

### Current code-backed truth
Audited anchors:
- `language_udr_runtime.cpp:483`
- `language_udr_runtime.cpp:532`
- `language_udr_runtime.cpp:581`
- `language_udr_runtime.cpp:622`
- `language_udr_runtime.cpp:706`
- `language_udr_runtime.cpp:796`
- `emulation_package_manifest.cpp:69`
- `v3_opcodes.generated.h:757`
- `v3_opcodes.generated.h:758`
- `v3_emitter.cpp:697`
- `v3_emitter.cpp:722`
- `executor.cpp:77551`
- `executor.cpp:80910`
- `executor.cpp:92550`

Current guaranteed behavior in this pass:
- module registration requires complete metadata
- module-status transitions are fenced
- active-module selection checks profile id, profile version, semver compatibility, and capability-set hash
- compile requests are validated for payload schema, permission, sandbox policy, and quota limits
- builtin package manifests exist for `scratchbird`, `firebirdsql`, `postgresql`, and `mysql`

### Fail-closed boundary
Builtin package manifest presence does not prove one operator-managed package lifecycle or one closed runtime parity matrix across engines.

## Blob-filter catalog and runtime boundary

### Capability states
- blob-filter catalog row structure: `supported`
- blob-filter catalog upsert: `supported`
- blob-filter catalog get: `supported`
- blob-filter catalog list: `supported`
- blob-filter catalog delete: `supported`
- blob-filter runtime lookup: `unproven`
- blob-filter runtime invocation: `unproven`
- blob-filter streaming-chain support: `unproven`
- blob-filter runtime incident surface: `unproven`

### Current code-backed truth
Audited anchors:
- `catalog_manager.h:2571` `BlobFilterCatalogInfo`
- `catalog_manager.h:10005` `upsertBlobFilterCatalogEntry(...)`
- `catalog_manager.h:10007` `getBlobFilterCatalogEntry(...)`
- `catalog_manager.h:10010` `listBlobFilterCatalogEntries(...)`
- `catalog_manager.h:10012` `deleteBlobFilterCatalogEntry(...)`
- `catalog_manager.cpp:96139` `upsertBlobFilterCatalogEntry(...)`
- `catalog_manager.cpp:96230` `getBlobFilterCatalogEntry(...)`
- `catalog_manager.cpp:96258` `listBlobFilterCatalogEntries(...)`
- `catalog_manager.cpp:96281` `deleteBlobFilterCatalogEntry(...)`

Current guaranteed behavior in this pass:
- blob-filter catalog rows can be created, read, listed, and invalidated
- blob-filter rows validate owner presence and identifier uniqueness
- blob-filter rows carry subtype, entry-point, and module metadata

### Fail-closed boundary
Do not document runtime lookup, execution dispatch, streaming, or operator-visible runtime diagnostics for blob filters until a later code-backed audit proves them.
