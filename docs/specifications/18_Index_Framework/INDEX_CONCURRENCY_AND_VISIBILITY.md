# Index Concurrency and Visibility

Status: current_authority

## 1. Purpose

Define MGA-first latching, locking, and visibility rules for index operations.

## 2. Dependencies

- 08_Transaction_Core
- 09_Lock_Manager_Core
- 10_GC_and_Sweep

## 3. MGA-first rule

Indexes do not own transaction visibility.

The authoritative rule is:

1. index entries are candidates only
2. transaction visibility is decided from tuple or record version state
3. readers must not rely on index locks to determine visibility
4. dead index entries and superseded index entries may remain until garbage
   collection removes them

This is the Firebird-style MGA rule for index behavior.

## 4. Candidate and visibility pipeline

For every index access path the required pipeline is:

1. navigate the index structure to candidate entries
2. decode candidate key plus row reference
3. fetch the target row or version chain
4. apply MGA visibility rules from section 08
5. if visible, accept the candidate
6. if invisible, ignore the candidate
7. if obsolete and reclaimable, leave it for garbage collection or family-local
   maintenance

An index hit is never by itself proof of row visibility.

## 5. Read concurrency rule

Ordinary index scans must remain MGA readers, not lock-first readers.

That means:

1. readers navigate index pages and row references
2. readers compute visibility from transaction inventory and version lineage
3. readers do not acquire tuple locks merely to read visible data
4. reader access to stale or dead entries is tolerated because visibility
   filtering occurs after candidate fetch

## 6. Write concurrency rule

Index maintenance follows MGA version churn.

Required behavior:

1. an insert adds index entries for the inserted row version
2. an update creates a new row version and new index entries for that version
3. old index entries remain until they are proven reclaimable
4. a delete does not require immediate physical removal of index entries
5. conflicting writes are resolved by the transaction and tuple conflict model,
   not by treating index pages as the main concurrency truth

## 7. Unique conflict rule

Unique enforcement is MGA-aware.

Required behavior:

1. find the conflicting key candidate set
2. inspect referenced row versions
3. if a conflicting committed visible row version exists, fail
4. if the conflict belongs to another active transaction, follow the write-write
   conflict policy from section 09
5. if the conflicting transaction rolls back or becomes non-visible, proceed

Unique enforcement is therefore not a simple key-presence check.

## 8. Structural latches and locks

### In-page latches

- page latches are required for physical page access
- read path uses shared latches
- write path uses exclusive latches
- latch order is root to leaf with bounded coupling

### Structural locks

Structural locks are allowed only as bounded physical-coordination devices.

They must not:

1. become the authoritative visibility mechanism
2. turn ordinary reads into row-lock participants
3. override MGA write-write-only row conflict rules

## 9. Worked MGA examples

### 9.1 Invisible stale entry example

1. row version `V1` is indexed under key `K`
2. later update creates `V2` and indexes `V2` under the same or new key
3. old entry for `V1` may still exist in the index
4. an index scan that reaches `V1` must fetch the row version and apply MGA
   visibility
5. if `V1` is no longer visible to the reader, the candidate is discarded

This is correct MGA behavior. The index is allowed to over-return candidates as
long as the executor applies visibility filtering.

### 9.2 Concurrent unique conflict example

1. `T1` inserts key `K` and remains active
2. `T2` attempts to insert key `K`
3. `T2` inspects the candidate and learns that the conflicting version belongs
   to an active transaction
4. `T2` follows the write-write conflict policy:
   - wait
   - no-wait fail
   - restart
5. if `T1` commits, `T2` fails with unique conflict
6. if `T1` rolls back, `T2` may proceed

## 10. Approximate and layered families

Approximate or layered families still obey MGA visibility.

Required behavior:

1. family-local search may return approximate candidates
2. candidate acceptance still requires MGA visibility checks
3. if a family has mutable and published layers, it must evaluate one coherent
   read view across those layers
4. duplicate suppression and exactness rules are family-local, but MGA
   visibility is not optional

## 11. Garbage collection rule

Index garbage collection is version-reclamation work, not commit truth.

It may remove entries only after:

1. the referenced older version is no longer needed for any visible snapshot or
   retained boundary
2. the family-local reclaim rules are satisfied
3. the removal cannot invalidate an active correct scan contract

## 12. Error handling

Deadlock, lock timeout, or write conflict must return explicit conflict or
restart outcomes without corrupting index state.

## 13. Current code-backed boundary

Current code-backed facts that remain authoritative:

1. B-tree code uses structural page locking for physical traversal and
   modification
2. index search results are candidates and must be filtered by MGA visibility
3. family-local maintenance and exactness may vary by index family

Current non-authoritative or non-universal claims:

1. one universal maintenance-lock scheme across every index family
2. one universal duplicate-suppression algorithm across every layered family

## 14. Implementation contract

Any implementation against this file must prove:

1. index hits are treated as candidates only
2. visible acceptance always depends on MGA row-version rules
3. write-write conflict handling remains subordinate to section 09
4. structural index locking does not become the main transaction-visibility
   mechanism
