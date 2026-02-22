# Resources Remediation Plan: Timezones, Charsets, Collations

## Purpose
Bring ScratchBird resources and loader tooling to parity with Firebird, MySQL,
and PostgreSQL requirements for timezones, character sets, and collations.

## Inputs
- Findings: `ScratchBird/docs/findings/RESOURCES_I18N_TIMEZONE_AUDIT.md`
- Specs:
  - `ScratchBird/docs/specifications/types/character_sets_and_collations.md`
  - `ScratchBird/docs/specifications/types/TIMEZONE_SYSTEM_CATALOG.md`
  - `ScratchBird/docs/specifications/types/COLLATION_TAILORING_LOADER_SPEC.md`

## Scope
- Resource data files under `ScratchBird/resources/`
- Loader tooling (`sb_timezone_loader`, `sb_charset_loader` or replacement)
- Catalog metadata for tzdata version and charset/collation registration

## Out of Scope (for this plan)
- Runtime collation algorithms (ICU integration details)
- Parser changes
- Wire protocol changes

## Milestones and Checklist

### M1. Baseline Definition (Spec-Only)
- [x] Confirm canonical charset list for Firebird (Appendix H), MySQL 8.x, PostgreSQL
- [x] Confirm canonical collation lists per engine (minimum defaults + required)
- [x] Define alias mapping rules for name differences (Firebird/PG/MySQL)
- [x] Define tzdata version tracking location (config key or catalog record)

### M2. Resource Data Expansion
- [x] Expand `resources/charsets/charsets.json` to include baseline charsets + aliases
- [x] Expand `resources/collations/collations.json` to include baseline collations
- [x] Add generator script to refresh resources from upstream references
- [x] Add collation tailoring ingestion script (MySQL XML + Firebird tables)
- [x] Add mapping validation script (iconv + system charmaps)
- [x] Add charset mapping tables for all non-Unicode encodings
  - Location: `resources/charsets/mappings/`
  - Required coverage: every charset entry in `charsets.json` that is not UTF-8/UTF-16/UTF-32
  - Format: JSON mapping tables as defined in
    `ScratchBird/docs/specifications/types/character_sets_and_collations.md`
    (byte sequence -> Unicode codepoint)
  - Sources:
    - Unicode mapping tables: https://www.unicode.org/Public/MAPPINGS/
    - MySQL charsets: MySQL source `share/charsets/` for codepage tables
    - PostgreSQL encodings: PostgreSQL source `src/encoding/` tables
    - Firebird charsets: Firebird source charset tables (for legacy aliases)
- [x] Add multibyte conversion tables for Shift_JIS, EUC-JP, GBK, GB18030, Big5,
  EUC-KR, CP943C, SJIS_0208, EUCJ_0208, KSC_5601
  - Present: Shift_JIS, EUC-JP, GBK, Big5, EUC-KR, CP943C, SJIS_0208, EUCJ_0208, KSC_5601
  - Added: GB18030 (full charmap coverage from system GB18030.gz)
  - Include validation rules for illegal byte sequences
  - Document fallback behavior for unmapped sequences (reject vs replacement)
- [x] Add mapping table JSON schema at
  `resources/charsets/mappings/charset_mapping.schema.json`
  - UNICODE_FSS is treated as UTF-8 with a 3-byte ceiling (no mapping table).
- [x] Add a minimal OS/ICU collation ingestion strategy (placeholder list + hooks)
- [x] Add a collation data strategy that is fully implementable by the server:
  - **Binary collations:** no external data required (byte-compare).
  - **Unicode collations (UCA):**
    - Store UCA weight tables under `resources/collations/uca/`
    - Store per-collation tailorings under `resources/collations/tailorings/`
    - Track UCA version and collation data version in a manifest file
  - **MySQL collations:**
    - Use MySQL 8.x UCA tables + tailorings as the canonical source
    - Preserve MySQL collation names and defaults per charset
  - **Firebird collations:**
    - Use Firebird collation tables or documented rules to build tailorings
    - Preserve Firebird collation names from Appendix H
  - **PostgreSQL collations:**
    - Prefer OS/ICU locale collations; store locale inventory snapshots under
      `resources/collations/locales/` for reproducibility
  - Provide a loader workflow for UCA/tailoring data (doc-only)
