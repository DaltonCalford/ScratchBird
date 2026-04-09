# Emulated Engine Package Model

## Purpose
Define the canonical installable package model for every emulated engine family
so client-facing compatibility and engine-facing emulation support remain
outside the core engine.

## Scope
- All enabled emulated engine families.
- Package installation, activation, and readiness rules.
- Responsibility split between parser packages, compiler UDR packages, and
  emulation UDR packages.

## Hard Invariants
1. Core engine contains no parser implementation and no family-specific
   emulation logic beyond generic UDR hosting and SBLR execution.
2. Every emulated engine family is represented by three installable packages:
   - client-facing parser package
   - engine-facing compiler UDR package
   - engine-facing emulation UDR package
3. Full emulated-engine enablement requires:
   - parser package installed
   - compiler UDR package installed and `READY`
   - emulation UDR package installed and `READY`
   - compatible profile and package versions
   - configuration and catalog enablement for the family
4. Parser packages own protocol parsing, dialect normalization, capability
   gating, and wire-level error or response mapping only.
5. Compiler UDR packages own engine-facing dynamic SQL or command translation
   to SBLR and verification of those translations only.
6. Emulation UDR packages own family-specific bootstrap, overlay, bridge, and
   non-core runtime support operations only.
7. Parser packages, compiler UDR packages, and emulation UDR packages never
   bypass engine authority for authorization, catalog validation, or execution.

## Package Components

| Package Component | Location | Primary Responsibility | Forbidden Responsibility |
| --- | --- | --- | --- |
| Parser package | listener/parser worker side | client protocol, dialect grammar, AST normalization, UUID binding, SBLR emission for client requests | direct execution, direct storage access, direct legacy or remote engine access |
| Compiler UDR package | engine or IPC-server side | engine-origin dynamic SQL or command translation, canonical AST or SBLR artifact emission, translation verification | public listener protocol handling, direct client auth exchange, direct statement execution outside engine |
| Emulation UDR package | engine or IPC-server side | emulated database bootstrap, catalog overlays, family bridge client, migration helpers, family-owned non-core behaviors | public listener protocol handling, direct client auth exchange, direct statement execution outside engine |

## Activation States

| State | Parser Package | Compiler UDR Package | Emulation UDR Package | Config Enabled | Listener Exposure |
| --- | --- | --- | --- | --- | --- |
| `ABSENT` | no | no | no | no | forbidden |
| `PARTIAL_INSTALL` | any incomplete combination | any incomplete combination | any incomplete combination | optional | forbidden |
| `INSTALLED_DISABLED` | yes | yes | yes | no | forbidden |
| `ENABLED` | yes | yes (`READY`) | yes (`READY`) | yes | allowed |
| `DEGRADED` | any non-ready or version-skewed combination | any non-ready or version-skewed combination | any non-ready or version-skewed combination | any | forbidden |

Rules:
1. `ENABLED` is the only state that permits public listener startup for an
   emulated family.
2. Partial installs are valid staged-install states but are not serviceable
   emulation states.
3. Version mismatch between any bundle part forces `DEGRADED`.
4. Core engine must not silently substitute built-in emulation behavior when a
   package is incomplete.

## Canonical Request Paths

### Client-Facing Request
`client -> listener -> parser package -> sb_ipc_server -> engine -> executor`

### Engine-Facing Dynamic Request
`engine/system routine -> compiler UDR package -> canonical translation artifact -> engine -> executor`

### Engine-Facing Family Support Request
`engine/system routine -> emulation UDR package -> overlay/bootstrap/bridge operation -> engine`

Rules:
1. Engine-facing dynamic translation is allowed only through the family
   compiler UDR package.
2. The same capability profile and normalization rules must be used for parser
   and compiler-UDR translation for a given family and profile version.
3. Compiler UDR translation returns canonical artifacts to engine; it does not
   replace the parser package for client traffic.
4. Emulation UDR operations provide family-owned support behavior only; they do
   not replace parser or compiler responsibilities.

## Family-Owned Support Operations
The emulation UDR package is the authority for engine-facing family behaviors
that are not part of the generic engine core, including but not limited to:
- logical emulated `CREATE|ALTER|DROP DATABASE`
- external table or equivalent family-specific support objects
- emulated catalog overlay bootstrap and teardown
- family-owned system procedures or service hooks
- internal donor-client bridge routines
- migration and validation helpers

## Install and Enablement Contract
1. Installer may stage parser, compiler UDR, and emulation UDR packages
   independently.
2. Enabling an emulated family requires a successful install-validation record
   for all three bundle parts.
3. Disabling an emulated family removes listener exposure even if all packages
   remain installed.
4. Package presence and activation state are persisted in the emulation profile
   catalog and consulted by listener startup, parser admission, and engine
   support-runtime activation.

## Failure Rules
1. Missing compiler UDR package with parser package present must fail listener
   startup deterministically.
2. Missing emulation UDR package with parser package present must fail listener
   startup deterministically.
3. Missing parser package with engine-side packages present must reject public
   listener enablement deterministically.
4. Version or capability mismatch must reject activation before first client
   request.
5. No incomplete package state may fall back to native parser or to compiled
   core-engine emulation behavior.

## Cross References
- `17_Functions_and_Procedures/NORMATIVE_UDR_EMULATED_ENGINE_SUPPORT_CHECKLIST.md`
- `28_Parser_Implementations/BETA2_EMULATION_PACKAGE_TEMPLATE_AND_FAMILY_DELIVERABLE_MODEL.md`
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_EMULATION_PROFILE.md`
- `29_Listener_and_Server_Orchestration/PROCESS_MODEL_AND_DEPENDENCY_GRAPH.md`
- `30_Client_Tooling/INSTALLER_PROFILES_AND_ARTIFACTS.md`

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.

## Hardening promotion note (2026-03-28)
- section `28` now carries explicit capability-state vocabulary for parser implementation proof lanes:
  - `supported_native_v3`
  - `supported_emulated_sql_family`
  - `supported_scaffold_or_udr_boundary`
  - `bounded_shipped_front_door`
  - `checklist_only`
  - `target_state_only`
  - `fail_closed`
- dedicated parser-family proof must be anchored to live parser code plus shipped parser-agent or listener/runtime seams, not to checklist presence alone
- native-V3 internal feature vocabulary must not be promoted into dedicated external parser-family parity without family-local source proof
- universal capability-profile generation, universal corpus cardinality, and universal wire parity claims remain non-authoritative unless backed by generated or runtime evidence
