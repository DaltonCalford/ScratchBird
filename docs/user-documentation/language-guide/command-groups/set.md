# Command Group: SET
Last modified: 2026-02-19

Back links:
- [Language Guide README](../README.md)
- [Command Group Index](README.md)

Series navigation:
- Previous: [SELECT](select.md)
- Next: [SHOW](show.md)

Native v3 `parseSet()` dispatch includes:

- `SET SESSION AUTHORIZATION`
- `SET TIME ZONE`
- `SET AUTOCOMMIT`
- `SET TRANSACTION`
- `SET CONSTRAINTS`
- `SET SQL DIALECT`
- `SET NAMES`
- `SET LOCAL_TIMEOUT`
- `SET CONSISTENCY`
- `SET SERIAL CONSISTENCY`
- `SET CONCURRENCY MODE`
- `SET SINGLE_WRITER`
- `SET SEQUENCE`
- `SET GENERATOR`
- `SET ROLE`
- `SET TERM <new> [old]`
- `SET SCHEMA [=|TO] <path|DEFAULT>`
- `SET CURRENT_SCHEMA [=|TO] <path|DEFAULT>`
- `SET PARSER VERSION` (parsed then rejected)
- Generic variable assignment: `SET [SESSION|LOCAL] <name>[.<name>...] =|TO <expr|DEFAULT>`

Session and schema path details are in [Admin](../admin/README.md) and [PSQL](../psql/README.md).
