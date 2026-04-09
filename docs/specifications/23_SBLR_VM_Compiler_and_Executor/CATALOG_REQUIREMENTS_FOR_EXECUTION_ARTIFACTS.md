# Catalog Requirements for Execution Artifacts

Status: current_authority_with_reconstructed_expansion

Current audited catalog-facing anchors are narrower than the older section prose.

## Capability states

- `normalization_evidence_hash_storage`: `bounded`
- `plan_profile_or_identifier_hashes`: `bounded`
- `universal_execution_artifact_catalog`: `unproven`
- `persistent_vm_module_catalog`: `target_state_only`

Current proof:
- normalization-evidence hash storage fields exist in catalog code paths
- plan-cache invalidation depends on catalog or security epochs and related signatures
- runtime-plan payloads and compiler plan profiles carry identifiers and hashes in the current execution path

Not proven now:
- a universal catalog schema for VM modules, compiled artifacts, plan artifacts, and cache artifacts
- complete persistent artifact lifecycle ownership in catalog tables
