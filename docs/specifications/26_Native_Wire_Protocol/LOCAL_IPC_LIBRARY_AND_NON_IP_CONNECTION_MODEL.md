Status: reconstructed_required

# Local IPC Library and Non-IP Connection Model

## Purpose

This document defines the canonical non-IP local connection model used when applications share a local database through the IPC library and threaded IPC server.

## Canonical Rule

ScratchBird does not require IP networking for local shared-database use. The IPC library is the canonical non-IP connection surface for local clients or parser libraries that do not embed the engine directly.

## Local IPC Responsibilities

The IPC library owns:

- local process-to-process connection establishment
- canonical request framing to the threaded IPC server
- local endpoint addressing
- response transport back to the caller

## Server Relationship

The IPC library talks to the threaded IPC server, and that server uses the engine library. The IPC library is not the engine and does not introduce an alternate semantic layer.

## Parser Relationship

A parser library may use the IPC library instead of direct engine embedding when:

- the database is shared locally
- IP networking is unnecessary
- the deployment wants process separation without a network listener

## Non-Guarantees

This file does not require the native IP protocol for local-only shared-database deployment. It defines the non-IP local shared-database option.
