# IMP-27 Implementation Checklist

## Ticket
- ID: IMP-27
- Section: 27_Native_Handshake
- Gate Contract: docs/specifications/27_Native_Handshake/TEST_CONTRACT.md

## Inputs
- docs/specifications/27_Native_Handshake/SPEC_OUTLINE.md
- docs/specifications/27_Native_Handshake/HANDSHAKE_MESSAGE_SCHEMAS.md
- docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md
- docs/specifications/27_Native_Handshake/REGISTRY_EXCHANGE_AND_VISIBILITY.md
- docs/specifications/27_Native_Handshake/HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md
- docs/specifications/27_Native_Handshake/TEST_CONTRACT.md

## Ordered Tasks
1. Implement transcript and authentication negotiation matrices.
2. Implement identity stack and registry visibility matrices.
3. Implement failure matrix and negative handshake matrices.
4. Implement fabric channel profile matrices.

## Exit Criteria
- Required suites A-G pass.
- Gate result is pass.
- Handshake/auth/identity behavior is deterministic and policy-bound.
