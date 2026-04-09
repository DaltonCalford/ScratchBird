Status: reconstructed_required

# Embedded Direct Engine Parser IPC and Stack Deployment Model

## Purpose

This document defines the canonical deployment variants built from the ScratchBird library layers.

## Canonical Deployment Variants

The canonical variants are:

1. direct embedded engine
2. embedded parser plus engine
3. parser plus IPC library to local threaded IPC server
4. full network stack with listener and parser-agent pool
5. manager-fronted full network stack

## Direct Embedded Engine

In the direct embedded engine variant:

- the application links the engine library
- the application submits SBLR or internal procedure calls directly
- no SQL parser is required
- no IP support is required

This is the minimal embedded deployment form.

## Embedded Parser Plus Engine

In the embedded parser-plus-engine variant:

- the application links one parser library
- that parser library performs dialect-local SQL to SBLR lowering
- the parser library may also shape the response for its dialect contract
- the engine library remains the execution authority

## Parser Plus IPC Library

In the parser-plus-IPC variant:

- the application links a parser library
- the parser library lowers SQL to SBLR
- the parser or host application uses the IPC library
- the IPC library talks to a local threaded IPC server without requiring IP networking

## Full Network Stack

In the full network stack:

- listeners accept inbound network connections
- listeners hand off to parser-agent processes or stacks
- parser agents use parser libraries
- parser libraries lower SQL to SBLR and use IPC
- the threaded IPC server uses the engine library

## Manager-Fronted Stack

In the manager-fronted stack:

- the manager owns the outer-facing ScratchBird address
- manager traffic is proxied to an internal listener on a non-routable address
- emulation listeners remain separate from the manager-owned ScratchBird front door

## Scalability Rule

The same core design must support small standalone embedded use and larger layered server deployments without moving SQL parsing into the engine.

## Non-Guarantees

This file does not require every deployment to be process-isolated. The canonical requirement is the ownership boundary, not a mandatory process count.
