# Architecture

**Last Updated:** 2026-02-03

---

ScratchBird is layered to isolate dialect parsing from the engine core.

Layers:
1. Listener + wire protocol adapter
2. Dialect parser
3. SBLR bytecode
4. Engine core (storage, transactions, indexes)

Key rule: only SBLR reaches the engine; SQL text never does.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
