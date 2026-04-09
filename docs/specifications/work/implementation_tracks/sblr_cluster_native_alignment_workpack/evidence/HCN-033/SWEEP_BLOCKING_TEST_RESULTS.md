# Sweep Blocking Test Results

Validated by `GcSafeHorizonCalculatorTest.ComputesMinOfOstAndRwmAndControlsReclaimability`:
- with gc_safe_horizon=6, version creator 5 is reclaimable.
- version creator 6 is not reclaimable.

This enforces strict MGA visibility preservation at the safe-horizon boundary.
