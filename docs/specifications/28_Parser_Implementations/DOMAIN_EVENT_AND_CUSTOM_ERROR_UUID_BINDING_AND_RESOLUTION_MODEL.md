Status: reconstructed_required

# Domain Event and Custom Error UUID Binding and Resolution Model

## Purpose

This document defines how parser libraries bind references to domains, events, and custom error messages through UUID-backed shared-definition identity.

## Canonical Rule

Parser libraries may accept user-visible names for domains, events, and custom error messages, but canonical binding shall resolve those names to UUID-backed shared-definition objects before execution artifacts are finalized.

## Covered Shared Definitions

The canonical shared-definition classes are:

- domains
- events
- custom error messages

## Resolution Rule

Parser-local resolution shall:

1. resolve the user-facing name in the current naming scope
2. verify the target object class
3. bind the canonical UUID identity
4. preserve that UUID identity in the emitted execution or DDL artifact

## Cross-Node Rule

Because these object classes are cluster-shared definitions, parser output shall never treat a successful name lookup as sufficient by itself for cross-node portability. The emitted artifact must preserve UUID identity so other nodes can verify the same shared definition.

## DDL Rule

When DDL creates or alters objects depending on shared definitions:

- emitted artifacts shall preserve the shared-definition UUID dependency
- later movement or activation of the dependent object shall validate that same dependency UUID on the target node

## Non-Guarantees

This file does not require every parser to expose shared-definition UUIDs directly to the user. It requires parser-local name resolution to end in UUID-bound canonical identity.
