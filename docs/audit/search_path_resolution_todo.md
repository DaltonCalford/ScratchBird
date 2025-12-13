# TODO: Schema/Search Path Resolution & Indexing

Goal: Formalize session/connection search path handling and object lookup so name resolution is deterministic and efficient.

## Requirements
- Session state: current schema and search path (ordered list). SET commands to manage both in parser/engine.
- Resolution: left-to-right search path; first matching object name wins. Emulated databases: sandboxed to their schema/sub-schema (default path confined).
- Object identity: every SQL object has a UUID and an immediate name; also store a full path string (e.g., `.users.dalton.testing.mytable`).
- Indexing: hash index on UUID; B-tree index on full path; lookups use full path to avoid collisions on identical immediate names in different paths.
- Visibility: default SHOW/list commands honor search path and sandbox rules; fully-qualified names always resolve directly.

## Work Items
- Define/set parser commands for current schema and search path.
- Implement search-path aware lookup in semantic/executor layers using full path/B-tree for unambiguous resolution; UUID/hash for identity.
- Enforce emulation sandboxing: path defaults to emulated schema and sub-schemas only.
- Tests: resolution with overlapping names across schemas/packages; search path ordering; sandbox constraints; SET path changes.

## Notes
- Packages are schema-like containers; default listings exclude package members unless explicitly qualified (see package notes).
- Emulated engines must not resolve outside their sandbox unless via explicit cross-schema constructs (e.g., synonyms/views).
