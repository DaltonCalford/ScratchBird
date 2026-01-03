# SBSEC_HANDOFF_SUMMARY.md

**ScratchBird Security Architecture & Specification – Handoff Summary**

Version: Alpha-1
Audience: Architects, senior engineers, automated design agents, specification authors
Status: Informational but Authoritative for Context

---

## 1. Purpose of This Document

This document is a **complete architectural and security-model summary** of the ScratchBird project as it stands at Alpha.

It is intended to be provided verbatim to a new chat session, engineer, or AI system so that:

- core assumptions do NOT need to be re-explained
- architectural invariants are preserved
- future specifications build *on* this model rather than reinterpreting it

This document does **not** replace the SBSEC specifications. It explains **how they fit together**.

---

## 2. High-Level System Architecture

ScratchBird is a database engine designed around **explicit trust boundaries** and **engine-centric security authority**.

### 2.1 Major Components

1. **Client / Network Listener**
   - Handles network I/O and protocol framing
   - Untrusted by design

2. **Parser Layer (Constrained)**
   - Parses SQL or other query languages
   - Produces a structural query representation or bytecode
   - Performs **no security decisions**
   - May be sandboxed or externally provided

3. **Engine Core (Trusted)**
   - Sole authority for:
     - authentication
     - authorization
     - row-level security (RLS)
     - column-level security (CLS)
     - audit emission
     - cryptographic key access
   - Executes queries and enforces all security invariants

4. **Storage / Execution Subsystems**
   - Operate under engine control
   - Do not make independent security decisions

---

## 3. Core Security Philosophy

### 3.1 Engine Is the Sole Authority

All security decisions are made **inside the engine core**.

- Parsers carry syntax only
- Plans or bytecode are treated as untrusted input
- The engine validates, approves, and executes

This model is intentionally hostile to:
- parser compromise
- SQL injection
- confused-deputy attacks

---

## 4. Canonical Execution Pipeline (Security-Relevant)

Every query follows this pipeline:

1. Receive untrusted query representation
2. Bind to a **transaction Security Context** (immutable)
3. Resolve all object references by UUID
4. Determine applicable RLS/CLS policies from the catalog
5. Apply or validate RLS/CLS **engine-side**
6. Validate authorization for all referenced objects
7. Approve execution plan
8. Execute
9. Run optional security hooks (BeforeSelect / BeforeInsert)
10. Emit audit events

RLS/CLS MUST be enforced **at or before first tuple visibility**.

---

## 5. Security Context Model

- Security Context is bound to the transaction
- Role, group, and privilege changes become visible only after transaction end
- Default behavior favors consistency over immediacy

This avoids mid-statement privilege confusion and supports MVCC semantics.

---

## 6. Hooks and Triggers (Important Asymmetry)

### 6.1 BeforeSelect Hooks

Purpose: **revocation-sensitive enforcement**

- May re-check authorization or policy epochs
- May abort execution before row materialization
- MUST NOT modify result rows

### 6.2 BeforeInsert Hooks

Purpose: **deception, honeypots, transparent data substitution**

- MAY modify results returned to the client
- MAY fabricate or redirect data
- Used intentionally to hide real data or trap misuse
- All actions are auditable

This asymmetry is intentional and security-critical.

---

## 7. System Catalog (`sys.*`) Model

- Direct SQL access to `sys.*` tables is prohibited
- Applies to all users, including superusers

### 7.1 Engine-Owned Virtual Views

Metadata is exposed via:
- engine-owned virtual views
- `SHOW` / `DESCRIBE` commands

These views:
- are NOT SQL views
- are not user-definable
- enforce explicit redaction and authorization

This prevents catalog-based privilege escalation.

---

## 8. Cryptographic Architecture (Summary)

- Correct key lifecycle > algorithm choice
- AES-GCM used with **strict, normative nonce/IV rules**
- Keys are hierarchical (CMK → DBK → DEK, etc.)

### 8.1 Memory Encryption Key (MEK)

- MEK is OPTIONAL for Security Levels 0–5
- MEK is REQUIRED for Level 6
- Absence of MEK at Level 6 seals the database

MEK supports **presence binding / dead-man’s-switch** clustering.

---

## 9. Clustering, Quorum, and Presence Binding

- Cluster operations may require quorum approval
- Quorum proposals use canonical hashing and signatures

### 9.1 Presence Binding (Level 6)

- Key material is split across cluster nodes
- Loss of quorum permanently destroys access
- Irrecoverable loss is a **deliberate design choice**

---

## 10. Audit Model

- Audit is mandatory for security-relevant events
- Audit events are canonically serialized
- Events are chained via cryptographic hashes
- Checkpoints enable verification

Audit is designed to be tamper-evident and externally verifiable.

---

## 11. Security Levels

Security is progressive from **Level 0 (no security)** to **Level 6 (military-grade)**.

Rule:
> If any required control for a level is missing, the system operates at the highest lower valid level.

Partial levels are forbidden.

---

## 12. Open Source + Certified Binaries Model

- The codebase is fully open source
- Official releases provide:
  - signed, certified binaries
  - SBOMs
  - official technical support

Community builds remain valid but are not certified.

---

## 13. Key Normative Documents (Current Alpha)

- SBSEC-ALPHA-BASE.md – authoritative base model
- CONTRIBUTOR_SECURITY_RULES.md – contributor behavior contract
- SBSEC-01 … SBSEC-09 – subsystem specifications
- SBSEC-10 – release integrity & provenance
- SUPPORTABILITY_CONTRACT.md – operational support guarantees

---

## 14. Design Non-Negotiables (Do Not Reinterpret)

- Engine is sole security authority
- Parser is untrusted
- sys.* is never directly accessible
- RLS/CLS are engine-applied
- BeforeInsert deception is allowed
- Level 6 may cause irrecoverable loss

Any future specification must preserve these invariants.

---

**End of SBSEC_HANDOFF_SUMMARY.md**

