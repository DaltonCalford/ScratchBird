# ScratchBird Project Status

Date: 2026-02-27
Status: Public Beta 1 (code/tests baseline)

## Summary

ScratchBird is at a public beta engineering state where core implementation and regression/conformance gates are active and used as acceptance criteria.

## Confirmed Baseline

- Canonical parser/runtime model: `v3`
- Emulation adapters in active verification scope: PostgreSQL, MySQL, Firebird
- Security conformance lanes active: row-level security, column-level security, domain-level security/masking
- Transaction/MGA verification active for compatibility dependency lanes

## Required Beta Gate Categories

The required public-beta gate validates these categories:

1. wire protocol correctness
2. transaction semantics
3. security enforcement
4. end-to-end SQL correctness
5. modal/NoSQL surface verification
6. cluster infrastructure verification

All required categories are expected to pass before release tagging.

## Release Positioning

- Public beta means feature and behavioral hardening is still in progress.
- This status does not imply GA support, long-term compatibility guarantees, or production certification.
