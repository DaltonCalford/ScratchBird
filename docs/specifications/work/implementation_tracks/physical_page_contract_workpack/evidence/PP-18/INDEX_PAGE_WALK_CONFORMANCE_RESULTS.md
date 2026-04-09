# Index Page Walk Conformance Results

## Cases
1. All-valid walk (`3` pages):
- Expected: `Status::OK`
- Observed: `Status::OK`, `pages_ok=3`, `pages_failed=0`

2. Mixed walk (`3` pages, `1` checksum-corrupt):
- Expected: first failure `Status::CHECKSUM_MISMATCH`, full entry list retained
- Observed: `Status::CHECKSUM_MISMATCH`, `pages_ok=2`, `pages_failed=1`, per-page statuses captured in report entries

## Test Source
- `tests/unit/test_index_page_walk_conformance.cpp`
