# Section 40 Specification Outline

## Owning files

`TIME_SOURCES_AND_CLOCK_AUTHORITY.md`

`TIMESTAMP_ORDERING_AND_MONOTONICITY_BOUNDARY.md`

`CLOCK_SKEW_TIMEOUT_AND_LEASE_BOUNDARY.md`

`EVENT_ORDERING_AND_REPLAY_ORDER_BOUNDARY.md`

`TIMEZONE_CALENDAR_AND_LOCALE_BOUNDARY.md`

`DEPENDENCIES.md`

`DECISION_RECORD.md`

`TEST_CONTRACT.md`

## Section contract

This section owns the current boundary between local clock use and correctness truth.

This section must make it explicit when time is only annotation, only local runtime policy, or entirely non-authoritative for correctness.
