# Engine Maintenance Operations and Safety Classes

Status: current_authority

## Current authority

Current section authority covers local scheduler-backed maintenance operations, long-transaction monitoring, sweep-related background work, and local operator safety classification around those operations.

## Current guarantees

- maintenance work is scheduled and executed through current local scheduler paths
- sweep and long-transaction enforcement are local runtime operations
- maintenance operations can be classified and guarded without claiming cluster-scheduler parity

## Non-claims

- full cluster maintenance arbitration
- full distributed maintenance job placement
