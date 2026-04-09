Status: reconstructed_required

# Standalone Local Only and Layered Scale Up Runtime Model

## Purpose

This document defines the canonical progression from minimal standalone local deployment to layered scaled deployment.

## Canonical Rule

ScratchBird shall support a scale-up path that preserves the same core semantic engine while adding optional layers. The small local deployment and the larger layered deployment are variants of one architecture, not separate products.

## Canonical Runtime Variants

The canonical variants are:

1. direct embedded SBLR-only engine
2. embedded parser-plus-engine
3. local shared database through IPC server without IP
4. listener plus parser-agent pool stack
5. manager-fronted routed stack

## Local Only Variant

The local-only standalone variant may:

- omit network listeners
- omit manager and cluster layers
- use direct engine embedding or parser-plus-engine embedding
- use a local IPC server for shared local access without IP

## Layered Scale-Up Variant

The scaled deployment may add:

- listener surfaces
- parser-agent pools
- manager proxy layer
- remote management and cluster control
- stronger security, authentication, and heartbeat layers

## Preservation Rule

Adding layers shall not move SQL parsing into the engine and shall not change MGA, catalog, or transaction truth ownership.

## Operator Rule

Operators shall be able to select a deployment variant intentionally rather than being forced into one process topology for all use cases.

## Non-Guarantees

This file does not require every runtime variant to ship as one binary. It defines the supported architectural progression and invariants.
