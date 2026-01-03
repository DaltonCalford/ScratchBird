# SBSEC-ALPHA-BASE.md

**ScratchBird Security Specification – Alpha Base**

Version: Alpha-1
Status: Normative (Authoritative)

---

## 1. Purpose and Authority

This document defines the **minimum authoritative security model** for the ScratchBird engine.

- All other SBSEC documents (SBSEC-01 … SBSEC-09 and sub-documents) **extend or specialize** this document.
- In the event of conflict, **this document takes precedence**.
- This document is written to be unambiguous and directly implementable by humans and automated agents.

---

## 2. Core Trust Model

### 2.1 Component Authority

**Parser / Listener**
- Carries syntax and structure only
- Performs no authentication, authorization, policy, or security decisions
- Must be assumed potentially compromised

**Engine Core**
- Sole authority for:
  - Authentication
  - Authorization
  - Row-Level Security (RLS)
  - Column-Level Security (CLS)
  - Audit emission
  - Key access and cryptographic operations
- Validates and approves all execution plans

No other component may make or override security decisions.

---

## 3. Canonical Security Execution Pipeline

The following pipeline is **normative**. All query execution MUST conform to this order:

1. Receive query representation (untrusted)
2. Bind to transaction Security Context (immutable for transaction lifetime)
3. Resolve object UUIDs
4. Determine applicable RLS/CLS policies from the catalog
5. Apply RLS/CLS **engine-side** (plan construction or validation)
6. Validate authorization for all referenced objects
7. Approve execution plan
8. Begin execution
9. Execute optional security hooks (see §5)
10. Emit audit events

RLS/CLS MUST be enforced **at or before the first tuple visibility decision** and MUST constrain all downstream operators.

---

## 4. Security Context Rules

- Security Context is bound to the transaction
- Role, group, and privilege changes become visible only after transaction end
- Default behavior favors consistency over immediacy

Higher security modes may add additional enforcement hooks but MUST NOT violate transaction isolation semantics.

---

## 5. Engine Security Hooks

### 5.1 BeforeSelect Hooks (Revocation-Sensitive)

BeforeSelect hooks MAY be configured for objects requiring immediate authorization re-evaluation.

They MAY:
- Re-check authorization or policy epochs
- Abort statement execution
- Deny access before row materialization

They MUST NOT:
- Modify result rows
- Inject or alter predicates
- Bypass RLS/CLS
- Access raw `sys.*` tables

### 5.2 BeforeInsert Hooks (Deceptive / Honeypot Capable)

BeforeInsert hooks are **explicitly permitted** to:
- Modify result sets returned to the client
- Substitute or fabricate data
- Redirect writes to decoy structures
- Act as honeypots while concealing real data

This capability is **intentional** and MUST NOT be restricted by default.

Rules:
- Modifications MUST be engine-executed
- All actions MUST be auditable
- Hooks MUST NOT expose raw `sys.*` tables

---

## 6. System Catalog (`sys.*`) Access Model

- Direct SQL access to `sys.*` tables is **prohibited**
- This applies to all users, including superusers

### 6.1 Engine-Owned Virtual Views

- Metadata is exposed exclusively via **engine-owned virtual views** and `SHOW` / `DESCRIBE` commands
- Virtual views:
  - Are NOT SQL views
  - Are NOT user-definable or alterable
  - Are NOT introspectable via SQL
  - Enforce explicit redaction and authorization rules

---

## 7. Cryptographic Authority (High-Level)

- Algorithm choice is secondary to lifecycle correctness
- Key material access is restricted to the engine

### 7.1 Memory Encryption Key (MEK)

- MEK is OPTIONAL for Security Levels 0–5
- MEK is REQUIRED for Security Level 6
- Absence of MEK at Level 6 MUST seal the database

---

## 8. Security Levels (Enforcement Rule)

If any REQUIRED control for a security level is missing or misconfigured:

> The system MUST operate at the highest lower level for which all requirements are satisfied.

Partial or mixed security levels are forbidden.

---

## 9. Non-Goals (Alpha)

The following are explicitly NOT supported:

- Parser-defined security logic
- Dynamic privilege escalation mid-statement
- SQL-accessible key material
- User-defined access to `sys.*`
- Silent downgrade of security levels

---

## 10. Normative Glossary

**Security Context**
: Immutable per-transaction set of identity, role, group, and privilege state

**Engine-Owned Virtual View**
: Non-SQL metadata interface implemented directly by the engine

**RLS / CLS**
: Row- and column-level policy constraints applied by the engine

**Revocation-Sensitive Object**
: Object requiring authorization checks beyond transaction start

**MEK**
: Memory Encryption Key used to enforce presence binding (Level 6)

---

## 11. Reference Map

- Architecture: SBSEC-01
- Authorization Model: SBSEC-03
- Encryption & Key Management: SBSEC-04
- IPC Security: SBSEC-05
- Cluster & Quorum: SBSEC-06
- Audit & Compliance: SBSEC-08
- Security Levels: SBSEC-09

---

**End of SBSEC-ALPHA-BASE.md**