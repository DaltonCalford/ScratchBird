# Implementation Notes

- Tightened `cleanToastChunksByTIP` to use explicit TIP-state outcomes for TOAST chunk `xmax`.
- Added chunk-marker normalization (`xmax=0`, clear delete flags) for aborted and out-of-range delete transactions.
- Added `ToastGCContractTest` coverage for committed-delete purge and aborted-delete marker cleanup.
