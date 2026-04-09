# SBLR Verifier and Validation Rules

Status: current_authority

## Mandatory verifier position

The verifier runs before shared planning and execution. A container that fails verification must not reach compilation or execution.

## Required validation classes

1. header and container-family validation
2. section inventory and singleton duplication checks
3. compatibility manifest validation
4. constant-pool bounds and type-tag validation
5. opcode-symbol validity and family membership validation
6. statement payload schema validation
7. domain payload schema validation
8. reference integrity checks across statements, domains, and constants
9. durable identity checks for UUID-bound objects
10. parser-normalization contract checks for committed-baseline catalog resolution

## Stable rejection classes

Verifier diagnostics must be stable by class even if message wording changes. At minimum, implementations must distinguish:

- malformed container
- unsupported version or section
- invalid reference or index
- opcode and schema mismatch
- invalid domain payload
- unresolved durable identity
- forbidden unsupported feature emission

## Fail-closed rule

A verifier may reject more aggressively as the canonical registry hardens. It must not accept malformed or ambiguous artifacts in the name of forward compatibility.
