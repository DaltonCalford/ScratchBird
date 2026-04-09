# Catalog Emulation Profile

Status: current_authority

## Current authority

emulation_profile is a real runtime authority for deciding which emulation overlay handlers are registered at startup.

Current source proves:
- catalog entries can be listed for emulation-profile evaluation
- overlay registration is conditional on those entries
- the virtual catalog router can register engine-specific handlers only when the profile allows it

## Capability-state split

- core_overlay: proven for information_schema and sys_catalog
- profile_gated_engine_overlay: proven for current engine handlers when enabled by profile state
- donor_semantic_parity: unsupported

## Boundary

This file does not claim:
- universal semantic parity with donor engines
- complete package lifecycle parity across every emulation profile
- exhaustive policy semantics beyond the current profile-gated registration behavior
