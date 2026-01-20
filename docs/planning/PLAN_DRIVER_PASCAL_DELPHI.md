# Driver Plan: Pascal / Delphi

Status: Draft

Spec references:
- docs/specifications/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- docs/specifications/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- docs/specifications/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver FireDAC, IBX, Zeos, and FreePascal SQLdb connectivity using SBWP.

Scope
- Network-only driver libraries and component bindings.

Plan
1) Core library
- Implement a Pascal client library wrapping SBWP.
- Provide connection string mapping and TLS/auth handling.

2) Component adapters
- FireDAC driver module.
- IBX compatibility layer.
- Zeos and SQLdb adapters.

3) Types + metadata
- Map types per baseline spec and expose metadata APIs.

4) Packaging
- Delphi packages and FreePascal units.

Testing
- Unit tests for client library.
- Integration tests for each component adapter.

Dependencies
- libscratchbird client core or Pascal-native SBWP implementation.
