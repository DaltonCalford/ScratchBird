# Collation Tailoring Loader (Minimal Stub Spec)

**Status:** Draft  
**Target:** Alpha (loader contract), Beta (runtime integration)

## Purpose

Define the minimal file contract for loading collation tailoring data into
ScratchBird. This spec focuses on the ingestion interface and on-disk formats
so implementers can build loader tooling before full runtime collation support
is complete.

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
   - If no collation weights are generated, mark the collation as `stub_only`.
3. **Firebird collations**
   - Parse Firebird collation table headers and register names + attributes.
   - Collation names must match Appendix H.
   - If no weights are generated, mark the collation as `stub_only`.
4. **UCA availability**
   - If UCA weight tables are present, register `uca_available=true`.

### Loader Workflow (Implementable)
1. Validate `uca_manifest.json` and all referenced files.
2. Register or update UCA metadata in catalog (version + data_version).
3. Parse MySQL XML definitions:
   - Create collations in catalog, preserving names and defaults.
   - Mark `stub_only` when weight data is not present.
4. Parse Firebird tables:
   - Register collations using Appendix H names.
   - Mark `stub_only` when weight data is not present.
5. Record i18n bundle version from `resources/i18n/version`.

### Minimal Parsing Rules (Stub)
The loader does not compute weights yet; it only extracts metadata:

- **MySQL XML files**:
  - For each `<collation name="...">` entry, register a collation record.
  - Persist `charset`, `case_insensitive`, and `accent_insensitive` flags using
    naming conventions (`_ci`, `_ai`, `_bin`).
- **Firebird collation tables**:
  - Use the header filename (e.g., `bl88591da0.h`) as the table identifier.
  - Map to a collation name using Appendix H resource list.
  - Mark as `stub_only` unless a compiled weight table exists.

### Output Records (catalog)
Minimum metadata fields (schema names are placeholders):
- `sys.collations`:
  - `collation_name`
  - `charset_name`
  - `source_engine` (`MySQL`/`Firebird`/`UCA`)
  - `uca_version`
  - `data_version`
  - `stub_only` (boolean)

## Notes
- This stub spec does not define the runtime collation weight format.
- A full implementation can later extend the loader to generate binary collation
  data files alongside the catalog records.
