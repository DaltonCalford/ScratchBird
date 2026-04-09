# Cloud Front-Door Service Topology and Rolling Restart Model

Status: reconstructed_required
Section: 29_Listener_and_Server_Orchestration

## Purpose

Define the deployable cloud service topology for the manager, listener, parser, IPC server, and engine layers, and define which layers can be rolled independently.

## Layer model

1. Manager front door
- optional for native ScratchBird connections
- may bind the routable address
- authenticates and gates database selection
- proxies only validated native ScratchBird traffic to internal listeners
- owns heartbeat and remote-management agent surfaces

2. Listener layer
- protocol-specific network acceptor
- may bind loopback, private address, or internal service address
- may remain entirely non-routable when fronted by a manager or cloud load balancer
- owns connection acceptance, protocol negotiation preconditions, and parser-pool handoff

3. Parser-agent layer
- stateless from the perspective of durable engine state
- owns dialect-local SQL parsing, semantic shaping, and SBLR lowering
- may scale horizontally within a host or service group
- must not become correctness authority for transaction state or committed metadata truth

4. Local IPC server layer
- threaded IPC server hosting engine access
- local correctness and execution bridge between parser-facing and engine-facing layers

5. Engine layer
- correctness anchor
- owns MGA visibility, record truth, DDL publication, recovery, and durable state
- not directly exposed on a public network address

## Cloud service topology rules

- Direct public exposure of engine IPC is prohibited.
- Manager and listener public exposure rules are deployment-profile specific and must be explicit.
- Emulated protocols may be exposed through their own listeners.
- ScratchBird native traffic may be fronted by the optional manager for firewall and policy control.
- Parser pools and listeners may be scaled independently from the engine process as long as the handoff contract remains unchanged.

## Node identity and service routing

- manager identity, listener identity, parser-pool identity, and engine identity must remain distinguishable in logs and metrics
- a cloud deployment must be able to route traffic to:
  - manager front door
  - protocol-specific listeners
  - health and admin surfaces as allowed by policy
- service discovery must not collapse engine identity and listener identity into one opaque node label

## Rolling restart model

### Safe independent roll lanes

1. Manager roll
- allowed when existing proxy sessions are drained or transferred according to policy
- must not alter engine durable truth
- must preserve or explicitly invalidate in-flight proxy-only state

2. Listener roll
- allowed after connection drain or bounded refusal period
- existing accepted sessions may continue through existing workers if the deployment pattern supports it
- must not fabricate engine health or derivative status that it does not own

3. Parser-pool roll
- allowed independently of listener socket ownership when the handoff contract remains compatible
- must preserve dialect-local lowering rules for the active deployment generation

### Stateful roll lane

4. Engine or local IPC server roll
- requires stateful restart procedure
- must respect section 08 dormant detach and restart reattach rules
- must respect section 35 recovery and publication ordering rules
- must not be described as zero-downtime unless the exact reattach and routing evidence exists for the deployment profile

## Low-downtime promise boundaries

Beta 1 allows low or bounded-downtime rolling refresh for manager, listener, and parser layers when:

- the service topology isolates those layers from engine durable truth
- health probes reflect transitional unready states correctly
- secret and certificate rotation is coordinated with the active listener or manager generation

Beta 1 does not promise universal no-downtime engine replacement.

Beta 2 may add stronger promises only after cluster routing, membership, and failover rules are certified.
