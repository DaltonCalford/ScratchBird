# Upgrade Downgrade and Compatibility Boundary

This file owns the bounded lifecycle compatibility model and is governed by SYSTEM_COMPATIBILITY_MANIFEST_AND_OPERATIONAL_ROLLOUT_MODEL.md.

## Lifecycle compatibility matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| bounded upgrade path | bounded_current_authority | supported rollout claims exist only where the system compatibility manifest and current gates directly prove them | not broad rolling-upgrade support |
| downgrade support | fail_closed | downgrade remains fail-closed unless explicitly proven | not backward-compatibility folklore |
| protocol/catalog compatibility | bounded_current_authority | compatibility may exist in bounded form where the manifest admits it | not universal cross-version parity |
| deprecation lifecycle | bounded_current_authority | lifecycle language may identify bounded current or target-state removals | not a fully formalized product-policy regime |

## Canonical rules

1. Upgrade and compatibility claims require direct current proof.
2. Downgrade remains fail-closed unless section-local truth says otherwise.
3. Version-compatibility language must stay narrower than general ecosystem expectations.

## Explicit non-guarantees

- no universal rolling-upgrade guarantee
- no blanket downgrade support
- no stable multi-major compatibility promise
