Status: reconstructed_required

# Manager Proxy Front Door and Inner Listener Routing Model

## Purpose

This document defines the canonical manager-fronted ScratchBird routing model.

## Canonical Rule

The optional manager may own the outer-facing ScratchBird network address and proxy ScratchBird-native traffic to an internal listener bound to a non-routable local address. This is a proxy and control-plane layer, not a semantic replacement for the listener or engine.

## Routing Model

The canonical manager-fronted route is:

1. client connects to manager
2. manager authenticates or gates ScratchBird-native admission
3. manager selects the target database and policy
4. manager proxies to an internal listener on a non-routable address
5. listener hands off to a parser-agent pool
6. parser agent uses IPC to the threaded IPC server
7. threaded IPC server uses the engine library

## Scope Rule

This manager-fronted route applies to ScratchBird-native traffic. It does not automatically subsume every emulation listener path.

## Security Boundary

The manager sits behind the outer-facing IP boundary so the inner listener may remain on loopback or another non-routable local address.

## Heartbeat and Control Rule

The manager also serves as the server-local heartbeat and control-plane participant for the cluster layer where that layer is enabled. That role does not erase the listener’s bounded responsibility for parser-agent pooling and local transport handoff.

## Non-Guarantees

This file does not require the manager in all deployments. It defines the optional scaled-up routing form.
