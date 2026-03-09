# Documentation Workspace

## Coverage and Evidence Status

[![Documentation Integrity](https://img.shields.io/badge/docs%20integrity-100%25%20verified-brightgreen)]()

**Audit Results (2026-03-08):**
- Total files: 912
- Files with source anchors: 843
- Source anchors verified: 1,369
- Test anchors verified: 881
- Verification rate: **100%**

All source code references have been verified against the actual ScratchBird codebase. Unverified claims have been removed.

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/main.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_state_v3.cpp:1`

---

## Documentation Architecture

This documentation follows a two-tier hierarchy:

### 1. Developer Guide (`developers_guide/`)
- High-level architecture explanations
- **Specifications** - Reverse-engineered from implementation with verified source anchors
- Design rationale and integration patterns

### 2. User Documentation 
- `installation/` - Installation and setup
- `language_reference/` - SQL syntax and semantics
- `how-to-guide/` - Task-oriented procedures
- `cli_guide/` - Command-line tool reference
- `configuration_reference/` - Configuration parameters
- `error_and_diagnostics_reference/` - Error codes and troubleshooting
- `upgrade_migration_and_compatibility/`
- `performance_and_capacity_guide/`
- `security_hardening_and_compliance/`
- `disaster_recovery_and_continuity/`
- `release_notes_and_support_policy/`

---

This directory contains the first-pass public-beta documentation scaffold.

## Guide Directories

### User-Facing Guides
- [installation/](installation/README.md) - Installation and setup instructions
- [how-to-guide/](how-to-guide/README.md) - Task-oriented procedures
- [cli_guide/](cli_guide/README.md) - Command-line tool reference
- [language_reference/](language_reference/README.md) - SQL syntax and semantics
- [configuration_reference/](configuration_reference/README.md) - Configuration parameters
- [error_and_diagnostics_reference/](error_and_diagnostics_reference/README.md) - Error codes and troubleshooting

### Developer Resources
- [developers_guide/](developers_guide/README.md) - Architecture, internals, and development
  - **Specifications** (in dev guide) - Reverse-engineered specs with source anchors to actual code

### Operations & Governance
- [upgrade_migration_and_compatibility/](upgrade_migration_and_compatibility/README.md)
- [performance_and_capacity_guide/](performance_and_capacity_guide/README.md)
- [security_hardening_and_compliance/](security_hardening_and_compliance/README.md)
- [disaster_recovery_and_continuity/](disaster_recovery_and_continuity/README.md)
- [release_notes_and_support_policy/](release_notes_and_support_policy/README.md)

## Authoring Intent

These documents are structured for incremental completion by documentation contributors.
Each file defines scope and required content with navigation links.

All source anchors must point to files within the ScratchBird project (`src/`, `tests/`).
