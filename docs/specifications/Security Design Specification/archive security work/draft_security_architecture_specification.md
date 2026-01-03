# Draft Security Architecture Specification

## 1. Purpose and Scope
This document defines a draft security architecture and policy framework for the database engine and its associated components. It is intended as a living specification that can be refined as security levels are implemented. The scope explicitly covers **server and cluster modes**; embedded mode is treated as a trusted-host scenario with limited security guarantees.

## 2. Operating Modes

### 2.1 Embedded Mode
- Engine is linked directly into an application.
- Single-writer access enforced by file locking.
- Security controls are advisory only; a hostile application can bypass them.
- Meaningful protection is limited to:
  - Encryption at rest
  - Integrity checking of database files
- Embedded mode is **out of scope** for military-grade guarantees.

### 2.2 Server Mode
- Engine runs as one or more server processes.
- All external clients connect via network listeners.
- Listeners, parsers, and emulation layers communicate with the engine via authenticated local IPC (socket/localhost).
- Engine is the final authority for execution, authorization, and auditing.

## 3. Architectural Layers

### 3.1 Engine Core
- Standalone library implementing storage, execution, authorization, and auditing.
- Executes SBLR (internal bytecode language).
- Enforces all security decisions.

### 3.2 Parser and Emulation Layer
- Multiple parsers exist:
  - Native SQL
  - Emulated SQL dialects (e.g., MySQL, Firebird)
- Parsers translate SQL to SBLR.
- Parsers may cache parse results but do **not** enforce authorization.

### 3.3 Network Listener Layer
- Listens on protocol-specific ports.
- Performs protocol framing, TLS/mTLS, client identity validation.
- Establishes a connection identity but not user identity.

### 3.4 Plugin Subsystem
- Extends engine functionality:
  - Functions and procedures
  - Index mechanisms
  - Remote database connectivity (bridges)
- Plugins declare required capabilities and dependencies.
- Plugins participate in engine approval and auditing.

## 4. Identity and Context Model

### 4.1 Identifiers
- All entities are identified by UUID v7.
- UUIDs are never reused.
- Each database has a unique database UUID.
- Object identity is always scoped as `(database UUID, object UUID)`.

### 4.2 Connection, Session, and Transaction
- A connection represents a client-machine or library identity.
- A connection may host multiple sessions.
- Each session represents a user identity authenticated via an auth source.
- All activity occurs within a transaction.
- A transaction:
  - Has a full, immutable security context
  - Is always active (connecting starts a transaction)

### 4.3 AuthKey
- Each session is associated with an AuthKey object.
- AuthKey properties:
  - UUID
  - Issuing authentication source
  - Expiry and validity window
  - Usage limits
  - Allowed roles and groups
- Passwords are never stored or logged.

## 5. Security Context

- The Security Context is bound to a transaction.
- It includes:
  - Session UUID
  - AuthKey UUID
  - Effective role and group set
  - Emulation mode
  - Policy epoch
- The Security Context is immutable for the lifetime of the transaction.
- Role switching requires transaction commit or rollback.

## 6. Authorization and Approval Model

### 6.1 Plan vs Security Separation
- Parsers generate SBLR without applying privileges.
- Each SBLR program declares a complete set of dependent object UUIDs.
- The engine performs authorization using the Security Context.

### 6.2 Dependency Tracking
- Engine maintains full forward and backward dependency graphs.
- Dependencies include:
  - Tables, views, indexes, sequences
  - Functions, procedures, packages, UDRs
  - Domains, constraints, triggers
  - Synonyms (resolved to targets)
  - System catalog objects

### 6.3 Engine Approval
For each transaction or dynamic compilation, the engine:
- Verifies SBLR correctness
- Validates declared dependencies
- Authorizes access to all dependent UUIDs
- Issues an internal approval token bound to the transaction

Execution is denied if undeclared or unauthorized access is attempted.

## 7. Row-Level and Column-Level Security

- Row-level security (RLS) is applied at plan generation.
- Plans may be cached only within the same session and security context.
- Plan invalidation occurs when:
  - Relevant privileges change
  - Policies or domains change
  - Referenced objects are altered
- RLS context inputs must be explicitly defined and immutable within a transaction.

## 8. Dynamic SQL

- Dynamic SQL is a two-phase process:
  1. Generation of a dynamic SBLR block
  2. Runtime compilation and approval under the current Security Context
