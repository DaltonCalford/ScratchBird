# Resources Audit: Timezones, Charsets, Collations

## Scope
- Resources: `ScratchBird/resources/timezones/`, `ScratchBird/resources/charsets/`, `ScratchBird/resources/collations/`
- Loader tools (read-only code): `ScratchBird/tools/sb_timezone_loader.cpp`, `ScratchBird/tools/sb_charset_loader.cpp`
- Firebird reference list: `ScratchBird/docs/specifications/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md`

## Current Inventory (code-truth)
- IANA tzdata version present: `2024b` (`ScratchBird/resources/timezones/version`).
- Charset definitions: 36 entries in `ScratchBird/resources/charsets/charsets.json`.
- Collation definitions: 30 entries in `ScratchBird/resources/collations/collations.json`.
- `sb_timezone_loader` exists and supports `--from`, `--file`, `--stats` only.
  - Code: `ScratchBird/tools/sb_timezone_loader.cpp:1-120`
- `sb_charset_loader` exists but is marked deprecated and does not compile due to missing OpenSSL linkage.
  - Code: `ScratchBird/tools/sb_charset_loader.cpp:1-36`

## Findings

### F-RES-001 Firebird charset coverage is incomplete in resources
Firebird Appendix H lists these character sets (baseline), which are missing from
`resources/charsets/charsets.json` (by name or alias):
- CP943C, CYRL
- DOS437, DOS737, DOS775, DOS850, DOS852, DOS857, DOS858, DOS860, DOS861, DOS862,
  DOS863, DOS864, DOS865, DOS866, DOS869
- EUCJ_0208, SJIS_0208, KSC_5601, GB_2312
- NONE, OCTETS, UNICODE_FSS
- BIG_5, NEXT
- Firebird name aliases for WIN1250/WIN1251/WIN1252/WIN1253 (resources only expose
  Windows-1250/1251/1252/1253 without Firebird-style aliases)
  - Firebird list: `ScratchBird/docs/specifications/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md`
  - Resources: `ScratchBird/resources/charsets/charsets.json`

### F-RES-002 Firebird collation coverage is far below baseline
`resources/collations/collations.json` contains a handful of Firebird collations
(e.g., `UTF8_UNICODE`, `UTF8_UNICODE_CI`, `WIN1252_UNICODE`, `WIN1252_UNICODE_CI`),
but Appendix H enumerates many more collations per charset (e.g., PXW_*,
DB_* variants, UNICODE_CI_AI, UCS_BASIC, etc.). These are not represented in
resources.
- Firebird list: `ScratchBird/docs/specifications/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md`
- Resources: `ScratchBird/resources/collations/collations.json`

### F-RES-003 MySQL and PostgreSQL charset baselines are not met
The resource charset list is missing widely used MySQL/PostgreSQL encodings and
aliases. Examples (non-exhaustive, verify against official lists):
- MySQL: `utf8mb4`, `utf8mb3`, `ucs2`, `utf16le`, `binary`, `gb2312`,
  `cp850`, `cp852`, `cp866`, `cp932`, `eucjpms`, `macroman`, `macce`,
  `armscii8`, `dec8`, `hp8`, `swe7`, `keybcs2`, `geostd8`
- PostgreSQL: `SQL_ASCII`, `MULE_INTERNAL`, `LATIN6/7/8/9/10/14/16`,
  `WIN866`, `WIN874`, `EUC_JIS_2004`, `EUC_CN`, `EUC_TW`
Resources: `ScratchBird/resources/charsets/charsets.json`

### F-RES-004 Collation coverage is not sufficient for MySQL/PostgreSQL
MySQL 8.x provides extensive per-charset collations; PostgreSQL collations are
locale-driven (OS/ICU). The current resources file has 30 entries and does not
represent either baseline.
Resources: `ScratchBird/resources/collations/collations.json`

### F-RES-005 Loader/tooling mismatch for charsets/collations
The documented `sb_charset_loader` is deprecated and does not compile, while
the resources README still instructs users to run it.
- Code: `ScratchBird/tools/sb_charset_loader.cpp:1-36`
- Doc: `ScratchBird/resources/README.md:256-264`

### F-RES-006 Timezone loader options mismatch in docs
`sb_timezone_loader` does not expose `--replace`, but resources README instructs
its use for updates.
- Code: `ScratchBird/tools/sb_timezone_loader.cpp:1-120`
- Doc: `ScratchBird/resources/README.md:240-252`

### F-RES-007 Broken reference to archived data loader plan
Resources README references a planning doc that has been archived.
- Doc: `ScratchBird/resources/README.md:151`
- Archived file: `ScratchBird/docs/archive/2026-01-04/planning/old_Plans/archive/DATA_LOADERS_IMPLEMENTATION_PLAN.md`

## Recommended Spec Actions
1. Define a baseline charset/collation import policy: Firebird Appendix H list +
   MySQL 8.x + PostgreSQL encodings as minimum coverage, with engine-specific alias
   mapping.
2. Document a collation ingestion strategy (ICU/OS locales) instead of static lists
   for PostgreSQL and high-volume MySQL collations.
3. Reconcile loader tooling: clarify `sb_charset_loader` status or replace with
   a maintained loader for resource JSON ingestion.
4. Align documentation with actual loader flags (`sb_timezone_loader`).
