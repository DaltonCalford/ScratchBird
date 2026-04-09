# IMP-30 Implementation Checklist

## Ticket
- ID: IMP-30
- Section: 30_Client_Tooling
- Gate Contract: docs/specifications/30_Client_Tooling/TEST_CONTRACT.md

## Inputs
- docs/specifications/30_Client_Tooling/SPEC_OUTLINE.md
- docs/specifications/30_Client_Tooling/EMBEDDED_AND_LINKED_LIBRARY_API.md
- docs/specifications/30_Client_Tooling/CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md
- docs/specifications/30_Client_Tooling/INSTALLER_PROFILES_AND_ARTIFACTS.md
- docs/specifications/30_Client_Tooling/TOOL_COMMAND_SURFACE_CONTRACTS.md
- docs/specifications/30_Client_Tooling/CLIENT_ERROR_AND_RESULT_MODEL.md
- docs/specifications/30_Client_Tooling/TEST_CONTRACT.md

## Ordered Tasks
1. Implement API ABI/lifecycle and connectivity profile matrices.
2. Implement installer profile and statement/result/tool command matrices.
3. Implement error/exit and negative behavior matrices.
4. Implement migration and replication control surface matrices.

## Exit Criteria
- Required suites A-H pass.
- Gate result is pass.
- Client API/tool surfaces are deterministic and profile-bound.
