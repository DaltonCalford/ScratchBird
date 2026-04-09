# Platform Abstraction and OS Boundary

This file owns the top-level platform abstraction boundary for ScratchBird.

## Platform abstraction matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| current host OS support | current_bounded | support claims remain bounded to current build and gate surfaces | not universal OS parity |
| platform abstraction layers | partial | abstraction exists only where current code and build surfaces explicitly prove it | not a complete portability layer |
| OS-specific behavior | partial | some behavior may remain host-specific even where current support exists | not hidden portability equivalence |
| non-current hosts | fail_closed | unsupported or unproven hosts remain fail-closed | not donor-engine portability claims |

## Canonical rules

1. Platform support claims must be backed by explicit current build or gate evidence.
2. Abstraction language must stay narrower than “runs everywhere” folklore.
3. Any unproven host or target remains fail-closed.

## Explicit non-guarantees

- no universal OS parity claim
- no full hardware-abstraction guarantee
- no promise of behavior identity across platforms
