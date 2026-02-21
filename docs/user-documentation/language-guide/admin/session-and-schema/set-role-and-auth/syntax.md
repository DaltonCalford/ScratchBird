# Admin SET ROLE AND SESSION AUTHORIZATION: Syntax
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Topic README](README.md)
- Next: [Semantics](semantics.md)

## Coverage
- Status: Supported

## Forms
~~~sql
SET ROLE <role_name>;
SET ROLE NONE;
SET ROLE DEFAULT;
RESET ROLE;

SET SESSION AUTHORIZATION <user_name>;
SET SESSION AUTHORIZATION DEFAULT;
RESET SESSION AUTHORIZATION;
~~~

## Token Rules
- `<role_name>` and `<user_name>` accept identifier or string literal.
- `SET SESSION AUTHORIZATION DEFAULT` is equivalent to reset semantics.
- `SET ROLE NONE` and `SET ROLE DEFAULT` map to reset semantics.

## Notes
- Parser accepts optional `SESSION`/`LOCAL` scope prefix, but role/auth operations currently execute as session-level context changes.
