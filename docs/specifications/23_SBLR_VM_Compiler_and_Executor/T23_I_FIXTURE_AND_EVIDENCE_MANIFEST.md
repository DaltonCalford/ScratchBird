# T23-I Fixture and Evidence Manifest

Status: current_authority_with_reconstructed_expansion

Current section-23 evidence should be anchored to live sources and bounded test surfaces around:
- `QueryPlanner::planStatement(...)`
- runtime-plan payload encode or decode
- join-ordering, access-path, and statistics-backed plan formation
- `VNextPlanCache` keying, reuse, stats, and invalidation
- compiler trace and plan-profile emission
- bytecode validation through the `v3_validator` bridge
- executor explain or runtime-plan consumption paths

This manifest is not proof of the historical full VM or distributed execution contract.
