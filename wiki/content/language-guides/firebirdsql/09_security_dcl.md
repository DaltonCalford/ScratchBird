[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Security (DCL)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`

## GRANT / REVOKE
Description: Firebird privilege and role management.

Status: Missing.
Spec delta: Parser emits errors for GRANT/REVOKE.

## CREATE/ALTER/DROP ROLE, USER
Description: Firebird security DDL.

Status: Missing.
