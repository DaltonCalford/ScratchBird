# Code Area Ownership Map

## Primary Write Scopes By Ticket

| Ticket | Primary Write Scope | Conflict Files | Parallelization Notes |
| --- | --- | --- | --- |
| SV-09-001 | `docs/work-plans/09-Canonical_Spec_Status_Audit_and_Search_Key_Migration/` | package trackers | do not parallelize until status vocabulary and proof rules are frozen |
| SV-09-002 | work-plan package trackers plus canonical inventory-driven audit outputs | `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`, `SPEC_STATUS_CLASSIFICATION.csv` | can parallelize per section only after the bootstrap scope is frozen |
| SV-09-003 | canonical spec files plus owned implementation files that need durable search keys | touched canonical spec files, touched implementation files, migration log | high conflict risk; serialize by section or repo root |
| SV-09-004 | `docs/specifications/Reference_Documentation_specification.md`, sections `00`-`13`, matching implementation roots | section README files, `TEST_CONTRACT.md`, shared implementation files | safe to parallelize only if write scopes do not overlap |
| SV-09-005 | sections `14`-`23` plus matching implementation roots | section README files, `TEST_CONTRACT.md`, shared runtime files | coordinate around shared type, AST, SBLR, and executor files |
| SV-09-006 | sections `24`-`31` plus matching implementation roots | section README files, `TEST_CONTRACT.md`, parser, listener, tooling, and gate files | parser and listener files are high-conflict surfaces |
| SV-09-007 | sections `32`-`42` plus explicitly referenced sibling repos | section README files, `TEST_CONTRACT.md`, shared platform/runtime files | cross-repo coordination required for driver and benchmark roots |
| SV-09-008 | final rollup outputs and package trackers | final inventory markdown and csv files | serialize final closeout to avoid rollup drift |

## Safe Shared Rules

1. Search-key insertion into implementation files and canonical spec rewrites
   for those same files belong to the same ticket.
2. A section audit is not complete until:
   - the canonical file status claim is updated
   - the audit matrix row exists
   - any needed search-key migration is logged
3. Sibling repos are in scope only when a canonical spec explicitly points to
   them as current implementation authority.
