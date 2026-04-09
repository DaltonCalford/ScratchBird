# Filespace File Layout

Status: current_authority

## 1. Scope and authority

This document defines the current physical file-layout split between the primary database file and
secondary tablespace files.
The controlling binary authority is the current engine file-header and page-type implementation,
including the current `DatabaseHeader`, `TablespaceHeaderV1` or `TablespaceHeader`, bootstrap page map,
and `GPID` semantics.

This file replaces older prose that treated every filespace as if it began with one generic
`FilespaceHeader`.
That is not current ScratchBird truth.

## 2. Primary database file layout

The primary database file is special.
It begins with the primary database header and the fixed bootstrap page map.

Current required primary-file prefix:

| Physical page number | Required role |
| --- | --- |
| `0` | Primary `DatabaseHeader` page |
| `1` | System-state bootstrap page |
| `2` | Catalog-root bootstrap page |
| `3` | FSM-root bootstrap page |
| `4` | Transaction-map bootstrap root |
| `5` | Reserved bootstrap page |

After the fixed bootstrap region, the primary file may contain normal storage families according to the
allocator, free-space, catalog, transaction-map, and data-page rules in the relevant sections.

## 3. Secondary tablespace file layout

Secondary tablespace files do not start with the primary database bootstrap map.
They begin with the current tablespace header structure owned by the filespace subsystem.
The current header authority is `TablespaceHeaderV1` or `TablespaceHeader`, depending on the active code
path and compatibility surface.

Rules for secondary tablespace files:

1. the first page is a tablespace header, not a primary database header
2. pages `1` through `5` are not reserved as a cloned primary bootstrap map
3. primary-database bootstrap roles remain owned by the main database file
4. tablespace-local allocation and object placement metadata are governed by the tablespace header and the
   catalog-driven placement model

## 4. GPID contract

The current global page identifier contract is:

1. `GPID` is a `16-bit tablespace id + 48-bit page number`

This is the cross-filespace addressing rule that allows logical page identity to span the primary file and
secondary tablespaces without flattening them into one implicit single-file namespace.

Implementation rules:

1. the tablespace identifier portion selects the physical file domain
2. the page-number portion selects the page within that domain
3. code and spec text must not silently collapse `GPID` into a single local page number
4. any parser, executor, catalog, or repair logic that persists page references across filespaces must use
   the full `GPID` contract

## 5. Header ownership boundary

Header ownership is split by file class.

| File class | Owning header contract |
| --- | --- |
| Primary database file | `DatabaseHeader` plus fixed bootstrap pages |
| Secondary tablespace file | `TablespaceHeaderV1` or `TablespaceHeader` |

This split is mandatory.
A limited implementation agent must not unify these into one synthetic generic file header.

## 6. Lifecycle consequences

The file-layout split has these operational consequences:

1. startup authority begins in the primary database file
2. tablespace discovery is downstream of primary bootstrap and catalog authority
3. corruption handling must distinguish primary bootstrap incidents from secondary tablespace incidents
4. attach, detach, placement, and relocation operations must preserve the `GPID` addressing model
5. repair tooling must not rewrite secondary files as if they were standalone primary databases

## 7. Transaction and durability boundary

Filespace layout does not change the core transaction model.
ScratchBird remains MGA-based, always in a transaction, and does not derive correctness from `WAL` or
log-sequence authority.

This means:

1. filespace identity and page placement are catalog and header concerns
2. transaction visibility remains MGA-driven
3. recovery and checkpoint handling must not invent a WAL-owned filespace truth model

## 8. Negative requirements

The following are explicitly prohibited as current truth:

1. treating the primary file as if it started with a generic `FilespaceHeader`
2. cloning the fixed bootstrap page map into every secondary tablespace
3. collapsing `GPID` into a single local page number without the tablespace-id component
4. describing filespace correctness in WAL or LSN terms

## 9. Implementation and test contract

Any implementation or audit against this file must prove:

1. the primary file begins with `DatabaseHeader` plus the fixed bootstrap map
2. secondary tablespaces begin with the current tablespace header family
3. `GPID` preserves the `16-bit tablespace id + 48-bit page number` contract
4. startup authority flows from the primary file, not from detached tablespace-local bootstrap guesses
5. tablespace handling does not reintroduce a generic unified file-header fiction