- Dynamic SQL is subject to the same dependency discovery and authorization rules as static SQL.
- Dynamic SQL may be globally disabled by engine configuration.

## 9. Object Types (Alpha)

The engine currently supports the following SQL objects:
- Functions
- Stored Procedures
- Packages
- User Defined Routines (UDRs)
- Exceptions
- Tables
- Views
- Indexes
- Sequences
- Domains
- Constraints
- Triggers
- Synonyms

All objects participate in dependency tracking and authorization.

## 10. Domains

- Domains are cluster-scoped.
- Domains may define:
  - Data constraints
  - Security validation (e.g., group membership for PII)
- Domain policies are enforced at write time and optionally at read time.
- Domain changes invalidate dependent plans.

## 11. UDRs

- UDRs are database-local objects.
- Maintenance rights are governed by cluster security.
- Execution rights are separate from maintenance rights.
- UDRs execute with a declared capability set.

## 12. Backup and Recovery

### 12.1 Logical Backup
- Text-based metadata and INSERT export.
- May be compressed and encrypted during generation.
- Subject to explicit export privileges.
- May operate in user-scoped or privileged modes.

### 12.2 Page-Level Backup
- Physical page-level copy for failover.
- Produces a shadow database.
- Promotion to production is a security-sensitive operation.

## 13. Cluster Architecture

### 13.1 Cluster Model
- Every database belongs to a cluster.
- Default cluster is local-to-self.
- Databases may act as:
  - Cluster controller
  - Cluster member (slave)

### 13.2 Cluster Authority
- Cluster governs:
  - Users, roles, groups
  - Domains
  - Security policies
- Databases enforce cluster-issued policies.

## 14. Remote Connectivity and Migration

- Remote database access is implemented via plugins.
- Emulation layers must be explicitly enabled.
- Live migration supports pass-through execution until migration completes.
- Audit logs must distinguish local vs pass-through execution.

## 15. Auditing and Monitoring

- All actions are audited with full context:
  - Connection UUID
  - Session UUID
  - AuthKey UUID
  - Transaction UUID
  - Effective roles
- Logs are append-only and tamper-evident.

## 16. Security Levels (Draft)

- Level 0: Open / Development
- Level 1: Authenticated Access Control
- Level 2: Encryption at Rest
- Level 3: Policy-Based Restrictions
- Level 4: Auditing and Monitoring
- Level 5: Client and Network Restrictions
- Level 6: Cluster-Hardened / High Assurance

Each level builds cumulatively on the previous.

## 16.1 Security-Level Compliance Checklist

### Level 0 – Open / Development
- Authentication optional or disabled
- No encryption required
- Minimal or no auditing
- Plugins unrestricted
- Intended for development, testing, or embedded-like use

### Level 1 – Authenticated Access Control
- User authentication required for all sessions
- Roles and privileges enforced by engine
- UUID-based object authorization enabled
- Role switching requires transaction boundary

### Level 2 – Encryption at Rest
- Database files encrypted
- Logical backups encrypted or protected
- Page-level backups encrypted and integrity-protected
- Encryption keys isolated from application access

### Level 3 – Policy-Based Restrictions
- Row-level and column-level security enabled
- Domain security validation enforced
- Time/date-based access policies enabled
- Dynamic SQL restricted or audited

### Level 4 – Auditing and Monitoring
- Full audit logging enabled
- Logs include connection, session, authkey, transaction, and object context
- Audit logs are append-only and tamper-evident
- Backup, restore, promotion, and security changes audited

### Level 5 – Client and Network Restrictions
- TLS/mTLS required for all external connections
- Client identity validated (certificates or equivalent)
- Network allowlists enforced
- Plugin outbound connectivity restricted

### Level 6 – Cluster-Hardened / High Assurance
- Centralized cluster authority for identities and policies
- Majority-truth cluster time source
- Quorum-based approval for critical operations
- Strict plugin capability enforcement
- Fail-closed behavior on security subsystem failures


## 17. Known Risks and Open Questions

- Long-running transactions vs delayed privilege revocation
- Plugin isolation guarantees
- Migration semantic mismatches
- Metadata visibility policies

## Appendix A: Threat Model and Attack Resistance

### A.1 Threat Actors

