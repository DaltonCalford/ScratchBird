Status: reconstructed_required

# Embedded Direct Engine and Local Shared Server Runtime Selection Model

## Purpose

This document defines how an application chooses between direct embedded engine use and local shared-server use.

## Canonical Rule

Small local deployments may choose either:

- direct engine-library embedding
- local shared use through the IPC library and threaded IPC server

Both are first-class runtime selections of the same architecture.

## Direct Embedded Mode

In direct embedded mode:

- the application links the engine library directly
- the application may submit SBLR directly
- a parser library may optionally be linked for SQL handling
- no IPC server is required

## Local Shared-Server Mode

In local shared-server mode:

- one local threaded IPC server hosts the engine library
- one or more local applications or parser libraries connect through the IPC library
- no routable network listener is required

## Selection Rule

Runtime selection between these modes shall preserve:

- the same MGA semantics
- the same catalog and durability truth
- the same parser-independence rule

## Scale-Up Rule

An application may start in direct embedded mode and later scale to local shared-server, listener, or manager-fronted deployments without changing engine semantics.

## Non-Guarantees

This file does not require automatic migration between deployment modes. It defines the canonical supported selections and invariants.
