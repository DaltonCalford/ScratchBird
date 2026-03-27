# Compatibility CTest List Summary

Generated: 2026-03-27T02:38:41Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260326_223840, 0.02s/test) | 0s | 13s | 41s |
| mysql | 4 | 2995 | 8842 | latest_run (20260326_113231, 24.15s/test) | 1m 37s | 20h 5m 31s | 59h 18m 58s |
| postgresql | 5 | 88 | 238 | latest_run (20260326_113141, 4.31s/test) | 22s | 6m 20s | 17m 6s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