1. Unauthenticated remote attacker
2. Authenticated low-privilege user
3. Malicious or compromised plugin
4. Compromised client machine
5. Compromised server host
6. Rogue or compromised cluster member

### A.2 Attack Surfaces

- Network listeners and wire protocols
- SQL parsers and emulation layers
- SBLR bytecode execution
- Plugin and bridge interfaces
- Backup and export mechanisms
- Cluster membership and configuration changes

### A.3 Core Defensive Principles

- Engine-side enforcement of all authorization
- UUID-based immutable object identity
- Complete dependency declaration and verification
- Transaction-scoped immutable security contexts
- Explicit capability-based execution

### A.4 Attack Scenarios and Mitigations

#### Privilege Escalation via Cached Plans
- Mitigation: Plans cached only per session and security context; engine re-authorizes all dependencies

#### Unauthorized Object Access via Dynamic SQL
- Mitigation: Two-phase dynamic SQL with runtime authorization and dependency discovery

#### Metadata Enumeration
- Mitigation: Metadata visibility controls and restricted UUID lookup in hardened modes

#### Malicious Plugin Abuse
- Mitigation: Plugin capability declarations, cluster-controlled maintenance rights, audit logging

#### Data Exfiltration via Backup
- Mitigation: Explicit export privileges, encryption, and audit of all backup operations

#### Cluster Split-Brain or Time Manipulation
- Mitigation: Majority-based cluster authority and timekeeping in Level 6

### A.5 Residual Risks

- Host-level compromise can bypass in-process controls
- Authorized privileged users can perform destructive actions
- Migration pass-through may inherit legacy security weaknesses

These risks are accepted and documented as part of the system threat posture.


## Appendix B: Formal Security Invariants

The following invariants MUST hold for all deployments, regardless of security level, unless explicitly stated otherwise.

1. **Engine Authority Invariant**  
   All authorization, dependency verification, and execution approval decisions are made by the engine. Parsers, listeners, and plugins may not bypass engine enforcement.

2. **Declared Dependency Invariant**  
   No execution context may access any database object whose UUID is not present in the approved dependency set for the transaction.

3. **Immutable Transaction Security Context**  
   The Security Context (user, roles, groups, authkey, emulation mode) is immutable for the lifetime of a transaction.

4. **UUID Non-Reuse Invariant**  
   Object UUIDs are never reused. Drop and recreate operations always generate new UUIDs.

5. **Plugin Capability Invariant**  
   Plugins may only perform actions explicitly granted by their declared and approved capability set.

6. **Audit Completeness Invariant**  
   All security-relevant events must generate audit records with sufficient context to reconstruct intent and outcome.

---

## Appendix C: Security Testing and Verification Matrix

This appendix maps security requirements to recommended testing strategies.

### C.1 Unit Testing
- Authorization decisions for all object types
- Dependency graph completeness
- UUID resolution and scoping
- Domain security validation

### C.2 Integration Testing
- End-to-end authentication and authorization flows
- Role switching and transaction boundary enforcement
- Backup and restore workflows
- Dynamic SQL approval paths

### C.3 Fuzz and Robustness Testing
- SQL parser fuzzing (native and emulated)
- Wire protocol fuzzing
- SBLR bytecode verification fuzzing
- Plugin interface fuzzing

### C.4 Security Regression Testing
- Known privilege escalation patterns
- Metadata enumeration attempts
- Cached plan reuse across contexts

---

## Appendix D: Operational Security Runbooks (Draft)

### D.1 Cluster Authority Loss
- Default behavior: fail closed in Level 6, configurable in lower levels
- Allow limited cached authorization only if explicitly configured
- Audit all degraded-mode operation

### D.2 Key Compromise or Rotation
- Revoke affected AuthKeys
- Rotate encryption keys
- Rewrap page-level backups
- Invalidate affected sessions and plans

### D.3 Database Promotion from Shadow Backup
- Verify integrity and version compatibility
- Require elevated cluster privileges
- Record promotion event in audit log

### D.4 Plugin Compromise Response
- Disable or unload plugin
- Revoke plugin capabilities
- Audit all prior plugin activity

---

## Appendix E: Compliance and Control Mapping (Draft)

This appendix provides a conceptual mapping to common security control frameworks.

### E.1 Identity and Access Control
- Maps to NIST AC family, ISO 27001 A.9
- Covered by AuthKey model, roles, groups, and transaction-scoped contexts

