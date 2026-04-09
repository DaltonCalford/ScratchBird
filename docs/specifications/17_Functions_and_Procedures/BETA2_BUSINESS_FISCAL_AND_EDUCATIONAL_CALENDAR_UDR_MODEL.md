# Beta 2 Business Fiscal And Educational Calendar UDR Model

## Purpose

This document defines the calendar UDR family for business-day arithmetic,
holiday calendars, fiscal periods, and educational/term calendars.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `Workalendar`.

## Owning package

- `sb_pkg_calendar_udr`

## Mandatory surfaces

The package shall provide:

- business-day add/subtract
- holiday calendars by jurisdiction
- working-day checks
- settlement-day helpers
- fiscal year, quarter, and period mapping
- academic term and session calendar helpers for the admitted educational set

## Required routine families

- `sb_calendar.is_working_day(...)`
- `sb_calendar.add_working_days(...)`
- `sb_calendar.next_business_day(...)`
- `sb_calendar.holidays(...)`
- `sb_calendar.fiscal_period(...)`
- `sb_calendar.term_period(...)`

## Example contract

```sql
select sb_calendar.add_working_days(
    start_date => date '2026-04-03',
    days => 5,
    calendar_id => 'CA-ON-BUSINESS'
);
```

## Operational rules

1. Calendar ids shall be versioned and explicit.
2. Region-specific rules must be data-driven and auditable.
3. Calendar computations shall be deterministic for a given calendar version.

## Explicit exclusions

- remote holiday APIs
- generic scheduling/optimization logic that belongs in `sb_pkg_opt_udr`
