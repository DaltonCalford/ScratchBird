# Firebird GC/Sweep Glossary (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose
Concise, Firebird-accurate definitions for GC and sweep terminology.

## Terms

**MGA (Multi-Generational Architecture)**
Firebird-style MVCC that stores multiple versions of a record on-page and resolves
visibility via TIP and snapshots. No WAL replay is required for visibility recovery.

**TIP (Transaction Inventory Pages)**
Pages that store 2-bit states for transactions (active, committed, dead, limbo).
TIP is the authoritative source of transaction state.

**OIT (Oldest Interesting Transaction)**
Oldest transaction whose state still matters for visibility. Versions older than OIT
may be garbage if no snapshot requires them.

**OAT (Oldest Active Transaction)**
Oldest currently active transaction.

**OST (Oldest Snapshot Transaction)**
Oldest snapshot still in use. Derived from active transactions’ snapshots and used
for garbage collection thresholds.

**Cooperative GC**
Garbage collection performed opportunistically during normal record access. Readers
and writers prune back-versions they encounter when safe.

**Background GC Thread**
Dedicated GC thread that processes candidate pages collected in per-relation GC
bitmaps. Attachments notify it when garbageable pages are encountered.

**Sweep**
Database-wide scan that advances OIT and reclaims leftover garbage. Triggered
automatically when the transaction gap exceeds sweep interval per Firebird’s logic:
`(oldest_active_snapshot - OIT) > sweep_interval` and no limbo is involved.

**Sweep Interval**
Transaction-count threshold configured by `ALTER DATABASE SET SWEEP INTERVAL`.

**GC Horizon (Firebird)**
The oldest active snapshot, computed via transaction locks (`LCK_tra`) and used to
decide which versions are safe to remove. This is not xmin-style.

## References
- Firebird transaction/sweep: `firebird/src/jrd/tra.cpp`
- Firebird GC: `firebird/src/jrd/vio.cpp`
- Read consistency and snapshots: `firebird/doc/README.read_consistency.md`

**Terminology note:** ScratchBird uses Firebird MGA. Any MGA references in this file are legacy shorthand and must be interpreted as MGA per the authoritative references above.
