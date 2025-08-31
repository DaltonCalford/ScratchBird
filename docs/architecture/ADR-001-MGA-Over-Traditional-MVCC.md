# ADR-001: Multi-Generational Architecture Over Traditional MVCC

## Status
Accepted

## Context
We need to choose a concurrency control mechanism for ScratchBird. The main options are:
1. Traditional lock-based 2PL (Two-Phase Locking)
2. Traditional MVCC with locks for writers
3. PostgreSQL-style MVCC with snapshots
4. Firebird-style MGA (Multi-Generational Architecture)

## Decision
We will use Firebird's Multi-Generational Architecture (MGA) as the primary concurrency control mechanism.

## Rationale

### MGA Advantages

1. **No Read Locks Ever**
   - Readers never block writers
   - Writers never block readers
   - Massive concurrency for read-heavy workloads

2. **Natural Versioning**
   - Every update creates a new version naturally
   - No additional overhead for MVCC
   - Version chains are part of the architecture

3. **Simple Rollback**
   - Rollback just marks transaction as aborted in TIP
   - No undo log needed
   - No complex rollback logic

4. **WAL Optional**
   - ACID (except Durability) works without WAL
   - Can run in-memory mode
   - WAL only adds durability, not correctness

5. **Proven Architecture**
   - Used successfully in Firebird for 40+ years
   - InterBase heritage from 1985
   - Battle-tested in production

### Comparison Matrix

| Feature | Traditional 2PL | PostgreSQL MVCC | Firebird MGA |
|---------|----------------|-----------------|--------------|
| Read locks | Yes | No | No |
| Write locks | Yes | Yes | Yes (only writers) |
| Versions per row | 1 | Multiple | Multiple |
| Rollback mechanism | Undo log | Undo log | Mark aborted |
| WAL required | Yes | Yes | No |
| Garbage collection | No | Yes (vacuum) | Yes (sweep) |
| Memory overhead | Low | Medium | Medium |

### Trade-offs

**Costs:**
- Garbage collection required (old versions accumulate)
- Slightly higher storage overhead (version chains)
- More complex visibility rules

**Benefits:**
- Superior read concurrency
- Simpler transaction abort
- Optional durability (WAL can be disabled)
- Natural time-travel queries

## Implementation Notes

1. **Transaction Inventory Pages (TIP)**
   - Track transaction states (Active, Committed, Aborted)
   - Persistent structure (survives crashes)
   - No WAL required for correctness

2. **Version Chains**
   - Each tuple has created_xid and deleted_xid
   - Updates create new version with backptr to old
   - Deletes just mark deleted_xid

3. **Garbage Collection**
   - Background sweep process
   - Remove versions not visible to any transaction
   - Can be scheduled or triggered

## Consequences

### Positive
- Best-in-class read concurrency
- Simpler transaction management
- Flexibility in durability choices
- Natural foundation for time-travel

### Negative
- Must implement garbage collection
- Storage overhead from versions
- Different from mainstream databases (learning curve)
- Some tools may not understand MGA

## References
- Firebird Architecture Guide
- "InterBase: Architecture and Implementation" (1991)
- "A Multi-Generational Architecture for Database Management Systems" (1984)