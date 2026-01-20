# Firebird GC/Sweep Glossary (ScratchBird)

## Purpose
Quick reference for Firebird MGA garbage collection terms used across ScratchBird specs.

## Terms

**Sweep**
- A database-wide pass that forces garbage collection.
- Triggered when `(OST - OIT) > sweep_interval`.
- Removes obsolete back versions only; primary record slots remain stable.
- SQL: `SWEEP` (native command). `VACUUM` is a PostgreSQL alias.

**Cooperative GC**
- Opportunistic garbage collection during normal record access.
- Low overhead; runs in the caller's context.

**Background GC Thread**
- Dedicated worker that processes candidate pages when policy allows.
- Uses a read-only, read-committed, no-lock transaction in Firebird.

**GC Policy**
- `cooperative`, `background`, or `combined`.
- Controls whether GC runs during reads, in a background thread, or both.

**Candidate Page Tracking**
- Firebird uses per-relation GC bitmaps (page sequence numbers).
- ScratchBird currently tracks dirty pages globally and prioritizes them.

**OIT / OAT / OST**
- **OIT**: Oldest Interesting Transaction (visibility cutoff for GC).
- **OAT**: Oldest Active Transaction (oldest still-running txn).
- **OST**: Oldest Snapshot Transaction (oldest snapshot still in use).

**Sweep Interval**
- Transaction-count threshold between automatic sweeps.
- Set via `ALTER DATABASE SET SWEEP INTERVAL`.

**VACUUM (PostgreSQL Alias)**
- PostgreSQL compatibility term in ScratchBird.
- Maps to sweep/GC semantics; no PostgreSQL VACUUM phases or VACUUM FULL rewrite.

**SWEEP STATUS (planned)**
- Read-only statement returning sweep statistics and transaction markers.
- Does not perform a sweep; intended for status and scheduling decisions.

## References
- Canonical GC model: `ScratchBird/docs/specifications/transaction/TRANSACTION_MGA_CORE.md`
