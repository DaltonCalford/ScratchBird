Status: reconstructed_required

# Parser Agent Executable Composition and Runtime Stack Model

## Purpose

This document defines the canonical composition of the parser-agent executable or process stack used behind listeners.

## Canonical Rule

A parser agent is a runtime composition of:

- one parser library
- the IPC library
- session or protocol glue specific to that parser-agent process

It is not a new semantic engine and it is not a listener replacement.

## Parser-Agent Responsibilities

The parser agent owns:

- protocol-session-local request handling after listener handoff
- parser-library invocation
- dialect-local SQL to SBLR lowering
- parser-owned response shaping
- IPC request emission to the threaded IPC server

## Listener Relationship

The listener owns:

- inbound network acceptance
- parser-agent pool management
- assignment of a connection to a parser agent

The parser agent begins after handoff.

## IPC Relationship

The parser agent uses the IPC library as its engine-facing transport unless the deployment explicitly embeds the engine instead. The normal networked stack uses IPC to the threaded IPC server.

## Prohibitions

The parser agent shall not:

- depend on another parser library
- embed SQL parsing into the engine
- redefine transaction or durability semantics

## Non-Guarantees

This file does not require every parser agent to be a separate OS process. It defines the runtime composition and ownership model.
