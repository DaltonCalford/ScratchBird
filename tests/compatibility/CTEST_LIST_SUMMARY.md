# Compatibility CTest List Summary

Generated: 2026-03-23T01:14:09Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260322_210909, 0.03s/test) | 0s | 19s | 59s |
| mysql | 4 | 2995 | 8842 | latest_run (20260322_210627, 31.77s/test) | 2m 7s | 26h 26m 4s | 78h 2m 28s |
| postgresql | 5 | 88 | 238 | latest_run (20260322_210524, 5.88s/test) | 29s | 8m 37s | 23m 18s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
