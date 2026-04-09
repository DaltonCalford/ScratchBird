Status: reconstructed_required

# Local IPC Request Framing Session Lifecycle and No IP Guarantee Model

## Purpose

This document defines the canonical request framing and session lifecycle for the local non-IP IPC stack.

## Canonical Rule

The local IPC stack is guaranteed to function without IP networking. Request framing, session lifecycle, and endpoint identity are defined independently of any IP listener or routable address.

## Request Framing

The canonical local IPC request frame shall preserve:

- request identity
- session identity
- database identity
- canonical operation class
- transaction or reattach handle context when applicable

## Session Lifecycle

The canonical session lifecycle is:

1. local endpoint resolve
2. session open
3. authenticated or anonymous capability negotiation as admitted by policy
4. request or response exchange
5. session close or dormant-handle preservation where applicable

## No-IP Rule

The existence of the local IPC mode does not depend on:

- an IP listener
- a parser-agent pool
- a manager proxy

Those layers may be absent and the local IPC mode remains conforming.

## Non-Guarantees

This file does not define one physical socket or shared-memory primitive. It defines the canonical non-IP session model.
