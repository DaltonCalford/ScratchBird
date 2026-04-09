# Section 40 Time Clocks and Ordering Assumptions

This section is authoritative for ScratchBird's current time-source, ordering, timeout, and locale boundary model.

ScratchBird correctness is not defined by wall-clock time. Transaction truth remains anchored to MGA publication, transaction visibility, commit and rollback state, schema publication, and replay-binding evidence owned by adjacent sections.

## Section scope

time sources and clock authority

timestamp ordering and monotonicity boundaries

clock skew, timeout, and lease boundaries

event ordering and replay-order boundaries

timezone, calendar, and locale boundaries

## Core section rule

Local clocks may annotate, schedule, or format. They do not replace MGA, transaction publication, schema epoch, or replay-binding truth.

## Primary audit lookup anchors

- `src/core/catalog_manager.cpp` search `struct ClockSourceRecord` for the
  catalog-backed declared clock-source model.
- `src/core/catalog_manager.cpp` search
  `CatalogManager::upsertClockSourceCatalogEntry` for clock-source mutation and
  persistence lookup.
- `tests/unit/test_catalog_cluster_clock_extension_contract.cpp` search
  `ClockCatalogContracts` for the maintained direct proof that declared clock
  policy, clock source, node clock state, and clock-violation-event contracts
  remain catalog-backed rather than wall-clock-derived.

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CLOCK_SKEW_TIMEOUT_AND_LEASE_BOUNDARY.md](CLOCK_SKEW_TIMEOUT_AND_LEASE_BOUNDARY.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [EVENT_ORDERING_AND_REPLAY_ORDER_BOUNDARY.md](EVENT_ORDERING_AND_REPLAY_ORDER_BOUNDARY.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TIMESTAMP_ORDERING_AND_MONOTONICITY_BOUNDARY.md](TIMESTAMP_ORDERING_AND_MONOTONICITY_BOUNDARY.md)
- [TIMEZONE_CALENDAR_AND_LOCALE_BOUNDARY.md](TIMEZONE_CALENDAR_AND_LOCALE_BOUNDARY.md)
- [TIME_SOURCES_AND_CLOCK_AUTHORITY.md](TIME_SOURCES_AND_CLOCK_AUTHORITY.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
