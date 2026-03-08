# Compatibility CTest List Summary

Generated: 2026-03-08T04:33:47Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260307_232844, 0.39s/test) | 5s | 4m 39s | 14m 33s |
| mysql | 4 | 2995 | 8842 | latest_run (20260307_232735, 10.86s/test) | 43s | 9h 2m 8s | 26h 40m 32s |
| postgresql | 5 | 88 | 238 | latest_run (20260307_232647, 4.38s/test) | 22s | 6m 25s | 17m 21s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
