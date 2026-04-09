# Cloud Readiness and Beta 2 Cluster Certification Model

Status: reconstructed_required
Section: 31_Conformance_Performance_and_Reliability_Gates

## Purpose

Define the evidence required to certify ScratchBird cloud support for Beta 1 and to certify clustered cloud-native operation for Beta 2.

## Beta 1 cloud-readiness certification

Beta 1 certification requires named evidence for:

- Linux and Windows runtime package profiles
- persistent storage attachment and restart behavior
- liveness, readiness, startup, and degraded-state probes
- structured logs, metrics, and trace-compatible correlation
- support-bundle generation
- secret and certificate rotation procedures
- signed artifact validation in deployment channels
- cgroup and container resource-limit behavior
- backup, restore, export, and import automation for declared cloud targets
- rolling refresh rules for manager, listener, and parser layers

Auxiliary VM images, container images, and IaC assets may contribute evidence,
but they do not become required first-class Beta 1 certification profiles unless
the section 41 support matrix explicitly promotes them.

## Beta 2 cluster certification

Beta 2 certification is not granted by Beta 1 success alone. Beta 2 requires named evidence for:

- manager heartbeat and node identity publication
- membership and failure detection
- control-plane instruction lifecycle correctness
- cluster routing and admission behavior
- mixed-version behavior under the compatibility manifest
- failover, promotion, recovery, and rollback rules
- cluster secret and certificate rollout behavior
- cluster observability and support-bundle completeness

## Required evidence classes

Each certified cloud support claim must map to:

- requirement identifier
- named test or exercised procedure
- artifact or log location
- deployment profile
- disposition result

## Downtime language

Certification must distinguish between:

- zero-downtime proven
- bounded-downtime proven
- restart-required but automated
- unsupported or refusal-boundary

No cloud marketing claim may overstate the certified class.
