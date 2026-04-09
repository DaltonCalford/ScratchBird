# Page Size Selection Compatibility and Operator Policy Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the product-facing page-size selection policy, compatibility rules, and operator guidance for supported page sizes.

## Supported sizes

Current admitted sizes remain:
- `8192`
- `16384`
- `32768`
- `65536`
- `131072`

## Selection rule

1. Page size is chosen at database creation time.
2. All attached secondary tablespaces must match the database page size.
3. Changing page size requires offline rebuild, restore, or equivalent explicit data-movement operation.
4. No implicit in-place page-size rewrite is guaranteed.

## Compatibility rule

A page size is supportable only when all required families agree:
- page header/layout
- heap layout
- admitted index family structures
- backup/restore path
- validation/checksum path

If any required family lacks support proof, that page size/family combination is refused.

## Operator guidance classes

| Workload class | Recommended starting point |
| --- | --- |
| general OLTP | `16384` unless measured evidence justifies otherwise |
| wide-row or larger analytical payloads | `32768` or higher when storage families admit it |
| very large-page use | bounded and evidence-driven only |

## Expansion rule

No page size beyond `131072` is admitted until:
- structure layouts are updated
- compatibility is published
- certification evidence exists
