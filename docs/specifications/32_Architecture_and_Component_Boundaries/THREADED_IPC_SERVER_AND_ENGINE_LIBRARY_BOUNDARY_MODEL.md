Status: reconstructed_required

# Threaded IPC Server and Engine Library Boundary Model

## Purpose

This document defines the canonical boundary between the threaded IPC server and the core engine library.

## Canonical Rule

The threaded IPC server is a host and coordination wrapper around the engine library. It is not a separate semantic engine. All storage, transaction, catalog, durability, and SBLR execution truth remains inside the engine library.

## Engine Library Responsibilities

The engine library owns:

- SBLR execution
- MGA transaction and visibility semantics
- storage and durability
- catalog and metadata truth
- lock and conflict semantics

## Threaded IPC Server Responsibilities

The threaded IPC server owns:

- request acceptance from the IPC library
- per-session threading or worker coordination
- canonical request framing into engine calls
- canonical response framing back to the IPC library

## Prohibitions

The threaded IPC server shall not own:

- SQL parsing
- dialect translation
- SQL syntax diagnostics
- parser-specific response shaping rules

## Concurrency Rule

Threading in the IPC server is a transport and orchestration concern. It does not create a second transaction model or a second catalog authority.

## Non-Guarantees

This file does not require one exact threading implementation. It requires that the IPC server remain a wrapper around the engine library rather than becoming a semantic fork of it.
