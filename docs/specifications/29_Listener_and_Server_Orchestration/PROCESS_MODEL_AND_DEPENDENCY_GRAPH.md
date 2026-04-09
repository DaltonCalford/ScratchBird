# Process Model and Dependency Graph

## Process roles

### `ServiceController`

`ServiceController` is the current policy and orchestration owner for front-door
process topology.

It currently owns:
- reading bootstrap listener and manager configuration
- constructing protocol listener profiles
- resolving owner-database engine endpoints
- spawning and stopping listener processes
- spawning and stopping the optional manager proxy
- opening the local listener-management channel
- issuing listener runtime control requests such as `RELOAD`, `STOP`, and
  `POOL SET`

The listener does not self-authorize:
- which emulation profiles are enabled
- which ports are bound
- which owner database a listener belongs to
- the permitted parser-pool range

Those are engine or controller-owned decisions, even when bootstrap input comes
from a file.

### Optional manager proxy (`sb_manager`)

The optional manager is a ScratchBird-native front-door proxy that sits in
front of the internal native listener.

Current code authority for the manager is:
- front-door bind for native manager traffic
- manager authentication and `DB_LIST` / `DB_CONNECT` flow
- owner-database gating inside manager-proxy scope
- readiness check against the internal native listener
- issuance of a `DBBT`
- construction of `LPREFACE`
- validation of that `LPREFACE` against the listener management socket before
  proxying
- bidirectional proxying of the client socket to the internal native listener

The manager is the correct owner for any future server-local heartbeat,
cluster-test, or control-plane communication lane. That role must not be
shifted into the listener runtime.

### Listener process (`sb_listener_main`)

The listener owns:
- front-door bind for one protocol family
- local control socket
- local management socket
- parser worker pool lifecycle
- admission or rejection at the listener edge
- validation of manager-proxy binding context when required
- controlled handoff of accepted sockets to parser workers
- listener-local metrics and managed-mode audit events

The listener is intentionally treated as untrusted by the engine for durable
policy truth. It executes controller-approved runtime requests, but it does not
become the authority for topology, transaction semantics, catalog truth, or MGA
visibility.

### Parser worker / parser agent

Parser workers own:
- protocol-specific handshake
- protocol framing after socket handoff
- SQL text intake for their dialect family
- SQL text to engine IPC translation
- forwarding engine IPC responses back to the client protocol

Parser workers do not own:
- listener topology
- database binding policy
- transaction truth
- catalog publication truth

### Engine IPC session handler

The engine IPC session handler owns:
- engine session attach
- connection-context creation or dormant reattach
- dialect-tag and emulation-mode seeding
- catalog-backed user lookup and session creation
- current schema and search-path initialization
- execution of engine payloads

It is explicitly not the SQL parser.

## Current startup and ownership topology

### Direct listener path

1. `ServiceController` resolves enabled listener profiles from bootstrap
   configuration.
2. For each enabled profile it resolves:
   - protocol family
   - bind address
   - port
   - owner database
   - parser-pool min and max
   - spawn strategy
   - engine endpoint
3. `ServiceController` spawns the protocol-specific listener binary.
4. The listener validates its launch arguments and opens:
   - front-door socket
   - local control socket
   - local management socket
5. The listener warms the parser pool to the required minimum unless current
   spawn strategy permits on-demand startup.

### Manager-proxy path

1. `ServiceController` spawns the optional manager proxy using the manager
   configuration block.
2. `ServiceController` also spawns the internal native listener with managed
   requirements:
   - `listener_id`
   - `dbbt_clock_skew_ms`
   - `dbbt_replay_cache_size`
   - `require_proxy_binding`
   - optional `dbbt_keyring`
3. The internal native listener must bind only on loopback in manager-proxy
   mode.
4. The manager authenticates the front-door client and selects the owner
   database within manager-proxy scope.
5. The manager issues a `DBBT`, builds an `LPREFACE`, and asks the listener to
   validate it over the local management channel.
6. Only after successful validation does the manager proxy traffic to the
   internal native listener.

### Current emulation boundary

Manager-proxy mode is currently a native ScratchBird front-door path.

Current code does not promote the manager to a universal proxy for every
emulated protocol family.

## Dependency graph

- `ServiceController`
  -> bootstrap configuration
  -> process control
  -> engine endpoint resolution
  -> listener binaries
  -> optional manager binary
- optional manager proxy
  -> front-door manager socket
  -> manager authentication
  -> `DBBT` issuance
  -> listener management socket
  -> internal native listener
- listener process
  -> local control-plane implementation
  -> parser workers
  -> engine endpoint
  -> telemetry registry
- parser worker
  -> protocol handler
  -> engine IPC server
  -> engine IPC session handler
- engine IPC session handler
  -> database connection context
  -> catalog manager
  -> SBLR execution surfaces

## Hard boundaries

- Listener control-plane traffic is a bounded internal runtime seam, not a
  public extension ABI.
- Listener management requests are execution requests, not durable policy truth.
- The manager is optional and native-scope; it is not current proof of a
  universal multi-emulation proxy surface.
- Cluster-fabric, replication-fabric, and migration runtimes remain outside
  current section `29` process authority.
