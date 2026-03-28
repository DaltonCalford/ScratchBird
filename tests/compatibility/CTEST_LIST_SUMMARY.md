# Compatibility CTest List Summary

Generated: 2026-03-28T07:10:04Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260328_030305, 0.02s/test) | 0s | 13s | 40s |
| mysql | 4 | 2995 | 8842 | latest_run (20260328_025240, 147.82s/test) | 9m 51s | 122h 58m 50s | 363h 4m 10s |
| postgresql | 5 | 88 | 238 | latest_run (20260328_025105, 12.38s/test) | 1m 2s | 18m 9s | 49m 6s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
