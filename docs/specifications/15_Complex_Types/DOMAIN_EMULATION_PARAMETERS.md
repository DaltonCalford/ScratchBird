# Domain Emulation Parameters

Status: current_authority

## Current authoritative parameter surface

The current audited structured parameter surface is narrower and more implementation-owned than historical prose claimed.

## Parameter and hint matrix

| Parameter surface | Current authority | Main boundary |
| --- | --- | --- |
| DomainCreateOptions | nullable, default_value, constraints, collation_name, dialect_tag, compat_name, enum_wrap, fixed_domain_id, and allow_system_reserved_name are real runtime inputs | not a universal donor-engine parameter catalog |
| BaseTypeInfo | base-domain input and output function requirements are real | scope is bounded to base-domain runtime truth |
| EmulatedTypeMapping storage_kind | current storage-kind hints are real | storage-kind truth is not semantic parity |
| EmulatedTypeMapping canonical_type | current canonical-type hint is real | mapping presence is not semantic parity |
| EmulatedTypeMapping domain_hint | current domain-hint truth is real | hint presence is not full domain-behavior proof |
| EmulatedTypeMapping parser_rule_hint | bounded current truth through requiresWholeValueUpdate and allowsElementLevelMutation | not full front-door semantics |
| historical donor-specific parameter inventory | not claimed as current authority | preserved prose is not re-audited current proof |

## Fail-closed boundary

This section does not claim a fully audited universal parameter inventory for:
- every donor engine
- every complex family
- every nested-value rule
- every geometry, vector, json, or xml option key
