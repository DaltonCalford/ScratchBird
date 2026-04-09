# Section 04 Test Contract

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-30

## Current status
This section now has an explicit replayed gate surface for the engine-core page-size contract.
B1-01-005 executed the invalid persisted-header refusal path, the tablespace
page-size mismatch refusal path, the invalid page-size creation matrix, and the
basic invalid-size predicate under one preserved gate bundle.

## Visible test anchor matrix
- Detailed evidence register:
  - [SECTION_04_PAGE_SIZE_TEST_ANCHOR_MATRIX.csv](../local_work/docs/planning/CODE_CAPABILITY_AUDIT_EXECUTION_WORKTREE/evidence/CCAW-001/SECTION_04_PAGE_SIZE_TEST_ANCHOR_MATRIX.csv)

| File | Line | Coverage note |
| --- | --- | --- |
| `tests/unit/test_ondisk_crc_uuid.cpp` | `56` | Accepts canonical sizes beginning with `8192`, `16384`, and `32768` |
| `tests/unit/test_ondisk_crc_uuid.cpp` | `61` | Accepts large sizes `65536` and `131072` |
| `tests/unit/test_ondisk_crc_uuid.cpp` | `65` | Rejects invalid sizes such as `0`, `4096`, and `262144` |
| `tests/unit/test_error_paths.cpp` | `33` | Boundary-oriented valid/invalid page-size contract check |
| `tests/unit/test_extended_page_sizes.cpp` | `48` | Iterates the full supported size set through create/open behavior |
| `tests/unit/test_extended_page_sizes.cpp` | `76` | Focuses on `65536` and `131072` large-page cases |
| `tests/unit/test_database_format_compatibility.cpp` | `92` | Validates persisted header page-size acceptance during format checks |
| `tests/unit/test_restore_validation_rehearsal.cpp` | `158` | Checks restored/source page-size agreement |
| `tests/unit/test_compression_interop.cpp` | `97` | Exercises compression behavior across all supported page sizes |

## Required behaviors that must remain covered
- creation failure on unsupported page size
- open failure on invalid persisted page size
- restore failure on invalid restored header page size
- restore failure on `block_size`/header page-size mismatch
- tablespace-open failure on page-size mismatch
- large-page offset helper correctness for `65536` and `131072`
- heap max-tuple-size and item-pointer behavior across supported sizes

## 2026-03-30 gate closure update

The explicit section `04` gate surface for this lane now includes:
- `tests/unit/test_storage_recovery_gate_contract.cpp` for invalid persisted
  header page-size refusal across all supported page sizes
- `tests/unit/test_storage_recovery_gate_contract.cpp` for direct tablespace
  page-size mismatch refusal
- `tests/unit/test_error_paths.cpp` for the invalid-size predicate boundary
- `tests/unit/test_extended_page_sizes.cpp` for invalid page-size creation
  refusal across a wider rejected-size set

## Non-blocking expansion candidates
- A machine-readable section `04` gate matrix
- A page-size coverage matrix per storage/index family
- Explicit pass/fail performance evidence if recommendation language about page-size tradeoffs is kept authoritative

## Suggestions
- Keep section `04` anchored to the executed explicit gate surface for engine-core
  behavior.
- Treat broader storage-family all-sizes coverage as follow-on depth, not as a
  blocker on this lane's implemented status.

## 2026-03-27 subsystem compatibility matrix addendum

The section `04` audit now includes a subsystem-by-subsystem compatibility baseline in `local_work/docs/planning/CODE_CAPABILITY_AUDIT_EXECUTION_WORKTREE/evidence/CCAW-001/SECTION_04_PAGE_SIZE_SUBSYSTEM_COMPATIBILITY_MATRIX.csv`.

The matrix separates three evidence classes:
- `code_backed_all_supported_sizes`
- `code_backed_shared_invariant_no_direct_family_test_seen`
- `partial_or_needs_more_family_specific_evidence`

The strongest remaining section `04` test gap is no longer size-set discovery. It is direct family-level exercise across the supported size set for index and secondary storage families that currently inherit the shared page-size contract but do not yet have family-specific all-sizes evidence in this audit wave.

## 2026-03-27 family coverage promotion update

The section `04` evidence model now distinguishes between:
- full supported-size exercise
- fixed-size family evidence
- large-page-only evidence
- no direct page-backed family proof in the current audit wave

Two companion artifacts now govern the section:
- `local_work/docs/planning/CODE_CAPABILITY_AUDIT_EXECUTION_WORKTREE/evidence/CCAW-001/SECTION_04_PAGE_SIZE_SUBSYSTEM_COMPATIBILITY_MATRIX.csv`
- `local_work/docs/planning/CODE_CAPABILITY_AUDIT_EXECUTION_WORKTREE/evidence/CCAW-001/SECTION_04_PAGE_SIZE_OPERATOR_COMPATIBILITY_TABLE.csv`

The strongest remaining section `04` gap is now precise: most storage and index families are only directly evidenced at one or two fixed page sizes, while only engine-core surfaces are fully exercised across the supported set.
