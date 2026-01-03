# SUPPORTABILITY_CONTRACT.md

**ScratchBird Supportability Contract**

Version: Alpha-1
Status: Normative for Supported Deployments

---

## 1. Purpose

This document defines the **minimum observability, diagnostics, and operational guarantees** required for ScratchBird deployments to be eligible for official technical support.

It aligns security correctness with real‑world operability.

---

## 2. Supported Deployment Definition

A deployment is considered **supported** if:
- It runs a certified binary
- It operates within declared security level constraints
- It does not bypass engine security controls

Unsupported deployments MAY function but are not eligible for official support.

---

## 3. Diagnostic Surface Contract

### 3.1 Engine‑Owned Virtual Views

Supported deployments MUST expose a stable set of engine‑owned virtual views, including at minimum:

- `SHOW SECURITY LEVEL`
- `SHOW ACTIVE ROLES`
- `SHOW EFFECTIVE PRIVILEGES`
- `SHOW AUDIT STATUS`
- `SHOW KEY STATUS`
- `SHOW CLUSTER STATUS` (if clustered)

These views:
- Are NOT SQL views
- Are versioned and backward‑compatible within a major release

---

## 4. Deception / Honeypot Transparency

If deceptive behaviors are enabled:

- The engine MUST emit explicit audit events indicating deception
- A diagnostic command MUST report deception status (e.g., `SHOW DECEPTION STATUS`)

This prevents ambiguity during support investigations.

---

## 5. Audit Requirements for Support

Supported deployments MUST:
- Enable audit logging appropriate to their security level
- Retain audit logs for the documented retention window

Audit suppression invalidates support eligibility.

---

## 6. Failure Mode Disclosure

### 6.1 Irrecoverable Modes (Level 6)

If Level 6 features (e.g., NPB / MEK presence binding) are enabled:

- Operators MUST acknowledge that data loss may be irrecoverable
- Tooling MUST warn prior to enablement
- Documentation MUST describe expected failure behavior

---

## 7. Configuration Disclosure

For support requests, operators MUST be able to provide:
- Active security level
- Enabled features
- Key management configuration (high‑level, non‑secret)
- Cluster quorum configuration (if applicable)

---

## 8. Versioning and Compatibility

- Diagnostic commands and virtual views MUST remain stable within a major version
- Breaking changes MUST be documented

---

## 9. Non‑Supported Conditions

The following invalidate support eligibility:

- Custom patches to engine security code
- Disabled audit in secured modes
- Direct access to `sys.*` tables
- Uncertified binaries

---

## 10. Escalation and Resolution

If an issue cannot be diagnosed within the supported surface:

- The issue MAY be escalated to deeper investigation
- Additional access requires explicit operator approval and audit

---

**End of SUPPORTABILITY_CONTRACT.md**