- [x] UCA base weights installed at `resources/collations/uca/allkeys.txt`
- [x] UCA manifest created at `resources/collations/uca/uca_manifest.json`
- [x] Stub tailoring index lists added for MySQL defaults and Firebird Appendix H
- [x] MySQL collation XML files ingested under `resources/collations/tailorings/mysql/`
- [x] Firebird collation tables ingested under `resources/collations/tailorings/firebird/tables/`
- [x] Add resource QA script to validate uniqueness, alias coverage, and references

#### UCA/Tailoring Manifest (Minimal Schema)
Store a single manifest file at `resources/collations/uca/uca_manifest.json`:

```json
{
  "uca_version": "15.1.0",
  "data_version": "2026-01-28",
  "source": "CLDR/UCA",
  "files": [
    {
      "type": "weights",
      "path": "resources/collations/uca/allkeys.txt",
      "sha256": "<hex>",
      "format": "allkeys"
    },
    {
      "type": "tailoring",
      "path": "resources/collations/tailorings/mysql/utf8mb4_0900_ai_ci.txt",
      "sha256": "<hex>",
      "format": "uca-tailoring",
      "collation_name": "utf8mb4_0900_ai_ci",
      "charset": "UTF-8",
      "derived_from": "UCA-15.0.0"
    }
  ]
}
```

Required fields:
- `uca_version`: Unicode Collation Algorithm version.
- `data_version`: resource package version (date or semver).
- `source`: canonical origin (e.g., `CLDR/UCA`, `MySQL 8.0`).
- `files[]`: list of weight/tailoring files with integrity hash and format.

#### Charset Mapping Table Naming Convention
Place mapping tables in `resources/charsets/mappings/` using:

```
<canonical-charset>.map.json
```

Rules:
- `canonical-charset` is the normalized resource name from `charsets.json`.
- Use ASCII lowercase with hyphens, e.g.:
  - `windows-1252.map.json`
  - `iso-8859-1.map.json`
  - `koi8-r.map.json`
  - `gbk.map.json`
  - `shift-jis.map.json`
- Multibyte encodings must include `invalid_sequences` and `replacement` policy
  fields (per `character_sets_and_collations.md`), so the loader can validate.


### M3. Loader Tooling Alignment
- [x] Restore or replace `sb_charset_loader` (resolve OpenSSL link or new loader)
- [x] Align `sb_timezone_loader` update workflow with docs (no `--replace` flag)
- [x] Add tzdata version write/update step to loader workflow

### M4. Catalog Integration
- [x] Ensure charset/collation records are persisted into catalog tables
- [x] Ensure timezone catalog is loaded from tzdata (not hardcoded)
  - Runtime now prefers catalog-loaded timezones when populated.
- [x] Add catalog schema versioning checks for i18n resources

### M5. Verification and Audit
- [x] Add a conformance report: Firebird/MySQL/PostgreSQL charset coverage
- [x] Add a conformance report: Firebird/MySQL/PostgreSQL collation coverage
- [x] Add a tzdata version report (from catalog)

## Dependencies
- Catalog structure for charsets/collations/timezones must be stable.
- Loader tools need to be buildable and invoked by init or admin workflows.
- ICU (or equivalent UCA data source) availability if Unicode collations are
  implemented without static tables.

## Risks
- High volume of collations (MySQL/PG) may be better served by ICU/OS locale
  ingestion rather than static lists.
- Charset aliasing may introduce ambiguity; enforce canonical name + alias mapping.

## Owners
- Engine/Resources: TBD
- Tools: TBD
- Documentation: TBD

## Status
Draft (Alpha remediation)
