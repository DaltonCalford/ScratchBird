# Resources Remediation Plan: Timezones, Charsets, Collations

## Purpose
Bring ScratchBird resources and loader tooling to parity with Firebird, MySQL,
and PostgreSQL requirements for timezones, character sets, and collations.

## Inputs
- Findings: `ScratchBird/docs/findings/RESOURCES_I18N_TIMEZONE_AUDIT.md`
- Specs:
  - `ScratchBird/docs/specifications/types/character_sets_and_collations.md`
  - `ScratchBird/docs/specifications/types/TIMEZONE_SYSTEM_CATALOG.md`

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
- [ ] Confirm canonical charset list for Firebird (Appendix H), MySQL 8.x, PostgreSQL
- [ ] Confirm canonical collation lists per engine (minimum defaults + required)
- [ ] Define alias mapping rules for name differences (Firebird/PG/MySQL)
- [ ] Define tzdata version tracking location (config key or catalog record)

### M2. Resource Data Expansion
- [ ] Expand `resources/charsets/charsets.json` to include baseline charsets + aliases
- [ ] Expand `resources/collations/collations.json` to include baseline collations
- [ ] Add a minimal OS/ICU collation ingestion strategy (placeholder list + hooks)
- [ ] Add resource QA script to validate uniqueness, alias coverage, and references

### M3. Loader Tooling Alignment
- [ ] Restore or replace `sb_charset_loader` (resolve OpenSSL link or new loader)
- [ ] Align `sb_timezone_loader` update workflow with docs (no `--replace` flag)
- [ ] Add tzdata version write/update step to loader workflow

### M4. Catalog Integration
- [ ] Ensure charset/collation records are persisted into catalog tables
- [ ] Ensure timezone catalog is loaded from tzdata (not hardcoded)
- [ ] Add catalog schema versioning checks for i18n resources

### M5. Verification and Audit
- [ ] Add a conformance report: Firebird/MySQL/PostgreSQL charset coverage
- [ ] Add a conformance report: Firebird/MySQL/PostgreSQL collation coverage
- [ ] Add a tzdata version report (from catalog)

## Dependencies
- Catalog structure for charsets/collations/timezones must be stable.
- Loader tools need to be buildable and invoked by init or admin workflows.

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
