# Encryption and Key Management

Status: current_authority

## Authority model

Data-at-rest protection is controlled by bootstrap-managed encryption policy and key descriptors. Secret material is not derived from SQL catalog state.

## Required rules

- Protected stores must not be opened without validated key material.
- Key selection is driven by configured descriptors and bootstrap metadata, not by page-local guessing.
- Key rotation is a controlled maintenance operation with explicit source and target key descriptors.
- Partial or mixed-key activation without an explicit supported transition path is forbidden.
- Recovery operates on protected page and metadata state directly; it is not WAL-driven.

## Refusal rules

- missing key descriptors
- unreadable or invalid wrapped keys
- mismatched encryption profile identifiers
- attempts to continue in plaintext after protected-open failure
