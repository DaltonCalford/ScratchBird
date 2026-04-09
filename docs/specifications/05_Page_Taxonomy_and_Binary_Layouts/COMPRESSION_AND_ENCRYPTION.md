# Compression and Encryption

Status: current_authority

## Current authority

Current code-backed authority includes:
- page-header flags declaring compressed and encrypted page-image state
- compression framing and helpers used by the current compression path
- backup and compatible page-image handling paths that preserve stored-image integrity semantics

## Current guarantees

- compression and encryption are current page-image concerns, not transaction or recovery authority
- checksum and integrity rules apply to the stored page image after current page-image transformations
- page families that do not support a transformation must fail closed rather than silently reinterpret payload layout

## Non-claims

This file does not claim:
- direct universal compression metadata fields in every common page header
- universal compression and encryption support across every page family
- a closed matrix for every reserved emulation family
