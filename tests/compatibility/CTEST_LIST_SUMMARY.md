# Compatibility CTest List Summary

Generated: 2026-03-24T05:39:56Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260324_013955, 0.02s/test) | 0s | 13s | 39s |
| mysql | 4 | 2995 | 8842 | latest_run (20260324_011206, 21.86s/test) | 1m 27s | 18h 11m 18s | 53h 41m 47s |
| postgresql | 5 | 88 | 238 | latest_run (20260324_011116, 4.32s/test) | 22s | 6m 20s | 17m 8s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
