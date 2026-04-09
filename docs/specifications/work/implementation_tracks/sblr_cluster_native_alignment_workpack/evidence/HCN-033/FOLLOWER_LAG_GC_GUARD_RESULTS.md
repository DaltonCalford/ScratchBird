# Follower Lag GC Guard Results

Validated by `GcSafeHorizonCalculatorTest.MissingOstOrRwmYieldsZeroSafeHorizon`:
- if replication watermark is missing (RWM=0), safe horizon remains 0.
- reclaimability is blocked, preventing GC from overtaking lagging follower state.
