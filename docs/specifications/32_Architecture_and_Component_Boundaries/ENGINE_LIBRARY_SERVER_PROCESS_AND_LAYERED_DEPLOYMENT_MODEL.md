Status: reconstructed_required

# Engine Library Server Process and Layered Deployment Model

## Purpose

This document restores the original architecture rule that ScratchBird is library-first. Standalone processes are deployment compositions of those libraries, not the core semantic authority.

## Canonical Rule

The core engine is a library. It executes SBLR and internal procedures only. It is not a SQL parser and shall not embed SQL parsing or dialect handling.

## Primary Library Layers

The canonical library layers are:

- engine library
- parser libraries
- IPC library

Standalone executables are composed from one or more of those layers.

## Engine Library

The engine library owns:

- SBLR execution
- MGA transaction semantics
- storage and durability
- lock and visibility rules
- catalog and metadata truth

The engine library does not own:

- SQL text parsing
- dialect translation
- client response shaping for dialect-specific SQL surfaces

## Threaded IPC Server

The server is a threaded IPC server that uses the engine library. It is not a separate semantic engine. It is one deployment wrapper around the engine library for shared-process or shared-host use.

## Standalone Deployment Rule

The canonical standalone stack may be compiled into executables, but the process layout remains derived from the library layers. The executable boundary does not redefine ownership of SQL parsing, storage, or transaction semantics.

## Embedded Deployment Rule

Embedded deployments may use:

- the engine library directly with SBLR only
- a parser library plus the engine library
- a parser library plus the IPC library plus the threaded IPC server

All three are valid derived deployment forms of the same library-first architecture.

## Non-Guarantees

This file does not require every product deployment to expose every layer separately. It requires the canonical ownership model to remain library-first and parser-external to the engine.
