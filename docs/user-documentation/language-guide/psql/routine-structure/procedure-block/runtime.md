# PSQL PROCEDURE BODY BLOCK: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Form
~~~sql
CREATE PROCEDURE <name>(...) AS DECLARE ... BEGIN ... END;
~~~

## Notes
- Details: Native v3 parser treats routine bodies as structured blocks with DECLARE and BEGIN..END.
- Runtime note: Core structured block parsing is available in 0.1.0.
- Error/contract note: Body syntax errors are deterministically rejected by parser contracts.
- Usage rationale: Primary routine implementation surface for business logic.
