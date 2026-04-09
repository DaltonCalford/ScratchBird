# Guard Audit Log

Validated guard outcomes:
- hard reject when cross-shard writes are prohibited.
- reject with explicit override reason when override is required but absent.
- allow when override is present under override-required policy.
