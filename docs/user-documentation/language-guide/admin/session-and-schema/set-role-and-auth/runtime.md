# Admin SET ROLE AND SESSION AUTHORIZATION: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Form
~~~sql
SET ROLE <role_name>; SET SESSION AUTHORIZATION <user_name>;
~~~

## Notes
- Details: Role/auth command surfaces are explicit in SET dispatch.
- Runtime note: Session identity and role context are applied in runtime session state.
- Error/contract note: Privilege checks apply to identity and role transition operations.
- Usage rationale: Explicit privilege context management per session.