### E.2 Cryptographic Protection
- Maps to NIST SC and IA families, ISO 27001 A.10
- Covered by encryption at rest, encrypted backups, TLS/mTLS

### E.3 Audit and Accountability
- Maps to NIST AU family, ISO 27001 A.12
- Covered by append-only, context-rich audit logs

### E.4 System and Communications Protection
- Maps to NIST SC family
- Covered by network restrictions, plugin capability controls, IPC authentication

This mapping is illustrative and non-exhaustive.

---

## Appendix F: Privilege Taxonomy and Capability Bundles (Draft)

### Appendix F.1 Capability Bundles

Capability bundles are named collections of privileges. They are not roles; they are composable building blocks used to define roles appropriate to each security level.

**access_basic** – minimal connectivity and read access
- CONNECT, SESSION_CREATE, ROLE_ACTIVATE
- SELECT (scoped), METADATA_READ (scoped)

**access_extended** – standard application access
- access_basic
- INSERT, UPDATE, DELETE (scoped)
- ROUTINE_EXECUTE (non-definer)

**developer_schema** – schema and code development
- access_extended
- CREATE/ALTER schema objects (tables, views, indexes, functions, procs, triggers, synonyms, exceptions)

**developer_drop** – destructive DDL (non-production)
- developer_schema
- DROP privileges for schema objects

**etl_ingest** – bulk ingest and transformation
- access_extended
- TRUNCATE, MERGE (scoped)
- EXPORT_LOGICAL_USERSCOPED

**backup_logical** – logical backup and restore
- EXPORT_LOGICAL_USERSCOPED
- RESTORE_LOGICAL
- METADATA_ENUMERATE (limited)

**backup_physical** – page-level backup
- BACKUP_PAGELEVEL_CREATE
- RESTORE_PAGELEVEL

**backup_promotion** – promote shadow database
- PROMOTE_SHADOW

**migration_operator** – live migration and pass-through
- MIGRATION_MANAGE
- PASSTHROUGH_EXECUTE
- REMOTE_CONNECT

**udr_author** – create and maintain UDRs
- UDR_CREATE, UDR_ALTER, UDR_DROP, UDR_EXECUTE

**udr_operator** – execute UDRs only
- UDR_EXECUTE

**plugin_operator** – plugin lifecycle management
- PLUGIN_LOAD, PLUGIN_UNLOAD, PLUGIN_UPDATE

**operations_runner** – operational maintenance
- QUERY_CANCEL, TX_KILL (scoped), SESSION_KILL (scoped)

**security_operator** – day-to-day security administration
- SECURITY_ADMIN, AUTHKEY_REVOKE, AUDIT_CONFIG_ALTER

**security_architect** – high-risk security configuration
- AUTHN_CONFIG_ALTER, DOMAIN_POLICY_MANAGE, KEY_MGMT

**cluster_controller** – cluster authority
- AUTHKEY_ISSUE, SESSION_IMPERSONATE, CONFIG_WRITE, BREAK_GLASS

---

### Appendix F.2 Default Bundle-to-Security-Level Allowance Table

Legend: A = Allowed, R = Restricted (see Appendix F.3), D = Disallowed

| Bundle | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---|---|---|---|---|---|---|---|
| access_basic | A | A | A | A | A | A | A |
| access_extended | A | A | A | A | A | A | A |
| developer_schema | A | A | A | R | R | R | R |
| developer_drop | A | R | R | D | D | D | D |
| etl_ingest | A | A | A | A | A | R | R |
| backup_logical | A | A | A | A | A | R | R |
| backup_physical | A | R | R | R | R | R | R |
| backup_promotion | A | R | R | R | R | R | R |
| migration_operator | A | R | R | R | R | R | R |
| udr_author | A | A | A | R | R | R | R |
| udr_operator | A | A | A | A | A | A | A |
| plugin_operator | A | R | R | R | R | R | R |
| operations_runner | A | A | A | A | A | A | R |
| security_operator | A | R | R | R | A | A | A |
| security_architect | A | R | R | R | R | R | R |
| cluster_controller | A | D | D | D | D | D | A |

---

### Appendix F.3 Restricted Grant Ruleset (Enforceable)

Restricted (R) grants are only valid if all conditions below are met; otherwise they are treated as Disallowed.

