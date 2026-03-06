# Compatibility CTest List Summary

Generated: 2026-03-06T22:18:08Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260306_123822, 3.40s/test) | 41s | 40m 53s | 2h 8m 0s |
| mysql | 4 | 2995 | 8842 | latest_run (20260306_123508, 0.05s/test) | 0s | 2m 39s | 7m 51s |
| postgresql | 5 | 88 | 238 | latest_run (20260306_110723, 3.72s/test) | 19s | 5m 27s | 14m 45s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
