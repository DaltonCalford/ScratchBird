# IMP-26 Implementation Checklist

## Ticket
- ID: IMP-26
- Section: 26_Native_Wire_Protocol
- Gate Contract: docs/specifications/26_Native_Wire_Protocol/TEST_CONTRACT.md

## Inputs
- docs/specifications/26_Native_Wire_Protocol/SPEC_OUTLINE.md
- docs/specifications/26_Native_Wire_Protocol/IPC_SBWP_FRAME_SPEC.md
- docs/specifications/26_Native_Wire_Protocol/MESSAGE_CATALOG_AND_SCHEMAS.md
- docs/specifications/26_Native_Wire_Protocol/SERVICE_CHANNELS_AND_STREAMING.md
- docs/specifications/26_Native_Wire_Protocol/CLUSTER_UDR_FABRIC_CHANNEL_SPEC.md
- docs/specifications/26_Native_Wire_Protocol/PROTOCOL_STATE_MACHINES.md
- docs/specifications/26_Native_Wire_Protocol/TEST_CONTRACT.md

## Ordered Tasks
1. Implement frame conformance and message catalog matrices.
2. Implement execution transport and service channel matrices.
3. Implement protocol state machine and negative protocol matrices.
4. Implement performance, P1 distributed wire, and cluster fabric channel matrices.

## Exit Criteria
- Required suites A-I pass.
- Gate result is pass.
- Wire contracts are deterministic and profile-gated.