**Common rules (all levels):** explicit scope required; TTL + absolute expiry required; audit tagging mandatory; no delegation; cache invalidation on grant/revoke.

**R@L1:** AuthKey required; scope ≤ schema; TTL ≤ 7 days.

**R@L2:** R@L1 + encryption required for export/backup; TTL ≤ 72h for backup/migration.

**R@L3:** R@L2 + RLS/CLS/domain enforcement; scope ≤ schema (≤ database only with justification); TTL ≤ 24h.

**R@L4:** R@L3 + tamper-evident audit; dual audit sinks; TTL ≤ 12h.

**R@L5:** R@L4 + mTLS required; client/network binding; scope ≤ schema by default; TTL ≤ 4h; outbound endpoint allowlisting.

**R@L6:** R@L5 + quorum approval; two-person rule; cluster time; scope ≤ object by default; TTL ≤ 1h; break-glass required for high-risk actions.

---

## Appendix H: Break-Glass Lifecycle Specification (Draft)

Break-glass is an emergency authorization mechanism used to temporarily enable otherwise disallowed or highly restricted operations. It is intended for incident response and recovery, and must be designed to be difficult to misuse and easy to audit.

### H.1 Break-Glass Goals
- Enable time-limited emergency actions required to restore service or security
- Provide strong accountability (who, why, what, when)
- Ensure automatic expiry and minimal persistence
- Minimize blast radius through scoping and capability restriction

### H.2 Break-Glass Artifact: BreakGlassToken
A BreakGlassToken is an engine-recognized, cluster-governed authorization artifact.

**Required fields**
- Token UUID
- Issuer identity (cluster/controller identity)
- Requester identity (user/session/authkey)
- Issued time (cluster time)
- Expiry time (cluster time)
- Approved capability bundle(s) and/or privileges
- Scope constraints (database/schema/object/operation)
- Justification text (incident ID / ticket ID)
- Approval metadata (quorum members, signatures)

**Optional fields**
- Usage limits (max statements, max transactions, max bytes exported)
- Network constraints (allowed client identity/cert, allowed IP)
- Mandatory audit level override (force verbose audit)

### H.3 Issuance Policy
- Break-glass issuance is **disabled by default** unless Level 6 or explicitly enabled.
- Issuance requires **quorum approval** by cluster controllers (configurable threshold).
- Issuance must fail closed if quorum cannot be established.
- The system must support “two-person rule” (separation of duties): requester and approver must be distinct identities.

### H.4 Activation and Use
- A BreakGlassToken may only be attached to a session by an identity with explicit break-glass activation permission.
- When attached, the effective Security Context gains only the explicitly listed capabilities and scopes.
- Break-glass never grants implicit superuser authority.
- All statements executed under break-glass must be tagged in audit logs.

### H.5 Expiry and Auto-Revoke
- BreakGlassTokens must have a short TTL (default minutes to hours).
- The engine must automatically revoke break-glass capabilities upon expiry.
- Expiry is evaluated using cluster-majority time.
- On expiry:
  - New transactions cannot begin under break-glass.
  - In-flight statements may be cancelled (policy), or allowed to complete but subsequent statements denied.
  - Any limbo transactions created under break-glass must be forcibly resolved (commit/rollback policy) or invalidated.

### H.6 Revocation
- Tokens may be revoked early by quorum-approved action or by designated security controllers.
- Revocation takes effect at the next statement boundary at latest; higher modes may enforce immediate cancellation.
- Revocation events must be audited with actor identity and reason.

### H.7 Audit Requirements
Every break-glass lifecycle event must be audited:
- Request created (who requested, why)
- Approvals recorded (who approved)
- Token issued (what scopes/capabilities, TTL)
- Token attached to session (which session/authkey)
- Each statement executed (statement hash, affected UUIDs, result)
- Token revoked or expired

Audit records must include token UUID to enable complete reconstruction.

### H.8 Safe Defaults and Guardrails
- Break-glass may not disable audit logging.
- Break-glass may not reduce encryption requirements.
- Break-glass may not relax dependency verification.
- Break-glass must not permit plugin capability grants unless explicitly scoped and approved.

### H.9 Operational Guidance
- Use break-glass only for:
  - restoring service
  - rotating compromised keys
  - disabling compromised plugins
  - emergency promotion of shadow databases
