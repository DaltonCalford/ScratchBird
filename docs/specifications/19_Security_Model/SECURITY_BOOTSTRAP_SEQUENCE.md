# Security Bootstrap Sequence

Status: current_authority

## Ordered sequence

1. Load immutable bootstrap security configuration.
2. Initialize cryptographic primitives and secure randomness required by configured security features.
3. Load node identity, server identity, and key-descriptor metadata required before protected storage is opened.
4. Validate authentication manager configuration and load the allowed built-in and signed extension set.
5. Load and validate required key and certificate material.
6. Open protected stores or encrypted files only after required key material validates.
7. Publish security readiness to the listener and service-controller layer.
8. Only after security readiness is published may remote listener acceptance begin.

## Fail-closed rule

Any failure in this sequence blocks listener exposure and engine service readiness until the condition is corrected and bootstrap restarts or reload completes successfully.
