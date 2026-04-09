Status: reconstructed_required

# Manager Heartbeat Database Proxy and Cluster Agent Boundary Model

## Purpose

This document defines the canonical boundary of the optional manager when it acts as front-door proxy, server-local heartbeat agent, and cluster-side control participant.

## Canonical Rule

The optional manager may combine three roles:

- ScratchBird-native front-door proxy
- server-local heartbeat and health publisher
- cluster control-plane agent

Combining those roles does not make the manager the engine, parser, or listener authority for semantic truth.

## Proxy Boundary

As front-door proxy, the manager owns:

- outer-facing ScratchBird-native address
- admission and initial routing
- handoff to the internal listener on non-routable local address

## Heartbeat Boundary

As server-local heartbeat agent, the manager owns:

- heartbeat publication for the local server
- liveness or health projection into the cluster layer
- remote-management queue visibility where admitted

## Cluster-Agent Boundary

As cluster agent, the manager may:

- receive remote-management instructions
- publish local capability and drift state
- coordinate with the controller and listener stack

It does not become the storage, MGA, or catalog authority.

## Non-Guarantees

This file does not require the manager in all deployments. It defines the explicit boundary where the manager is present.