- Maintain an incident workflow that always records a justification reference.

---

## Appendix I: Formal Security Context Hash Definition (Draft)

This appendix defines the **Security Context Hash (SCH)** and related hashes used to key caches and approvals in a way that is correct and auditable.

### I.1 Design Goals
- Prevent cache/plan reuse across differing authorization or policy contexts.
- Make cache keys deterministic and explainable.
- Ensure revocations and policy changes invalidate affected cached artifacts.

### I.2 Hashes
The engine maintains the following hashes (all are cryptographic hashes over canonicalized inputs):

1. **Security Context Hash (SCH)**  
   Captures identity and authorization-bearing context.

2. **Policy Epoch Hash (PEH)**  
   Captures relevant security policy versions/epochs.

3. **Dependency State Hash (DSH)**  
   Captures schema/object versioning for dependencies referenced by a plan/statement.

### I.3 Security Context Hash (SCH) Inputs
SCH MUST be computed from the following canonical fields:

**Identity and authorization**
- Database UUID
- Session UUID
- AuthKey UUID
- Auth source identifier (e.g., local/cluster/external provider ID)
- Effective principal UUID (cluster user identity)
- Effective role set (sorted list of role UUIDs)
- Effective group set (sorted list of group UUIDs)
- Allowed role set (if AuthKey constrains role activation)

**Connection/client binding (when enabled)**
- Client identity binding (e.g., mTLS certificate fingerprint/subject key ID)
- Listener protocol and emulation mode identifier

**Execution policy selectors**
- Security level (0–6)
- Transaction mode selectors that affect authorization semantics (if any):
  - e.g., definer/invoker mode toggles (where applicable)

**RLS/CLS and domain context**
- Canonicalized RLS context attributes that may affect predicates (e.g., tenant_id, clearance labels), expressed as a sorted key/value map
- Canonicalized domain-policy context inputs (if domains use contextual checks)

**Exclusions**
- SCH MUST NOT include secrets (passwords, raw tokens). It may include stable identifiers such as AuthKey UUID.

### I.4 Policy Epoch Hash (PEH) Inputs
PEH MUST include version/epoch identifiers for all policy sources that can affect authorization decisions:
- Grants/privileges epoch for the effective principal
- Role membership epoch
- Group membership epoch
- RLS/CLS policy epoch(s)
- Domain definition/policy epoch(s) (cluster-scoped)
- Authn configuration epoch (supported methods + mapping rules)
- Plugin capability policy epoch (if plugins influence access paths)

### I.5 Dependency State Hash (DSH) Inputs
DSH is computed per statement/plan from the resolved dependency set:
- Sorted list of dependent object UUIDs
- For each object UUID: object definition/version stamp (e.g., catalog generation, DDL version, or content hash)
- For programmable objects: code version stamp
- For synonyms: include both synonym UUID and resolved target UUID/version

### I.6 Cache Key Requirements

**Plan Cache Key** MUST include:
- Normalized SQL text (or parse-tree canonical form)
- Dialect/emulation identifier
- SCH
- PEH
- DSH
- Engine version identifier (to prevent reuse across incompatible bytecode/semantics)

**SBLR Approval Token** MUST be bound to:
- Transaction UUID
- SCH and PEH
- Declared dependency UUID set and DSH
- Code identity (SBLR checksum for transportable code, native blob hash + metadata for non-transportable code)
- Any declared capability requirements (plugins/remote connectivity)

### I.7 Invalidation Rules (Normative)
The engine MUST invalidate cached plans/approvals when any of the following change for the affected scopes:
- Grants/privileges, roles, or group memberships (PEH changes)
- RLS/CLS policies or inputs (SCH/PEH changes)
- Domain definitions or security validation rules (PEH changes)
- Dependent object definitions (DSH changes)
- Authn configuration or AuthKey constraints (SCH/PEH changes)
- Plugin capability policies for affected bundles/paths (PEH changes)

---

## Appendix I.8 Canonicalization Specification (Normative)

This section defines how inputs to SCH/PEH/DSH are normalized and encoded so that independent implementations produce identical hashes.

