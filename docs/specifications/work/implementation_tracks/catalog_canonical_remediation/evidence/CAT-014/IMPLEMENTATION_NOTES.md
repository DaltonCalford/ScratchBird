# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-014 catalog root fields and persistence wiring:
  - `reserved_words_page`
  - `emulation_profile_page`
  - `parser_profiles_page`
  - `parser_capability_entries_page`
  - `parser_transform_entries_page`
  - `parser_error_map_entries_page`
  - `parser_feature_precedence_page`
- Added bootstrap allocation and legacy backfill allocation for all CAT-014 tables.
- Added on-disk record contracts in `CatalogManager`:
  - `ReservedWordRecord`
  - `EmulationProfileRecord`
  - `ParserProfileRecord`
  - `ParserCapabilityRecord`
  - `ParserTransformRecord`
  - `ParserErrorMapRecord`
  - `ParserFeaturePrecedenceRecord`
- Added full CAT-014 CRUD/public APIs for:
  - `reserved_words`
  - `emulation_profile`
  - `parser_profile`
  - `parser_capability_entry`
  - `parser_transform_entry`
  - `parser_error_map_entry`
  - `parser_feature_precedence`
- Enforced deterministic contracts:
  - required-field constraints for all catalog families.
  - enum/domain validity gates for engine/profile/action/stage/severity/tiebreak fields.
  - one-default-parser-profile-per-engine enforcement.
  - parser-transform deterministic requirements for ordered stage pipelines.
  - parser-capability action-conditional parameter constraints.
  - unique tuple guards for identity/preference contracts.
- Added CAT-014 bootstrap persistence gate and parser capability CRUD contract tests.
