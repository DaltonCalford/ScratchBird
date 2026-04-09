# Process Model and Execution Surfaces

This file owns the top-level execution-surface matrix and request-lifecycle rules for ScratchBird architecture.

## Execution surface matrix

| Surface | Current state | Primary owning sections | Boundary class | Current truth | Fail-closed exclusions |
| --- | --- | --- | --- | --- | --- |
| embedded engine runtime | current | 25, 32 | hard_boundary | engine-adjacent execution exists as a real local runtime surface | not a license to infer listener/session/protocol semantics |
| local IPC runtime | current_bounded | 25, 29, 32 | shared_boundary | local IPC exists as a bounded runtime surface where section 25/29 already prove it | not a claim of remote/distributed execution |
| server-managed listener path | current_bounded | 29, 32 | shared_boundary | listener-managed process/session surfaces exist where section 29 documents them | not a general cluster-fabric execution claim |
| parser/front-door path | current_bounded | 21, 28, 32 | hard_boundary | front-door parsing exists outside core engine semantics | parser presence does not imply parser-inside-engine architecture |
| client/tooling path | current_bounded | 30, 32 | hard_boundary | client and tooling surfaces exist as bounded external control/API surfaces | client behavior does not widen engine-internal ownership |
| protocol/handshake session entry | current_bounded | 26, 27, 29, 32 | shared_boundary | protocol and handshake entry surfaces are real bounded contracts | not a claim of universal transport or multi-node fabric |

## Process model matrix

| Process or runtime role | Current state | Entry condition | Exit condition | Explicit exclusion |
| --- | --- | --- | --- | --- |
| embedded local runtime | current | local caller selects embedded or direct runtime path | local session teardown or process end | not a proof of listener or wire entry |
| listener-managed session runtime | current_bounded | external or bounded local transport enters through listener or server orchestration | session teardown, listener refusal, or runtime shutdown | not a cluster scheduler |
| parser/front-door compiler path | current_bounded | SQL or dialect input is accepted by front-door layer | lowered internal form handed off to shared execution layer | not engine-core semantics |
| direct internal execution path | current_bounded | internal SBLR or internal procedure invocation is ready for execution | execution result, refusal, rollback, or teardown | not a public client contract |

## Session lifecycle state machine

1. `unbound`: no validated runtime or session context exists.
2. `entry_bound`: a concrete entry surface is selected: embedded, local IPC, listener-managed, or bounded protocol entry.
3. `capability_checked`: the selected entry path validates mode and feature compatibility for that runtime.
4. `authenticated_or_local_trusted`: trust is established either by local bounded runtime rules or by the owning handshake/auth surface.
5. `execution_ready`: session context, transaction context, and request envelopes are ready for work submission.
6. `executing`: shared internal execution proceeds under the lower-level owning sections.
7. `result_emitting`: results, status, or refusal surfaces are returned through the owning external or local path.
8. `teardown`: session-local state is released and external or local handles are closed.
9. `closed`: no additional work may be submitted on that session context.

## Request-path algorithms

### Embedded local execution path

1. A local caller binds to the embedded runtime surface.
2. The caller either provides internal execution input directly or uses a front-door compiler that lowers outside core engine semantics.
3. The runtime creates or reuses bounded local session and transaction state.
4. Shared internal execution is invoked through the lower-level execution and transaction sections.
5. Results or refusal are returned directly to the local caller.
6. Session-local resources are released when the unit of work or caller lifetime ends.

### Listener-managed execution path

1. A client or bounded local transport enters through the listener-managed surface.
2. The owning handshake or protocol section validates transport and capability rules.
3. Listener/server orchestration creates the bounded session context.
4. The request is handed to the execution runtime either in-process or through bounded local IPC.
5. Shared internal execution occurs under lower subsystem ownership.
6. Results, errors, or refusal are emitted back through the listener-managed path.
7. Session teardown releases listener, transport, and runtime handles in the reverse order of acquisition.

### Parser-assisted execution path

1. A front-door parser accepts user-layer syntax.
2. Parser-local semantics lower input into shared internal execution forms.
3. Section 32 stops owning the request at the handoff boundary to shared execution.
4. Any parser feature not explicitly lowered remains outside engine-core truth.

## Canonical rules

1. Engine execution truth and front-door parsing truth are separate architectural layers.
2. Listener/session/process orchestration is a bounded outer surface, not a redefinition of engine internals.
3. Client/tooling contracts sit outside engine ownership unless a lower-level section explicitly delegates inward.
4. Every request must bind to exactly one entry surface before execution begins.
5. Any surface not explicitly listed above remains fail-closed at the architecture layer.

## Explicit non-guarantees

- no distributed multi-node execution fabric is implied here
- no remote execution model is implied beyond the bounded listener/protocol surfaces already owned elsewhere
- no architecture claim here overrides lower-level ownership in sections 21, 25, 26, 27, 29, or 30
- no stable guarantee that all entry surfaces share identical lifecycle hooks or transport behavior
