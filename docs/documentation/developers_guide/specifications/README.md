# Specifications (Reverse-Engineered from Implementation)

## Purpose

This directory contains **authoritative specifications** derived from actual ScratchBird implementation code. These specs serve as:

- **Source of truth** for developers working on the codebase
- **Contract documentation** for subsystem interfaces and behaviors
- **Implementation references** with verified source anchors to actual code

## Quick Navigation

| Subsystem | Specs | Description |
|-----------|-------|-------------|
| [parser/](parser/) | 3 specs | V3 SQL grammar, path resolution, semantic binding |
| [sblr/](sblr/) | 4 specs | Opcodes (877), container format, execution model |
| [storage/](storage/) | 5 specs | MGA, transactions, version chains, GC, page layout |
| [catalog/](catalog/) | 4 specs | Bootstrap, UUID mapping, object identity, table layouts |
| [security/](security/) | 4 specs | Authentication, authorization, RLS, CLS |
| [ipc/](ipc/) | 4 specs | Wire protocol, sessions, parser agents, adapters |
| [indexes/](indexes/) | 3 specs | B-Tree, GIN, DML integration |
| [types/](types/) | 3 specs | Scalar types, complex types, coercion rules |

**Total: 33 specifications** - See [Full Index](index.md)

## External Reference Specifications

> **Note:** External specification documents exist in `~/CliWork/local_work/docs/specifications/` (31 comprehensive sections). These are **reference material only** for understanding the system design - they should not be linked from this documentation.
>
> When documenting implementation details here, **reverse-engineer from the actual code** and add verified source anchors to files within this project.

## Spec vs. Implementation Relationship

```
External Reference Specs  ──►  Reverse-Engineered Specs  ──►  Implementation
   (local_work/docs/)            (this directory)             (src/)
        │                              │                            │
        │  (read for context)          │  (document with anchors)   │
        │                              ▼                            ▼
        └──────────────────►  specifications/                  Verified
                                 (code-linked specs)            Source
                                                               Anchors
```

## Status Legend

| Status | Meaning |
|--------|---------|
| 🔴 Draft | Initial reverse-engineering, not yet fully validated |
| 🟡 Review | Pending review against implementation |
| 🟢 Approved | Validated against current implementation |
| ⚪ Stable | Interface frozen, backward compatibility guaranteed |
| 🚫 Deprecated | Spec obsolete, implementation changing |

## Current Status

All specifications are currently **🔴 Draft** status as of 2026-03-08.

## Adding New Specifications

1. Read the relevant external spec in `~/CliWork/local_work/docs/specifications/` for context
2. Examine the actual implementation in `src/`
3. Copy `TEMPLATE.md` to appropriate subdirectory
4. Fill in all sections with content **reverse-engineered from actual code**
5. Add source anchors pointing to actual implementation files in `src/`
6. Run documentation audit to verify anchors: `python3 docs/documentation/audit_documentation.py`
7. Update `index.md` with the new spec entry

## Important: No External Links

All source anchors in this documentation MUST point to files within the ScratchBird project:
- ✅ `/home/dcalford/CliWork/ScratchBird/src/...`
- ✅ `/home/dcalford/CliWork/ScratchBird/tests/...`
- ❌ `~/CliWork/local_work/...` (not allowed)

## Relationship to Other Docs

- **Architecture docs** (`../architecture/`) - High-level system overview
- **Language Reference** (`../../language_reference/`) - User-facing SQL docs
- **These specs** - Low-level implementation contracts with code anchors

## Audit Trail

All specifications are subject to the documentation integrity audit. Any spec with unverified source anchors will be flagged for correction.

- Last full audit: 2026-03-08
- Verification rate: 100%
- Total specifications: 33
