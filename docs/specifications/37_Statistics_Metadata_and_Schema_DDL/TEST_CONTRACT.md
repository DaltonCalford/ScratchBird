# Section 37 Test Contract

Current directly evidenced package-02 proof:
- `ForensicReplaySessionTest.ReplayResolvesHistoricalSchemaAcrossCommittedDdl`
- `ForensicReplaySessionTest.SchemaChangeCatalogTracksMetadataOnlyAndValidatedPromotion`

Required evidence for future hardening:
- statistics presence/freshness evidence where claimed
- metadata visibility and invalidation evidence
- committed schema-epoch publication only on successful `DDL` commit
- rollback evidence showing abandoned `DDL` does not publish a committed schema
  epoch
- explicit online-schema-change classification evidence proving
  `METADATA_ONLY`, `EXPAND_BACKFILL_CUTOVER`, and `REWRITE_REQUIRED`
  operations are distinguished correctly
- durable plan, event, backfill-progress, and cutover-guard evidence for
  `EXPAND_BACKFILL_CUTOVER` paths
- restart or resume evidence proving backfill progress is not inferred only
  from memory
- explicit negative-path evidence for unsupported concurrent `DDL` or `DML`
  combinations where appropriate
- metadata-only visibility flips for `ALTER INDEX ... SET VISIBLE` and
  `ALTER INDEX ... SET INVISIBLE`
- online index build classification evidence for empty versus populated
  relations
- invisible or candidate index state evidence proving planner exclusion until
  promotion
- integrated cutover-guard evidence when schema DDL and online index work share
  one change boundary
