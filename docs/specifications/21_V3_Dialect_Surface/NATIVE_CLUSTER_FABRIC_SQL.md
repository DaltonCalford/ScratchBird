# Native Cluster Fabric SQL

## Current code-backed truth
- Parser extension tests prove some cluster-control and replication statement surfaces.
- Section `17` owns the bounded cluster-fabric runtime boundary and currently fails closed on live transport or task execution guarantees.

## Boundary
- This file may claim parser-front-door presence where tests exist.
- This file may not claim live cluster-fabric execution parity on its own.
- Treat cluster-fabric SQL as `partial`: parser proof present, runtime closure bounded elsewhere.
- package `03` keeps cluster-fabric SQL at this parser-front-door boundary and
  defers live transport or task-runtime closure to later work
