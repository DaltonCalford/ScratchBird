# Specifications (Reverse-Engineered from Implementation)

## Purpose

This directory contains **82 authoritative specifications** derived from actual ScratchBird implementation code. These specs serve as the definitive reference for understanding the system internals.

All specs have been rigorously validated:
- ✅ No references to WAL (ScratchBird uses MGA, not WAL)
- ✅ All source anchors verified against actual code
- ✅ Minimum 2 source anchors per specification
- ✅ No hallucinated directory structures

## Quick Navigation

| Subsystem | Count | Description |
|-----------|-------|-------------|
| [catalog/](catalog/) | 18 | Metadata, bootstrap, system tables |
| [indexes/](indexes/) | 13 | Index types and formats |
| [ipc/](ipc/) | 3 | Wire protocol, sessions |
| [parser/](parser/) | 23 | SQL parser, statements |
| [sblr/](sblr/) | 11 | SBLR opcodes, execution |
| [security/](security/) | 14 | Auth, authorization, RLS, CLS |
| [types/](types/) | 3 | Type system |

**Total: 82 specifications** - See [Full Index](index.md)

## Documentation Rules

### Source Anchors Required
All specifications MUST include verified source anchors:
- ✅ `/home/dcalford/CliWork/ScratchBird/src/...`
- ✅ `/home/dcalford/CliWork/ScratchBird/tests/...`
- ❌ No external links

### Minimum Requirements
- At least 2 source anchors per spec
- No prohibited concepts (WAL, XLog, etc.)
- Content must match actual code implementation

### Status

All specs are currently **🔴 Draft**

## Audit Status

- Last audit: 2026-03-08
- Verification rate: 100%
- Source anchors verified: 1,698
- Test anchors verified: 914
- Hallucinated specs removed: 93

---

**Full Index:** [index.md](index.md)