### I.8.1 General Rules
- **Deterministic encoding**: All hashes are computed over a byte sequence produced by a deterministic serialization.
- **No locale dependence**: No locale-specific collation, casefolding, or formatting is permitted.
- **No floating timestamps**: Only explicit epoch IDs, UUIDs, and declared time values may be included.
- **Domain separation**: Each hash input begins with a fixed ASCII prefix identifying the hash type and version.

### I.8.2 Serialization Format
- Use a canonical **TLV** (Type-Length-Value) encoding.
- **Type**: 2-byte unsigned integer (big-endian).
- **Length**: 4-byte unsigned integer (big-endian), length of Value in bytes.
- **Value**: raw bytes.

All fields are appended in strictly increasing Type order.

### I.8.3 Primitive Encodings
- **UUID v7**: 16 raw bytes in network byte order.
- **Unsigned integers**: minimal-length big-endian, unless specified by TLV header.
- **Booleans**: 1 byte: 0x00 false, 0x01 true.
- **Strings**: UTF-8 NFC normalized, with no BOM. Trim is not applied unless explicitly stated.

### I.8.4 Identifier Normalization
- **Database/schema/object path strings** (used only when present as labels, not for authorization):
  - Normalize to Unicode NFC.
  - Use `/` as separator in canonical paths.
  - Preserve case as stored; do not casefold.
- **Dialect/emulation identifier**: fixed enumerated ID, not a free-form string.
- **Auth source identifier**: fixed enumerated ID or UUID.

### I.8.5 Ordered Sets
For any set input (roles, groups, dependency UUIDs):
- Convert to raw UUID bytes.
- Sort lexicographically by the 16-byte UUID value.
- Serialize as a TLV containing the concatenation of sorted UUID bytes.

### I.8.6 Key/Value Maps (Context Attributes)
For any key/value map (e.g., RLS context attributes):
- Keys are UTF-8 NFC strings.
- Sort entries by bytewise UTF-8 key.
- Encode each entry as:
  - TLV(Type=Key, Value=key-bytes)
  - TLV(Type=Value, Value=value-bytes)
- Value encoding:
  - If value is UUID: 16 bytes
  - If value is integer: big-endian minimal
  - If value is string: UTF-8 NFC
  - If value is boolean: 1 byte

### I.8.7 Policy Epoch and Version Stamps
- Epoch identifiers must be stable, monotonic, and comparable.
- Canonical encoding is either:
  - UUID (preferred), or
  - 64-bit unsigned integer (big-endian) if UUID not available.
- Object definition/version stamps (for DSH) must be one of:
  - content hash bytes, or
  - monotonically increasing DDL generation counter.

### I.8.8 Hash Algorithms
- SCH/PEH/DSH use **SHA-256** over the canonical TLV byte sequence.
- Output is 32 bytes.
- For logging/display, represent as lowercase hex.

### I.8.9 Hash Versioning
- Each hash includes a version marker TLV:
  - Type=0x0001, Value=`SCHv1` / `PEHv1` / `DSHv1` ASCII.
- Any change to canonicalization or field set increments the version string.

### I.8.10 Field Type Registry (Initial)
The following TLV Type IDs are reserved for Appendix I hashes:

**Common**
- 0x0001: Hash version marker
- 0x0002: Database UUID
- 0x0003: Security level
- 0x0004: Dialect/emulation ID

**SCH**
- 0x0100: Session UUID
- 0x0101: AuthKey UUID
- 0x0102: Auth source ID
- 0x0103: Principal UUID
- 0x0104: Effective role set
- 0x0105: Effective group set
- 0x0106: Allowed role set
- 0x0107: Client identity binding
- 0x0108: RLS context map
- 0x0109: Domain context map

**PEH**
- 0x0200: Grants epoch
- 0x0201: Role membership epoch
- 0x0202: Group membership epoch
- 0x0203: RLS/CLS policy epoch
- 0x0204: Domain policy epoch
- 0x0205: Authn config epoch
- 0x0206: Plugin capability policy epoch

**DSH**
- 0x0300: Dependency UUID set
- 0x0301: Dependency version stamp list
- 0x0302: Synonym resolution pairs

Implementations must not reuse Type IDs for different meanings.

### I.8.11 Worked Example (SCHv1)

This example demonstrates a minimal Security Context Hash (SCH) input set and the resulting canonical TLV hex and SHA-256.

