Status: reconstructed_required

# Local Only IPC Stack Session and Endpoint Identity Model

## Purpose

This document defines the canonical identity and session model for local-only deployments using the IPC library and threaded IPC server without IP networking.

## Canonical Rule

The local-only IPC stack is a first-class deployment mode. It shall preserve explicit endpoint and session identity even though it does not use routable network addresses.

## Endpoint Identity

The canonical local IPC endpoint identity shall preserve:

- local server identity
- database identity
- endpoint path or local address
- process or runtime generation
- transport family indicating non-IP local IPC

## Session Identity

Each local IPC session shall preserve:

- client-side session identity
- authenticated principal identity if applicable
- bound database identity
- transaction context identity
- reattach-handle capability if applicable

## Connection Rule

A local IPC session may connect:

- directly from an application using the IPC library
- from a parser library using the IPC library
- from a parser-agent process using the IPC library

The session model is the same regardless of which local caller shape initiated the connection.

## Non-Guarantees

This file does not require the native IP protocol to exist in the local-only deployment mode. It requires explicit identity and session semantics for the non-IP mode.
