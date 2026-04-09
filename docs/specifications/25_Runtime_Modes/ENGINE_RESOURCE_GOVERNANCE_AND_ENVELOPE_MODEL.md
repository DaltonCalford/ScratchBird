# Engine Resource Governance and Envelope Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define one engine-wide resource envelope spanning memory, CPU, IO, temp/spill, accelerator, session, statement, and maintenance budgets so overload behavior is explicit and deterministic.

## Governing rules

1. Resource governance is engine-owned even when local runtime consumers enforce different parts of it.
2. Memory, spill, accelerator, and work-queue decisions must compose into one envelope rather than silently conflict.
3. Overload handling must degrade or refuse deterministically.
4. Cluster-wide autonomous fairness is not assumed unless explicitly proven; the current contract is engine-wide and node-local first.

## Envelope layers

| Layer | Scope |
| --- | --- |
| `GLOBAL_NODE_ENVELOPE` | total engine memory, worker, IO, spill, and accelerator budgets on one node |
| `SERVICE_CLASS_ENVELOPE` | per workload/service class limits |
| `SESSION_ENVELOPE` | per connection or session ceilings |
| `STATEMENT_ENVELOPE` | per statement/query ceilings |
| `TASK_ENVELOPE` | per operator/task runtime budget |
| `DERIVATIVE_WORK_ENVELOPE` | background export, shadow, audit, or maintenance workloads |

## Resource classes

- buffer/cache memory
- execution arena memory
- statement/translation/JIT caches
- temp and spill storage
- worker slots and scheduler quanta
- background maintenance bandwidth
- accelerator device memory and queue slots
- derivative export bandwidth

## State machine

| State | Meaning |
| --- | --- |
| `NORMAL` | all admitted work fits declared envelope |
| `GUARDED` | soft thresholds crossed; reduce speculative work |
| `THROTTLED` | lower-priority work slowed or paused |
| `SHED` | non-essential work refused or quarantined |
| `FAIL_CLOSED` | safety or correctness budget exceeded; new work refused |

## Required operator controls

The engine shall expose bounded operator control over:
- global memory ceiling
- spill quota ceiling
- per-session and per-statement budget classes
- worker pool ceiling
- maintenance concurrency ceiling
- accelerator enable/disable and reservation ceiling
- derivative lane reservation and throttling policy

## Fairness rule

1. Foreground correctness-critical work outranks derivative and optimization-only work.
2. Maintenance may borrow idle capacity but must yield under foreground pressure.
3. Accelerator work may not starve CPU fallback paths or correctness-required background work.
4. Temp/spill growth may be throttled before buffer-cache correctness surfaces are compromised.

## Overload rule

When envelopes conflict, the engine shall prefer the following order:
1. preserve MGA correctness and commit publication
2. preserve visibility-critical reads and writes
3. preserve session and statement budget integrity
4. shed derivative, speculative, or advisory work
5. refuse new work rather than silently violating ceilings

## Current substrate alignment

Current code-backed authority in `ENGINE_RESOURCE_GOVERNANCE_AND_BUDGETS.md` remains valid as the bounded runtime substrate for:
- governance metadata
- local routing/admission decisions
- budgets and overload outcomes

This file raises those bounded concerns into one unified envelope model.
