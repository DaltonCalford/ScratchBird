# Engine Thread, Worker, and Task Model

Status: current_authority

## Current authority

Current code-backed authority is the local worker and task runtime centered on the thread pool and local task queueing surfaces used by listeners, parser assignment, and background work.

## Current guarantees

- local worker threads and queued task execution exist
- task dispatch and scheduling are local-runtime concerns, not distributed scheduler semantics
- queueing, priorities, and worker assignment are bounded by the current thread-pool and nearby runtime code

## Non-claims

- a cluster-wide worker model
- universal work stealing or distributed scheduler semantics
