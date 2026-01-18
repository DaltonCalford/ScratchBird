# What makes ScratchBird different

ScratchBird is not just a rewrite. It is a deliberate attempt to build the database I always wanted while staying honest about what it is and what it is not. The differences below are not marketing features. They are choices I made to keep the project clear, safe, and compatible.

---

## It starts with MGA, not WAL

I chose Firebird's Multi-Generational Architecture (MGA) because it fits how I want a database to behave under real load. Most engines write to a WAL and then read from that WAL to apply changes to the database. MGA writes directly to the database. When a row changes, the new version is written alongside the old one. That means:

- **Commits are fast** because they are mostly a flag change, not a heavy write.
- **Every user is always in a transaction**, and every transaction is a snapshot.
- **Readers do not block writers, and writers do not block readers.**
- **Recovery is fast and safe** because failures do not lose data or leave half-applied changes.

ScratchBird still supports a write-after log for replication or recovery, but it is not the engine's spine. I wanted the transaction model to be clean first, not bolted on later.

---

## The engine speaks SBLR, not SQL

ScratchBird runs SBLR bytecode internally. SQL is a parser concern, not an engine concern. That boundary is deliberate. SBLR treats the engine like a VM (think Java): a simple, machine-readable instruction stream that can be optimized. Post-alpha, the plan is to allow native compilation of frequently used procedures so hot paths run fast.

Security is another reason for this design. Every incoming SQL statement is converted into SBLR by the parser (first line of defense). Then the engine audits the SBLR itself (second line of defense). That two-layer conversion and verification is how I avoid injection attacks and keep the core consistent.

---

## Parsers emulate, they do not extend

The Firebird, PostgreSQL, and MySQL parsers are there to make clients feel at home, not to expand the language. I do not want a MySQL client to see ScratchBird-only features or Firebird-only behavior. Parity is the goal. That rule protects compatibility and prevents confusing, half-true behavior.

---

## I do not trust clients, even local ones

Local IPC clients are not trusted any more than network clients. Every SBLR payload is verified, cached, and security checked inside the engine. That discipline is a direct response to the risks I saw when AI started guessing. If the core trusts nothing, it stays harder to break.

---

## Isolation is a feature, not an accident

The network listener and parser processes are intentionally separated. It keeps one bad client or one bad parser from bringing down everything else. That is why the design favors pools of parsers, clear handoffs, and strong boundaries. It is old-school stability with modern tooling.

---

## Specs and tests come before speed

This project exists because I wanted a way to safely work on a complex system with AI in the loop. Specs, plans, and regression tests are not red tape. They are the guardrails that keep progress real. I would rather be slower and right than fast and wrong.

---

## Recursive schemas, because names should mean something

I wanted a schema model that lets me group the things that belong together. When I was navigating databases, I hated seeing one task scattered across different schemas just because names collided or the structure was flat. ScratchBird uses recursive, multi-level schemas with a name resolution process. That gives me the flexibility to:

- Group objects by user, project, or task.
- Mirror the structures that emulated clients expect.
- Keep each emulated engine cleanly separated while still exposing everything to native users.

It is a design choice that keeps order in the catalog and makes navigation sane.

---

## It is not Firebird, and it is not trying to be

ScratchBird is a scratchpad by design. It is a place to learn, experiment, and prove ideas without pretending to be the original. The hope is that some of those ideas can flow back to Firebird, but the project stands on its own terms and its own rules.
