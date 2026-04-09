# Compatibility CTest List Summary

Generated: 2026-04-05T03:36:12Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260404_233329, 0.03s/test) | 0s | 19s | 58s |
| mysql | 3 | 2994 | 8842 | latest_run (20260404_233306, 1.22s/test) | 4s | 1h 0m 46s | 2h 59m 27s |
| postgresql | 5 | 88 | 238 | latest_run (20260404_233310, 3.87s/test) | 19s | 5m 41s | 15m 21s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
