# SBSEC-10-RELEASE-INTEGRITY-PROVENANCE.md

**ScratchBird Security Specification – Release Integrity & Provenance**

Version: Alpha-1
Status: Normative

---

## 1. Purpose

This document defines the **security requirements for building, signing, distributing, and verifying ScratchBird binaries**.

It exists to support:
- Certified binary releases
- Official technical support
- Supply‑chain integrity for open source deployments

This document applies to **all official builds** distributed by the ScratchBird project.

---

## 2. Release Authority Model

### 2.1 Source of Truth

- The canonical source code resides in the official ScratchBird repository.
- Release artifacts MUST be traceable to an exact source revision (commit hash).

### 2.2 Build Authority

- Official builds MUST be produced by project‑controlled build infrastructure.
- Community builds MAY exist but MUST NOT be represented as certified.

---

## 3. Reproducible Build Requirements

### 3.1 Determinism

Official builds SHOULD be reproducible.

At minimum:
- Compiler version and flags MUST be recorded
- Build environment identifiers MUST be recorded
- Non‑deterministic inputs MUST be minimized or documented

Perfect reproducibility is a goal, not a hard requirement at Alpha.

---

## 4. Artifact Signing

### 4.1 Signing Keys

- All official artifacts MUST be cryptographically signed.
- Signing keys MUST be project‑owned and access‑controlled.
- Signing key rotation MUST be supported.

### 4.2 Verification

- ScratchBird binaries MUST verify their own signature at startup when enabled.
- Failure to verify MUST be auditable and configurable to fail‑closed.

---

## 5. Software Bill of Materials (SBOM)

### 5.1 SBOM Generation

Each official release MUST include an SBOM describing:
- Included libraries
- Dependency versions
- Cryptographic components

### 5.2 SBOM Integrity

- SBOMs MUST be signed with the same authority as the release artifact.
- SBOMs MUST be version‑matched to the binary.

---

## 6. Update and Patch Policy

### 6.1 Update Channels

- Official update channels MUST enforce signature verification.
- Downgrades across security‑fix releases SHOULD be prevented by default.

### 6.2 Vulnerability Handling

- Security fixes MUST be auditable and traceable.
- Supported versions MUST have a documented patch window.

---

## 7. Support Classification

- **Certified binaries**: signed, supported, eligible for official support
- **Community binaries**: unsigned or third‑party, best‑effort support only

The runtime MAY expose the support classification via an engine‑owned virtual view.

---

## 8. Non‑Goals

This document does NOT require:
- Closed source components
- DRM or license enforcement
- Obfuscation of code or symbols

---

**End of SBSEC-10-RELEASE-INTEGRITY-PROVENANCE.md**

