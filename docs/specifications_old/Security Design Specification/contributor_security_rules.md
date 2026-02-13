# CONTRIBUTOR_SECURITY_RULES.md

**ScratchBird Security – Contributor Rules (Alpha)**

Status: Normative for Contributors
Audience: Human contributors, junior engineers, automated agents, and code-generation systems

---

## 1. Purpose

This document defines **mandatory security rules** for anyone contributing code, documentation, or tooling to the ScratchBird engine.

- These rules are derived directly from `SBSEC-ALPHA-BASE.md`.
- Violating these rules is considered a **security defect**, not a style issue.
- If this document conflicts with any other contributor guidance, **this document takes precedence**.

---

## 2. Fundamental Authority Rules

### 2.1 Engine Is the Sole Security Authority

You MUST assume:
- The parser, listener, and client-facing layers are **untrusted**.
- Only the **engine core** may make security decisions.

You MUST NOT:
- Perform authentication, authorization, RLS, CLS, or audit decisions in the parser
- Trust SQL text, parse trees, or client-provided metadata for security enforcement

---

## 3. Query Processing Rules

### 3.1 Parser Responsibilities

The parser MAY:
- Parse syntax
- Build structural representations
- Assign placeholder identifiers

The parser MUST NOT:
- Apply RLS or CLS
- Inject or remove security predicates
- Decide object visibility
- Decide authorization outcomes

---

### 3.2 Engine Responsibilities

The engine MUST:
- Resolve object UUIDs
- Determine applicable RLS/CLS policies from the catalog
- Apply or validate RLS/CLS **engine-side** before execution
- Validate authorization for all referenced objects

---

## 4. Trigger and Hook Rules

### 4.1 BeforeSelect Hooks (Revocation-Sensitive)

BeforeSelect hooks MAY:
- Re-check authorization or policy epochs
- Abort execution before row materialization

BeforeSelect hooks MUST NOT:
- Modify result rows
- Inject or alter predicates
- Mask or fabricate data
- Access raw `sys.*` tables

---

### 4.2 BeforeInsert Hooks (Deception / Honeypot)

BeforeInsert hooks MAY:
- Modify rows returned to the client
- Substitute or fabricate data
- Redirect writes to decoy or shadow structures
- Act as deception or honeypot mechanisms

BeforeInsert hooks MUST:
- Be executed by the engine
- Emit audit records for all modifications

BeforeInsert hooks MUST NOT:
- Expose raw `sys.*` tables
- Disable or bypass audit

---

## 5. System Catalog (`sys.*`) Rules

You MUST NOT:
- Allow direct SQL `SELECT`, `INSERT`, `UPDATE`, or `DELETE` on `sys.*` tables

You MUST:
- Expose metadata only via engine-owned virtual views or `SHOW` / `DESCRIBE` commands

Engine-owned virtual views:
- Are NOT SQL views
- Are NOT user-definable or alterable
- Are implemented directly in engine code

---

## 6. Security Context Rules

You MUST assume:
- Security Context is immutable for the lifetime of a transaction

You MUST NOT:
- Change roles, privileges, or groups mid-transaction
- Re-evaluate baseline authorization mid-statement unless explicitly using a revocation-sensitive hook

---

## 7. Cryptography and Key Management Rules

### 7.1 General

You MUST:
- Treat key lifecycle correctness as more important than algorithm choice
- Follow normative nonce/IV specifications exactly

You MUST NOT:
- Invent new nonce or IV schemes
- Reuse AES-GCM nonces
- Log plaintext key material

---

### 7.2 Memory Encryption Key (MEK)

You MUST:
- Implement MEK only for Security Level 6

You MUST NOT:
- Depend on MEK behavior at Levels 0–5
- Add partial or optional MEK behavior outside Level 6

---

## 8. Security Levels

You MUST:
- Enforce security levels as an all-or-nothing set

If any required control is missing:
- The system MUST operate at the highest lower valid level

You MUST NOT:
- Mix controls from different levels
- Silently downgrade or upgrade levels

---

## 9. Audit Rules

You MUST:
- Emit audit records for all security-relevant actions
- Follow canonical audit serialization rules

You MUST NOT:
- Skip audit events for performance reasons
- Alter audit records after emission

---

## 10. Non-Negotiable Prohibitions

The following actions are **never permitted**:

- Parser-defined security logic
- SQL-accessible key material
- User-defined access to `sys.*`
- Silent security downgrades
- Disabling audit in secured modes

---

## 11. When in Doubt

If you are unsure whether an implementation is allowed:

1. Assume it is **not allowed**
2. Refer to `SBSEC-ALPHA-BASE.md`
3. Escalate for explicit design approval

Do NOT invent behavior.

---

**End of CONTRIBUTOR_SECURITY_RULES.md**

