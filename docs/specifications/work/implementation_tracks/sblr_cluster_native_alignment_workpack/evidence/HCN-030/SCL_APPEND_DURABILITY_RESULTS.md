# SCL Append Durability Results

Validated by `ShardCommitLogPipelineTest.AppendsAreDurableAndReadableAcrossInstances`:
- two sequential entries are appended.
- shard commit log file exists with ordered lines (`1`, `2`).
- a new `ShardCommitLog` instance reads and decodes both entries from disk.

Observed outcome:
- durable append behavior is functional for the tested pipeline path.
