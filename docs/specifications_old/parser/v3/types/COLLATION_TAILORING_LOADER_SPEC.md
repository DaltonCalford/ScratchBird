# Collation Tailoring Loader (Authoritative)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status:** Authoritative (V3)  
**Target:** Authoritative V3 (loader contract + runtime integration)

## Purpose

Define the file contract for loading collation tailoring data into ScratchBird
and generating runtime collation weight tables.

## Inputs

### 1) UCA Base Weights
- Path: `resources/collations/uca/allkeys.txt`
- Manifest: `resources/collations/uca/uca_manifest.json`

### 2) MySQL Collation Definitions
- Directory: `resources/collations/tailorings/mysql/`
- Format: MySQL charset XML files (e.g., `latin1.xml`, `utf8mb4.xml`)

### 3) Firebird Collation Tables
- Directory: `resources/collations/tailorings/firebird/tables/`
- Format: Firebird collation table headers (e.g., `bl88591da0.h`)

## Resource Layout (Server-Implementable)

The server ships with all collation data in deterministic resource files.

### UCA Weights
- `resources/collations/uca/allkeys.txt`
- `resources/collations/uca/uca_manifest.json` includes sha256 + version metadata

### Tailorings
- `resources/collations/tailorings/mysql/` (MySQL XML)
- `resources/collations/tailorings/firebird/tables/` (Firebird table headers)
- `resources/collations/locales/` (OS/ICU locale snapshot, JSON)

### Version Tracking
- `uca_manifest.json` includes `uca_version` and `data_version`.
- `resources/i18n/version` is the bundle version recorded in catalog metadata.

## Loader Contract (Stub)

### CLI Shape (proposed)
```
sb_collation_loader <db-path> --manifest resources/collations/uca/uca_manifest.json
  [--mysql resources/collations/tailorings/mysql]
  [--firebird resources/collations/tailorings/firebird/tables]
  [--dry-run]
  [--stats]
```

### Required Behaviors
1. **Manifest validation**
   - Verify `uca_manifest.json` exists and hashes match all referenced files.
   - Record UCA version + data_version into catalog metadata.
2. **MySQL collations**
   - Parse charset XML files and register collation names + attributes.
   - Record default collation per charset.
   - Generate SBCL weight files per `COLLATION_RUNTIME_FORMAT.md`.
3. **Firebird collations**
   - Parse Firebird collation table headers and register names + attributes.
   - Collation names must match Appendix H.
   - Generate SBCL weight files per `COLLATION_RUNTIME_FORMAT.md`.
4. **UCA availability**
   - Generate SBCL weight files for UCA-derived collations.

### Loader Workflow (Implementable)
1. Validate `uca_manifest.json` and all referenced files.
2. Register or update UCA metadata in catalog (version + data_version).
3. Parse MySQL XML definitions:
   - Create collations in catalog, preserving names and defaults.
   - Generate SBCL weights for each collation definition.
4. Parse Firebird tables:
   - Register collations using Appendix H names.
   - Generate SBCL weights for each collation definition.
5. Record i18n bundle version from `resources/i18n/version`.

### Parsing and Weight Generation Rules
The loader MUST compute SBCL weight files for all collations:

- **MySQL XML files**:
  - For each `<collation name="...">` entry, register a collation record.
  - Persist `charset`, `case_insensitive`, and `accent_insensitive` flags using
    naming conventions (`_ci`, `_ai`, `_bin`).
- **Firebird collation tables**:
  - Use the header filename (e.g., `bl88591da0.h`) as the table identifier.
  - Map to a collation name using Appendix H resource list.
  - Produce SBCL weights from the Firebird collation tables.

### Output Records (catalog)
Minimum metadata fields (authoritative):
- `sys.collations`:
  - `collation_name` (SBDB$NAME)
  - `charset_name` (SBDB$NAME)
  - `source_engine` (SBDB$NAME_ENUM: `MySQL`|`Firebird`|`UCA`)
  - `uca_version` (SBDB$NAME)
  - `data_version` (SBDB$NAME)
  - `sbcl_blob_id` (SBDB$KEY_TOAST)

## Notes
- Runtime collation weight format is defined in `COLLATION_RUNTIME_FORMAT.md`.
