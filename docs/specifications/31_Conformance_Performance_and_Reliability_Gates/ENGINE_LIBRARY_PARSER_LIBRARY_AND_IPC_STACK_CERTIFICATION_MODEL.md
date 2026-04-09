Status: reconstructed_required

# Engine Library Parser Library and IPC Stack Certification Model

## Purpose

This document defines the certification evidence required for the library-first architecture and its composed runtime stacks.

## Required Certification Classes

Certification shall cover:

- direct embedded engine with no parser
- embedded parser plus engine
- parser library plus local IPC server
- listener plus parser-agent stack
- manager-fronted routed stack

## Required Evidence

Each certification case shall preserve:

- deployment variant identity
- active layer set
- parser-library identity if present
- IPC server presence or absence
- listener presence or absence
- manager presence or absence
- outcome classification

## Failure Criteria

Certification fails when:

- the engine layer requires SQL parsing to be present
- one parser depends on another parser
- local shared-server mode depends on an IP listener
- a layered stack cannot explain which library layers compose it

## Non-Guarantees

This file does not require every current build artifact to expose every variant. It defines the certification target for the recovered architecture.
