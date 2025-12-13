# TODO: First-Class Cursors as Shareable Row Sources

Goal: Allow cursors/result sets to be treated as first-class variables/row sources, passable between triggers/functions/procedures within a transaction, and usable in FROM clauses (e.g., `SELECT ... FROM cursor_handle INTO :vx...`).

## Current State
- Cursors/portals are session-local to a statement execution; not modeled as catalog objects or passable handles.
- No language support for declaring/passing cursor handles; no FROM-cursor row source.

## Requirements
- Cursor handle type in SBLR/runtime, scoped to transaction/session, with explicit lifecycle (open/close) and auto-close on txn end/exception.
- Language/Parser: DECLARE/OPEN/FETCH/CLOSE; ability to pass cursor handles as parameters/returns; allow cursor handles in FROM.
- Executor: cursor registry (per session/transaction), row-source adapter for cursor-backed scans, INTO bindings compatible with existing fetch semantics.
- Permissions/visibility: respect transaction isolation; cursors not catalog objects; enforce same-session use.
- Cleanup: ensure determinism on errors, commit/rollback; disallow use after close/end.

## Work Items
1) Add cursor handle type to AST/Semantic (including argument/return typing).  
2) Implement runtime cursor registry and handle lifecycle (open/close/auto-close).  
3) Add row-source adapter to scan an existing cursor in FROM.  
4) Extend SBLR executor opcodes for cursor operations and handle passing.  
5) Update parser/bytecode generator for DECLARE/OPEN/FETCH/CLOSE/USE-IN-FROM.  
6) Tests: pass cursor between nested routines/triggers; use cursor in FROM; error cases (closed handle, cross-txn use).  
7) Docs: document cursor semantics, scope, and error behavior.  

## Constraints
- ScratchBird-only feature; emulated engines remain limited to their native cursor semantics.  
- Handles are transaction/session scoped; never catalog-persisted.  
