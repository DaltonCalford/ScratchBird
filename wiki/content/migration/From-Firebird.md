# From Firebird

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-09

ScratchBird uses MGA like Firebird, so transaction semantics map cleanly.

Migration checklist:
1. Review data types and domain definitions.
2. Validate triggers/procedures in the Firebird dialect.
3. Verify system catalog expectations in emulation mode.

See:
- `docs/user-documentation/migration/`
- `docs/specifications/reference/firebird/`
