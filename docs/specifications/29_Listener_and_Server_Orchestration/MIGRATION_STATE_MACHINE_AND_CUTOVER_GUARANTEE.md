# Migration State Machine and Cutover Guarantee

Status: reconstructed_required_with_current_substrate

## Purpose

Define the orchestration state machine for proxy, migration, verification, cutover, failback, and retirement, while preserving the rule that the listener is never the authority for committed engine truth.

## Current Code-Backed Boundary

Current repo proof exists for:
- migration catalog substrate
- manager and listener control seams
- remote connector and replication catalog families

Current repo proof does not yet establish a fully shipped listener-driven migration controller with end-to-end cutover execution.

## Ownership Model

- engine owns committed migration state
- controller publishes committed listener routing changes from committed engine state
- listener applies routing state but does not invent it
- manager may front, inspect, or request control, but it does not become data-truth authority

## Canonical States

- `DECLARED`
- `ASSESSING_SOURCE`
- `SNAPSHOT_COPY`
- `VERIFYING`
- `DUAL_READ_AUDIT`
- `CUTOVER_READY`
- `CUTOVER_COMMITTING`
- `PRIMARY_EMULATED`
- `MIRROR_LEGACY`
- `RECONCILING_FAILBACK`
- `RETIRED`
- `REFUSED`
- `QUARANTINED`

## Allowed Transitions

- `DECLARED -> ASSESSING_SOURCE`
- `ASSESSING_SOURCE -> SNAPSHOT_COPY`
- `SNAPSHOT_COPY -> VERIFYING`
- `VERIFYING -> DUAL_READ_AUDIT`
- `VERIFYING -> CUTOVER_READY`
- `DUAL_READ_AUDIT -> CUTOVER_READY`
- `CUTOVER_READY -> CUTOVER_COMMITTING`
- `CUTOVER_COMMITTING -> PRIMARY_EMULATED`
- `PRIMARY_EMULATED -> MIRROR_LEGACY`
- `PRIMARY_EMULATED -> RECONCILING_FAILBACK`
- `RECONCILING_FAILBACK -> PRIMARY_EMULATED`
- `MIRROR_LEGACY -> RETIRED`
- any pre-cutover state -> `REFUSED`
- any active state -> `QUARANTINED` on hard policy or safety failure

## Weak-Donor Restrictions

For donor capability classes `statement_consistent_only` and `weak_or_non_transactional`:
- `DUAL_WRITE` is not an implied or default state
- `CUTOVER_READY` requires explicit unresolved-drift assessment
- `CUTOVER_COMMITTING` is refused unless policy permits the donor’s weaker guarantee class
- listener routing must not advertise zero-drift or transactionally mirrored cutover where proof does not exist

## Cutover Guarantee

ScratchBird may claim cutover completion only when all of the following are true:
1. cutover readiness assessment is committed
2. routing generation change is committed by the engine
3. listener or manager consumers observe the committed generation
4. audit and continuity markers are emitted
5. donor capability class and remaining residual risk are recorded

The guarantee is therefore a committed routing and publication guarantee inside ScratchBird, not a claim that the donor had MGA semantics.

## Failback Rule

Failback is restore-style reconciliation, not blind route reuse.

Required order:
1. classify reason for failback
2. freeze new promotion claims
3. record reconciliation boundary
4. verify target and donor divergence
5. commit failback routing generation
6. publish continuity markers and operator-visible residual-risk state

## Listener and Manager Surface Rules

- listener status may expose migration generation and mode as committed engine-owned values
- listener-local status must not fabricate drift counts, audit counts, or cutover readiness if the engine did not publish them
- manager may expose fleet-wide inspection and remote control results, but mutation still requires engine authorization and committed state publication

## Refusal Rules

Mandatory refusal applies when:
- source capability assessment is missing or stale
- cutover readiness is absent
- unresolved drift exceeds policy
- routing generation expectation fails
- required audit or continuity marker persistence fails

## Reconstructed Required Expansion

The rebuilt canon additionally requires:
- explicit controller-run transcript gates
- generation-based listener routing update proof
- manager-heartbeat integration for migration inspection
- deterministic refuse, quarantine, and failback transcripts
