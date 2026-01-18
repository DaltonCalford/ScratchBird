[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Security (DCL)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## GRANT / REVOKE
Description: MySQL privilege statements are not implemented in the parser.

Status: Missing.

## CREATE USER / ALTER USER / DROP USER
Description: Not implemented in MySQL parser.

Status: Missing.
