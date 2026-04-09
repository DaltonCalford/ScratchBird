# Section 26 Decision Record

## Decision 1: native and IPC headers are separate current authorities

Current ScratchBird transport uses two distinct header families:
- native wire header `SBDB`
- internal IPC header `SBIP`

This section does not pretend they are one unified production header.

## Decision 2: current native wire version is `1.1`

The native wire contract uses:
- `PROTOCOL_MAGIC = 0x42444253` (`SBDB`)
- major `1`
- minor `1`
- 12-byte fixed header

## Decision 3: current IPC contract version is `1.1`

The internal IPC contract uses:
- `IPCHeader::MAGIC = 0x53424950` (`SBIP`)
- `IPC_CURRENT_VERSION = 0x0101`
- 40-byte fixed header

## Decision 4: control plane is not owned by section 26

Managed listener `DBBT` and `LPREFACE` framing belongs to section `29` and its
adjacent ownership surfaces, not to the native wire header defined here.

## Decision 5: result and error transport are deterministic but profile-specific

Current result and error frame mapping is deterministic, but native and IPC
profiles use different message families and payload structures. This section
documents both without inventing artificial parity.

## Decision 6: replay, cluster fabric, and distributed-wire expansion are not current authority

The replay session profile, cluster fabric channel, and distributed read or
telemetry transport remain explicit unsupported-boundary surfaces until direct
runtime code and maintained tests exist.
