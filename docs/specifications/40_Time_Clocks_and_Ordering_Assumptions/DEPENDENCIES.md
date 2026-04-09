# Section 40 Dependencies

## Purpose

This file defines the adjacent owners that section `40` relies on.

## Adjacent correctness owners

Section `08` owns transaction publication, visibility, and transaction-order truth.

Section `24` and section `37` own committed schema publication and metadata visibility truth.

Section `35` owns startup recovery, restart classification, and reopen ordering truth.

Section `39` owns backup, restore, and bulk-path ordering claims when those paths are relevant.

Section `41` owns platform and OS boundary surfaces that may affect clock source access or host-local behavior.

## Dependency rule

Section `40` may define the boundary for time-related claims, but it shall not absorb correctness ownership from the adjacent sections above.
