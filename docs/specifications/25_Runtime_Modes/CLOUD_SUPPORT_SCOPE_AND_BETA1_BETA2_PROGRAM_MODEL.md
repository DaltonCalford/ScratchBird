# Cloud Support Scope and Beta 1 / Beta 2 Program Model

Status: reconstructed_required
Section: 25_Runtime_Modes

## Purpose

Define what cloud support means for ScratchBird and separate:

- Beta 1 single-node cloud support
- Beta 2 clustered and cloud-native operation

Cloud support means operational fit with preserved ScratchBird correctness guarantees. It does not merely mean that a binary can run in a virtual machine or a container.

## Cloud support levels

1. Cloud-runnable
- starts, stops, and recovers non-interactively
- supports externally supplied configuration
- supports mounted persistent storage
- supports externally supplied TLS, certificates, and secrets
- does not require interactive startup repair steps

2. Cloud-operable
- exposes liveness, readiness, startup, and degraded-state health signals
- emits structured logs, metrics, and trace-compatible events
- supports support-bundle generation
- specifies behavior under container CPU and memory limits
- supports certificate and secret rotation without operator guesswork
- supports automated backup, restore, and incident response procedures

3. Cloud-deployable as a service
- treats front-door networking layers as separately deployable service units
- supports stateless scaling of listeners, parser pools, and manager front-door layers
- supports stateful packaging of engine and server layers with persistent storage
- supports load balancers, private service routing, and deployment automation
- supports signed release and controlled upgrade channels

4. Cloud-native clustered operation
- adds cluster membership, failure detection, routing, admission, placement, and recovery behavior
- requires Beta 2 certification before public promise expansion
- must not be implied by Beta 1 single-node support

## Supported deployment topologies

1. Embedded engine only
- engine library linked directly into an application
- SBLR-only operation is allowed
- no listener or IP surface is required

2. Embedded parser plus engine library
- parser library lowers dialect-local SQL to SBLR
- parser may call the engine library directly
- no inter-parser dependency is allowed

3. Local IPC single-host service
- parser or client uses local IPC library
- threaded IPC server hosts the engine library
- no IP listener is required

4. Single-node server deployment
- listener accepts protocol connections
- parser-agent pool lowers SQL to SBLR
- threaded IPC server hosts engine execution
- manager may optionally front ScratchBird native connections

5. Proxy migration deployment
- manager fronts the listener on a routable address
- listener may remain on loopback or other non-routable interface
- manager authenticates, gates database selection, validates handoff material, and proxies native ScratchBird traffic

6. Clustered deployment target
- reserved for Beta 2
- uses manager-owned heartbeat and remote-management control-plane surfaces plus cluster runtime surfaces specified in section 25

## Beta 1 promise set

Beta 1 cloud support means ScratchBird can be used as a serious single-node service in cloud environments.

Beta 1 requires:

- declared Linux and Windows runtime package profiles
- persistent volume and mount policy support
- externally supplied config, secrets, and certificates
- non-interactive startup, shutdown, restart, and recovery behavior
- health probes and structured observability
- support bundles for incident capture
- backup, restore, export, and import automation suitable for single-node cloud targets
- explicit cgroup and container resource-limit behavior
- low or bounded-downtime rolling refresh of manager, listener, and parser layers
- documented deployment topologies for embedded, local IPC, single-node service, and proxy migration use

Beta 1 package-profile rule:

- Linux and Windows runtime packages are the only required first-class package profiles
- VM images, container images, and IaC descriptors may exist as auxiliary deployment assets, but they are not implied first-class Beta 1 package obligations unless the section 41 support matrix explicitly promotes them

Beta 1 does not promise:

- metadata consensus
- cluster-wide commit authority
- full cluster read repair or quorum promotion
- automatic global multi-node failover
- universal orchestrator integration without a support matrix
- cluster-wide deployment queueing or multi-target remote-management history

## Beta 2 promise set

Beta 2 extends Beta 1 with cluster and cloud-native runtime guarantees.

Beta 2 requires implementation-grade behavior for:

- manager heartbeat and node identity publication
- membership and failure detection
- control-plane instruction queueing, assessment, application, and audit
- routing and admission decisions across nodes
- topology-aware placement and relocation rules for stateful surfaces
- mixed-version behavior bounded by the compatibility manifest
- explicit failover, promotion, recovery, and rollback rules
- cluster-aware secret and certificate rollout semantics
- hard multi-tenant isolation or QoS only where the section 33 and section 38
  Beta 2 rules are implemented and certified

Beta 2 cluster behavior must not be marketed or documented as supported until the section 25 cluster promise and section 31 certification gates are met.

## Non-goals

Cloud support does not automatically mean:

- serverless operation
- Kubernetes-only operation
- immediate globally distributed SQL
- automatic multi-tenant SaaS isolation before the Beta 2 isolation and budget
  contracts are implemented and certified
- universal cross-cloud portability without a published support matrix

## Required cross-section links

- section 19 owns cloud secret and signed-artifact lifecycle
- section 20 owns probes, telemetry, tracing, and support-bundle rules
- section 29 owns front-door topology and rolling-restart behavior
- section 31 owns Beta 1 and Beta 2 certification gates
- section 33 owns cgroup and container resource behavior
- section 39 owns cloud backup, snapshot, and restore automation
- section 41 owns packaging, support matrix, and rollout automation