**Inputs**
- Database UUID: `0198f0b2-3c4d-7e80-9a0b-1c2d3e4f5061`
- Security level: `5`
- Dialect/Emulation ID: `1`
- Session UUID: `0198f0b2-1111-7e80-9a0b-aaaaaaaaaaaa`
- AuthKey UUID: `0198f0b2-2222-7e80-9a0b-bbbbbbbbbbbb`
- Principal UUID: `0198f0b2-3333-7e80-9a0b-cccccccccccc`
- Effective role set: [`0198f0b2-4444-7e80-9a0b-dddddddddddd`]
- Effective group set: empty
- Client identity binding (mTLS fingerprint): 32 bytes of `0x11`

**Canonical TLV hex** (concatenated TLVs, wrapped for readability)
```
00010000000553434876310002000000100198f0b23c4d7e809a0b1c2d3e4f50
6100030000000105000400000001010100000000100198f0b211117e809a0baa
aaaaaaaaaa0101000000100198f0b222227e809a0bbbbbbbbbbbbb0103000000
100198f0b233337e809a0bcccccccccccc0104000000100198f0b244447e809a
0bdddddddddddd01050000000001070000002011111111111111111111111111
11111111111111111111111111111111111111
```

**SHA-256(SCH TLV bytes)**
- `2aa73b393ff278adcfe0ffbdb4d535a03fe7d326f6b9f1711f674575b2327e76`

Implementations can use this example as an interoperability test vector.

---

## Appendix J: Quorum Rule Matrix (Draft)

This appendix defines recommended quorum and approval requirements for **Level 6** deployments. Lower levels may adopt these rules optionally.

### J.1 Definitions
- **Controller**: a cluster authority node eligible to participate in quorum.
- **Quorum Threshold**: the minimum number of distinct controller approvals required.
- **Two-Person Rule**: requester and approver(s) must be distinct identities.

### J.2 Default Threshold Recommendations
Thresholds are configurable; defaults depend on controller count:
- 1 controller: quorum not meaningful; Level 6 is not recommended.
- 3 controllers: **2-of-3**
- 5 controllers: **3-of-5**
- 7 controllers: **4-of-7**

### J.3 Approval Classes

| Operation / Change | Approval Class | Default Threshold | Two-Person Rule | Notes |
|---|---|---:|---:|---|
| Issue BreakGlassToken | Critical | quorum | Yes | Must include justification + expiry |
| Key management (rotate/rewrap/master key ops) | Critical | quorum | Yes | Prefer break-glass coupling |
| Authn config change (enable/disable methods, mapping rules) | Critical | quorum | Yes | Fail closed on inability to validate |
| Grant ROW_SECURITY_BYPASS or COLUMN_SELECT_BYPASS | Critical | quorum | Yes | Time-limited; strongly scoped |
| Grant EXPORT_LOGICAL_PRIVILEGED | Critical | quorum | Yes | Must record row/byte counts |
| Promote shadow DB to production | Critical | quorum | Yes | Audit includes integrity verification |
| Plugin capability grant (network/file/raw access) | High | quorum | Yes | Use allowlists; record endpoints |
| Enable remote bridge endpoints / credentials | High | quorum | Yes | Prefer storing credentials via secure mechanism |
| Add/remove cluster controller | Critical | quorum | Yes | Controller set change is highest risk |
| Add/remove cluster member | High | quorum | Yes | Must bind node identity (cert) |
| Change security level (upgrade) | High | quorum | Yes | Downgrades require break-glass |
| Security level downgrade 6→5 or below | Critical | quorum | Yes | Must include expiry and remediation plan |
| Create/alter domain security policies | High | quorum | Yes | Domain changes can affect PII enforcement |
| UDR capability grant / expand UDR sandbox | High | quorum | Yes | Treat as code execution risk |
| Disable tamper-evident auditing | Critical | quorum | Yes | Disallowed by default; break-glass required |

### J.4 Audit Requirements (Normative)
For any operation in this matrix, audit logs MUST capture:
- Requester identity (session/authkey)
- Approver identities
- Operation parameters (scopes, endpoints, bundles/privileges)
- Before/after configuration or policy epoch IDs
- Effective time and expiry (if temporary)

### J.5 Enforcement Guidance
- If quorum cannot be established, Critical-class operations MUST fail closed.
- Approvals may be represented as signed approval records bound to the request payload hash.
- All approvals must be replay-protected (single-use request IDs).
