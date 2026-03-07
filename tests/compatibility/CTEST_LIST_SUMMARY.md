# Compatibility CTest List Summary

Generated: 2026-03-07T00:46:46Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260306_194116, 0.43s/test) | 5s | 5m 8s | 16m 4s |
| mysql | 4 | 2995 | 8842 | latest_run (20260306_193828, 9.82s/test) | 39s | 8h 10m 24s | 24h 7m 47s |
| postgresql | 5 | 88 | 238 | latest_run (20260306_193744, 3.79s/test) | 19s | 5m 33s | 15m 1s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
