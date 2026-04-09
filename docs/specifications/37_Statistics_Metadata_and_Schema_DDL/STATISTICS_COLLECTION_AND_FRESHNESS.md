# Statistics Collection and Freshness

This file owns the bounded statistics maturity statement for ScratchBird.

## Statistics maturity matrix

| Statistics surface | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| directly documented statistics surfaces | current_bounded | only explicitly documented and proven statistics or derived metadata surfaces may be treated as current truth | not a mature optimizer-facing statistics subsystem |
| freshness guarantees | current_bounded | freshness claims are only allowed where another canonical section or bounded evidence surface makes them explicit | not a universal automatic freshness guarantee |
| incremental or adaptive statistics maintenance | fail_closed | no general incremental/adaptive statistics framework is currently claimed | not implied by diagnostics or gate language |
| histogram/MCV/NDV/correlation model | fail_closed | no mature detailed statistics model is currently claimed | not a hidden cost-based optimizer dependency |

## Canonical rules

1. Statistics language must stay narrower than donor-engine optimizer expectations.
2. If a statistics surface is not directly proven, section 37 must keep it partial or fail-closed.
3. Statistics maturity must not be inferred from planner aspirations or future gate planning.
