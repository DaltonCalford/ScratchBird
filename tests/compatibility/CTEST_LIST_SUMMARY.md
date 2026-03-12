# Compatibility CTest List Summary

Generated: 2026-03-11T23:13:21Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260311_191321, 0.02s/test) | 0s | 12s | 38s |
| mysql | 4 | 2995 | 8842 | latest_run (20260311_185609, 46.18s/test) | 3m 5s | 38h 25m 2s | 113h 25m 3s |
| postgresql | 5 | 88 | 238 | latest_run (20260311_185415, 17.37s/test) | 1m 27s | 25m 29s | 1h 8m 55s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
