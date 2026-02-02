# Resources Audit: Timezones, Charsets, Collations
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


## Scope
- Resources: `ScratchBird/resources/timezones/`, `ScratchBird/resources/charsets/`, `ScratchBird/resources/collations/`
- Loader tools (read-only code): `ScratchBird/tools/sb_timezone_loader.cpp`, `ScratchBird/tools/sb_charset_loader.cpp`
- Firebird reference list: `ScratchBird/docs/specifications/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md`

## Current Inventory (code-truth)
- IANA tzdata version present: `2025c` (`ScratchBird/resources/timezones/version`).
- Charset definitions: 94 entries in `ScratchBird/resources/charsets/charsets.json`.
- Collation definitions: 277 entries in `ScratchBird/resources/collations/collations.json`.
- `sb_timezone_loader` exists and supports `--from`, `--file`, `--stats` only.
  - Code: `ScratchBird/tools/sb_timezone_loader.cpp:1-120`
- `sb_charset_loader` exists but is marked deprecated and does not compile due to missing OpenSSL linkage.
  - Code: `ScratchBird/tools/sb_charset_loader.cpp:1-36`

## Findings

### F-RES-001 Firebird charset coverage is incomplete in resources
Firebird Appendix H lists these character sets (baseline), which were missing from
`resources/charsets/charsets.json` (by name or alias):
- CP943C, CYRL
- DOS437, DOS737, DOS775, DOS850, DOS852, DOS857, DOS858, DOS860, DOS861, DOS862,
  DOS863, DOS864, DOS865, DOS866, DOS869
- EUCJ_0208, SJIS_0208, KSC_5601, GB_2312
- NONE, OCTETS, UNICODE_FSS
- BIG_5, NEXT
- Firebird name aliases for WIN1250/WIN1251/WIN1252/WIN1253 (resources only expose
  Windows-1250/1251/1252/1253 without Firebird-style aliases)
**Resolved:** Added Firebird baseline charsets + aliases.
- Firebird list: `ScratchBird/docs/specifications/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md`
- Resources: `ScratchBird/resources/charsets/charsets.json`

### F-RES-002 Firebird collation coverage is far below baseline
`resources/collations/collations.json` previously contained only a handful of Firebird
collations (e.g., `UTF8_UNICODE`, `UTF8_UNICODE_CI`, `WIN1252_UNICODE`, `WIN1252_UNICODE_CI`),
but Appendix H enumerates many more collations per charset (e.g., PXW_*,
DB_* variants, UNICODE_CI_AI, UCS_BASIC, etc.).
- Firebird list: `ScratchBird/docs/specifications/reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md`
**Resolved:** Added Appendix H collation baseline to resources.
- Resources: `ScratchBird/resources/collations/collations.json`

### F-RES-003 MySQL and PostgreSQL charset baselines are not met
The resource charset list is missing widely used MySQL/PostgreSQL encodings and
aliases. Examples (non-exhaustive, verify against official lists):
- MySQL: `utf8mb4`, `utf8mb3`, `ucs2`, `utf16le`, `binary`, `gb2312`,
  `cp850`, `cp852`, `cp866`, `cp932`, `eucjpms`, `macroman`, `macce`,
  `armscii8`, `dec8`, `hp8`, `swe7`, `keybcs2`, `geostd8`
- PostgreSQL: `SQL_ASCII`, `MULE_INTERNAL`, `LATIN6/7/8/9/10/14/16`,
  `WIN866`, `WIN874`, `EUC_JIS_2004`, `EUC_CN`, `EUC_TW`
**Resolved:** Added missing MySQL/PostgreSQL charset names and alias handling.
- Resources: `ScratchBird/resources/charsets/charsets.json`
- Loader aliases: `ScratchBird/src/core/charset_loader.cpp`

### F-RES-004 Collation coverage is not sufficient for MySQL/PostgreSQL
MySQL 8.x provides extensive per-charset collations; PostgreSQL collations are
locale-driven (OS/ICU). The current resources file has 30 entries and does not
represent either baseline.
**Resolved:** Expanded baseline collations and loader alias support.
- Resources: `ScratchBird/resources/collations/collations.json`

### F-RES-005 Loader/tooling mismatch for charsets/collations
The documented `sb_charset_loader` is deprecated and does not compile, while
the resources README still instructs users to run it.
**Resolved:** Loader is active and docs reflect current usage.
- Code: `ScratchBird/tools/sb_charset_loader.cpp`
- Doc: `ScratchBird/resources/README.md`

### F-RES-006 Timezone loader options mismatch in docs
`sb_timezone_loader` does not expose `--replace`, but resources README instructs
its use for updates.
**Resolved:** Documentation now matches loader flags.
- Code: `ScratchBird/tools/sb_timezone_loader.cpp:1-120`
- Doc: `ScratchBird/resources/README.md`

### F-RES-007 Broken reference to archived data loader plan
Resources README references a planning doc that has been archived.
**Resolved:** References now point at the active remediation plan.
- Doc: `ScratchBird/resources/README.md`

### F-RES-008 Charset mapping tables missing for non-Unicode encodings
The resources baseline now includes charset names/aliases, but conversion tables
were not present for non-Unicode encodings.
**Resolved:** Mapping JSON tables ingested under
`ScratchBird/resources/charsets/mappings/` for ISO-8859, Windows-125x, KOI8,
DOS code pages (including DOS858), MacRoman/MacCE, CP943C (iconv IBM943),
Shift_JIS, Big5, EUC-JP, EUC-KR, GBK, and GB2312 (from system charmap).
UNICODE_FSS is treated as UTF-8 with a 3-byte ceiling (no mapping table).
- Resources: `ScratchBird/resources/charsets/mappings/`

### F-RES-009 Collation tailoring data missing for MySQL/Firebird
Baseline collations exist in JSON, but no tailoring data was available for
implementers to build real collation weight tables.
**In Progress:** MySQL charset XML files and Firebird collation tables have been
ingested under `resources/collations/tailorings/`, with a UCA manifest entry per
file. Full UCA tailoring/ICU integration is still required for runtime use.
- Resources: `ScratchBird/resources/collations/tailorings/`
- Manifest: `ScratchBird/resources/collations/uca/uca_manifest.json`

## Recommended Spec Actions
1. Maintain the Firebird/MySQL/PostgreSQL baseline lists as new upstream versions are released.
2. Document the long-term collation ingestion strategy (ICU/OS locales) for large MySQL/PG sets.
3. Keep loader tooling and resources/README aligned as CLI flags evolve.
