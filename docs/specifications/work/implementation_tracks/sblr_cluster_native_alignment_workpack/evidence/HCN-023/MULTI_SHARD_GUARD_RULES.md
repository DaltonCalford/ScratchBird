# Multi-Shard Guard Rules

Evaluation inputs:
- list of target shard IDs for a write statement.
- guard policy: `allow_cross_shard`, `require_explicit_override`.
- runtime override flag.

Rules:
1. If unique shard count <= 1, allow.
2. If unique shard count > 1 and `allow_cross_shard=false`, reject.
3. If unique shard count > 1 and `allow_cross_shard=true` and `require_explicit_override=true` and override is missing, reject.
4. If unique shard count > 1 and policy allows with override present, allow.
