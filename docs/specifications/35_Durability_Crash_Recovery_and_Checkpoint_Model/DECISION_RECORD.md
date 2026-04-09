# Section 35 Decision Record

- ScratchBird durability and recovery remain MGA-centered.
- WAL is not Alpha recovery truth.
- This section unifies durability/recovery/checkpoint boundaries that were previously split across sections 08, 10, and 31.
- No donor-engine redo/undo narrative is adopted unless a future code-backed section explicitly owns it.
