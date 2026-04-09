# Domain Security Masking Encryption and Audit Model

## Purpose

Define the security model attached to domains, including masking, unmasked privilege bypass, encryption metadata, and audit flags.

## Governing Rules

1. Domain security is part of the canonical domain definition.
2. Domain security applies independently of table layout and can be reused across multiple columns through domain binding.
3. Masking is enforced unless the caller has the required unmasked privilege.
4. Audit and encryption metadata are stored with the domain security payload, not inferred ad hoc at query time.

## Domain Security Payload

The canonical domain security payload carries:

- masking type
- encryption enabled flag
- audit enabled flag
- encryption algorithm
- permission mask
- masking pattern
- full-mask character
- required privilege for unmasked access
- encryption key identifier

Default or empty security may be omitted from storage only when every security field is at its canonical default.

## Masking Types

Supported masking classes proven in code are:

- `NONE`
- `FULL`
- `PARTIAL`

## Full Masking

Full masking shall:

1. validate that the configured mask character is non-empty
2. count logical UTF-8 characters when the input is valid UTF-8
3. fall back to byte-count masking when the input is not valid UTF-8
4. emit one mask token per character or byte unit

## Partial Masking

Partial masking shall use a pattern string with the following semantics:

- `#`
  - preserve the next input character if present, otherwise emit the mask token
- `X`
  - consume the next input character if present and emit the mask token
- any other character
  - emit the literal pattern character and consume the next input character only when it matches that literal

Any remaining input characters after the pattern is exhausted shall be masked.

## Privilege Bypass

If the caller has the required unmasked privilege, the raw value shall be returned unchanged.

If the caller lacks that privilege, masking shall be applied according to the configured masking type.

## Persistence and Integrity

Domain security is versioned binary payload data. Invalid versioning or malformed payload contents are corruption conditions and shall fail closed.

## Reuse and Binding

Columns that bind to a secured domain inherit that domain security contract. The column does not need to restate the security rules to receive masking behavior.

## Current Proof and Rebuild Boundary

Current code proves:

- binary domain security serialization and deserialization
- masking algorithm behavior
- privilege-based masking bypass
- audit and encryption fields in the domain security record

This specification reconstructs the broader product rule that domain security is a first-class security contract, not a display-only formatter.
