Status: reconstructed_required

# Engine Admin Listener Control and Configuration Propagation Model

## Purpose

This document defines how database-owned listener configuration is propagated through the engine-admin path into the bounded listener runtime.

## Canonical Rule

Listener runtime control is initiated by the database through engine-admin and controller paths. The listener remains untrusted for semantic truth and applies only admitted bounded runtime changes over the tight local IPC control channel.

## Canonical Control Chain

The control chain is:

1. database-owned configuration or management record changes
2. engine-admin evaluation and authorization
3. controller dispatch
4. bounded listener-management IPC message
5. listener runtime apply or refusal
6. status publication back through the engine

## Admitted Control Operations

The admitted operations include:

- start listener profile
- stop listener profile
- force-stop listener profile
- reload listener profile
- enable or disable emulation binding
- change parser-pool policy
- publish listener status

## Propagation Rule

Every control action shall preserve:

- target listener identity
- source generation
- requested generation
- operation class
- authorization identity
- resulting apply or refusal state

## Listener Boundary Rule

The listener may report:

- bound endpoints
- parser-pool state
- runtime health
- emulation binding state

The listener may not invent or overwrite catalog truth. Its report is status evidence, not management authority.

## Refusal Rule

The listener shall refuse control operations when:

- generation is stale
- required runtime prerequisites are missing
- the requested operation exceeds the bounded control contract
- the manager-fronted routing requirement is violated

## Non-Guarantees

This file does not require the listener to trust the engine. The seam remains explicitly bounded and local, with the engine treating listener input as untrusted runtime evidence.
