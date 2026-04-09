# Join / Rejoin Flow

Validated sequence:
1. Upsert cluster and node metadata.
2. Attach role/service/binding metadata.
3. Upsert shard/scope/range/replica/migration metadata.
4. Retrieve/list state and verify deterministic values.
5. Delete in dependency-safe order.

Observed behavior:
- Invalid/duplicate records are rejected.
- Valid records round-trip through get/list APIs deterministically.
