# Compatibility CTest List Summary

Generated: 2026-03-18T05:38:54Z
Expanded ratio: `0.35`
Expanded max per suite: `400`

| Engine | Curated | Expanded | Full | Runtime Model | Est. Curated | Est. Expanded | Est. Full |
|--------|--------:|---------:|-----:|---------------|-------------:|--------------:|----------:|
| firebird | 12 | 721 | 2257 | latest_run (20260318_013854, 0.01s/test) | 0s | 10s | 32s |
| mysql | 4 | 2995 | 8842 | latest_run (20260318_012451, 94.59s/test) | 6m 18s | 78h 41m 48s | 232h 19m 58s |
| postgresql | 5 | 88 | 238 | latest_run (20260318_013110, 92.92s/test) | 7m 45s | 2h 16m 17s | 6h 8m 35s |

Notes:
- Runtime estimates are rough planning values, not hard guarantees.
- Models use latest engine run data when available; otherwise fallback constants.
- CTest list mode is selected in runners via `SCRATCHBIRD_COMPAT_CTEST_LIST_MODE` or per-engine override.
