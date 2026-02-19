# PSQL Language Guide
Last modified: 2026-02-19

Back links:
- [Language Guide README](../README.md)

This section documents procedural SQL (PSQL) syntax and runtime behavior in native parser v3.

PSQL families:
- [Routine Structure](routine-structure/README.md)
- [Control Flow](control-flow/README.md)
- [Error Handling](error-handling/README.md)
- [Execution Context](context/README.md)

Important dialect notes:
- SET TERM is supported but statement splitting around alternate terminators remains a caller/client workflow.
- Dollar-quoted body delimiters (296232 ... 296232) are not supported in native parser v3.
- TRY/CATCH block syntax is not supported; use WHEN/EXCEPTION flows.
