# Join Repair Mode Test Results

Validated by `DomainControlPlaneReplicaCatalogTest.DetectsJoinManifestHashAndMembershipMismatch`.

Observed mismatch classes:
- `remote_zero_domain_id`
- `remote_duplicate_domain_id:<uuid>`
- `hash_mismatch:<uuid>`
- `missing_remote_domain:<uuid>`
- `unexpected_remote_domain:<uuid>`

Repair implication:
- Operators can deterministically classify join divergence into identity, duplication, or definition drift categories using reason codes alone.
