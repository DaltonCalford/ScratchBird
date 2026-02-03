# MySQL performance_schema Column Notes

**Last Updated:** 2026-02-03

---

ScratchBird must emulate the full MySQL performance_schema catalog.
Tables below list the expected MySQL 8.0 column layout. For each column, the status is one of:

- ScratchBird tracked: populated from a ScratchBird runtime source.
- Always NULL: column exists but is never populated in ScratchBird.
- Always 0: column is always returned as 0 in ScratchBird.

If a table is not yet implemented, it should still be exposed as a schema-only view
with columns present and values returned as NULL/0. Those columns are marked Always NULL.

## performance_schema.accounts

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_CONNECTIONS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TOTAL_CONNECTIONS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_SESSION_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_SESSION_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.cond_instances

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.data_lock_waits

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `ENGINE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUESTING_ENGINE_LOCK_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUESTING_ENGINE_TRANSACTION_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUESTING_THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUESTING_OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `BLOCKING_ENGINE_LOCK_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `BLOCKING_ENGINE_TRANSACTION_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `BLOCKING_THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `BLOCKING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `BLOCKING_OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.data_locks

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `ENGINE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENGINE_LOCK_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENGINE_TRANSACTION_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PARTITION_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUBPARTITION_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCK_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCK_MODE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCK_STATUS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCK_DATA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.error_log

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `LOGGED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PRIO` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_CODE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUBSYSTEM` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DATA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_errors_summary_by_account_by_error

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_RAISED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_HANDLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_errors_summary_by_host_by_error

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_RAISED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_HANDLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_errors_summary_by_thread_by_error

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_RAISED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_HANDLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_errors_summary_by_user_by_error

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_RAISED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_HANDLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_errors_summary_global_by_error

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERROR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_RAISED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERROR_HANDLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_current

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `END_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_START` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_END` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORK_COMPLETED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORK_ESTIMATED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_history

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `END_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_START` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_END` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORK_COMPLETED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORK_ESTIMATED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_history_long

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `END_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_START` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_END` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORK_COMPLETED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORK_ESTIMATED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_summary_by_account_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_summary_by_host_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_summary_by_thread_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_summary_by_user_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_stages_summary_global_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_current

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | ProcArrayManager::ProcessControlBlock.proc_id | Row per backend (history_long includes idle backends). |
| `EVENT_ID` | ScratchBird tracked | ProcessControlBlock.query_start_time or proc_id | Populated when backend snapshot exists. |
| `END_EVENT_ID` | Always NULL | Not populated | Never. |
| `EVENT_NAME` | ScratchBird tracked | Constant "statement/sql/exec" | Always for emitted rows. |
| `SOURCE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `TIMER_START` | ScratchBird tracked | query_start_time/state_change_time/start_time | Populated when time source is available; NULL if all are zero. |
| `TIMER_END` | ScratchBird tracked | Current time (microseconds) | Always for emitted rows. |
| `TIMER_WAIT` | ScratchBird tracked | Current time minus TIMER_START | Populated when TIMER_START is available; 0 if missing. |
| `LOCK_TIME` | Always NULL | Not populated | Never. |
| `SQL_TEXT` | ScratchBird tracked | ProcessControlBlock.query_text | Populated when query text is non-empty. |
| `DIGEST` | Always NULL | Digest not populated in live snapshot | Never. |
| `DIGEST_TEXT` | Always NULL | Digest not populated in live snapshot | Never. |
| `CURRENT_SCHEMA` | ScratchBird tracked | Constant "scratchbird" | Always for emitted rows. |
| `OBJECT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_SCHEMA` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_NAME` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MYSQL_ERRNO` | Always NULL | Not populated | Never. |
| `RETURNED_SQLSTATE` | Always NULL | Not populated | Never. |
| `MESSAGE_TEXT` | Always NULL | Not populated | Never. |
| `ERRORS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `WARNINGS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `ROWS_AFFECTED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `ROWS_SENT` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `ROWS_EXAMINED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CREATED_TMP_DISK_TABLES` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CREATED_TMP_TABLES` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_FULL_JOIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_FULL_RANGE_JOIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_RANGE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_RANGE_CHECK` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_SCAN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_MERGE_PASSES` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_RANGE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_ROWS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_SCAN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NO_INDEX_USED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NO_GOOD_INDEX_USED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_LEVEL` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `STATEMENT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CPU_TIME` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `EXECUTION_ENGINE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |

## performance_schema.events_statements_histogram_by_digest

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SCHEMA_NAME` | ScratchBird tracked | CatalogManager::StatementDigestEntry.schema_name | Populated when schema is known. |
| `DIGEST` | ScratchBird tracked | CatalogManager::StatementDigestEntry.digest | Always for digest rows. |
| `BUCKET_NUMBER` | ScratchBird tracked | Histogram bucket index | One row per bucket for each digest. |
| `BUCKET_TIMER_LOW` | ScratchBird tracked | CatalogManager::digestHistogramLowerBound(bucket) | One row per bucket for each digest. |
| `BUCKET_TIMER_HIGH` | ScratchBird tracked | CatalogManager::digestHistogramUpperBound(bucket) | One row per bucket for each digest. |
| `COUNT_BUCKET` | ScratchBird tracked | CatalogManager::StatementDigestEntry.histogram_counts[bucket] | One row per bucket for each digest. |
| `COUNT_BUCKET_AND_LOWER` | ScratchBird tracked | Cumulative sum of histogram_counts | One row per bucket for each digest. |
| `BUCKET_QUANTILE` | ScratchBird tracked | Cumulative histogram count / total | One row per bucket for each digest. |

## performance_schema.events_statements_histogram_global

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `BUCKET_NUMBER` | ScratchBird tracked | Histogram bucket index | One row per bucket. |
| `BUCKET_TIMER_LOW` | ScratchBird tracked | CatalogManager::digestHistogramLowerBound(bucket) | One row per bucket. |
| `BUCKET_TIMER_HIGH` | ScratchBird tracked | CatalogManager::digestHistogramUpperBound(bucket) | One row per bucket. |
| `COUNT_BUCKET` | ScratchBird tracked | CatalogManager::getStatementDigestHistogramGlobal | One row per bucket. |
| `COUNT_BUCKET_AND_LOWER` | ScratchBird tracked | Cumulative global histogram count | One row per bucket. |
| `BUCKET_QUANTILE` | ScratchBird tracked | Cumulative global histogram count / total | One row per bucket. |

## performance_schema.events_statements_history

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `END_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_START` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_END` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_TEXT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DIGEST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DIGEST_TEXT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MYSQL_ERRNO` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `RETURNED_SQLSTATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MESSAGE_TEXT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_LEVEL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STATEMENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EXECUTION_ENGINE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_history_long

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | ProcArrayManager::ProcessControlBlock.proc_id | Row per backend (history_long includes idle backends). |
| `EVENT_ID` | ScratchBird tracked | ProcessControlBlock.query_start_time or proc_id | Populated when backend snapshot exists. |
| `END_EVENT_ID` | Always NULL | Not populated | Never. |
| `EVENT_NAME` | ScratchBird tracked | Constant "statement/sql/exec" | Always for emitted rows. |
| `SOURCE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `TIMER_START` | ScratchBird tracked | query_start_time/state_change_time/start_time | Populated when time source is available; NULL if all are zero. |
| `TIMER_END` | ScratchBird tracked | Current time (microseconds) | Always for emitted rows. |
| `TIMER_WAIT` | ScratchBird tracked | Current time minus TIMER_START | Populated when TIMER_START is available; 0 if missing. |
| `LOCK_TIME` | Always NULL | Not populated | Never. |
| `SQL_TEXT` | ScratchBird tracked | ProcessControlBlock.query_text | Populated when query text is non-empty. |
| `DIGEST` | Always NULL | Digest not populated in live snapshot | Never. |
| `DIGEST_TEXT` | Always NULL | Digest not populated in live snapshot | Never. |
| `CURRENT_SCHEMA` | ScratchBird tracked | Constant "scratchbird" | Always for emitted rows. |
| `OBJECT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_SCHEMA` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_NAME` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MYSQL_ERRNO` | Always NULL | Not populated | Never. |
| `RETURNED_SQLSTATE` | Always NULL | Not populated | Never. |
| `MESSAGE_TEXT` | Always NULL | Not populated | Never. |
| `ERRORS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `WARNINGS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `ROWS_AFFECTED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `ROWS_SENT` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `ROWS_EXAMINED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CREATED_TMP_DISK_TABLES` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CREATED_TMP_TABLES` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_FULL_JOIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_FULL_RANGE_JOIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_RANGE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_RANGE_CHECK` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SELECT_SCAN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_MERGE_PASSES` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_RANGE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_ROWS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SORT_SCAN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NO_INDEX_USED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NO_GOOD_INDEX_USED` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_LEVEL` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `STATEMENT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CPU_TIME` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `EXECUTION_ENGINE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |

## performance_schema.events_statements_summary_by_account_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_summary_by_digest

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SCHEMA_NAME` | ScratchBird tracked | CatalogManager::StatementDigestEntry.schema_name | Populated when schema is known. |
| `DIGEST` | ScratchBird tracked | CatalogManager::StatementDigestEntry.digest | Always for digest rows. |
| `DIGEST_TEXT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.digest_text | Populated when digest text is stored. |
| `COUNT_STAR` | ScratchBird tracked | CatalogManager::StatementDigestEntry.count_star | Populated when statement digest tracking has observed statements. |
| `SUM_TIMER_WAIT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_timer_wait | Populated when statement digest tracking has observed statements. |
| `MIN_TIMER_WAIT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.min_timer_wait | Populated when statement digest tracking has observed statements. |
| `AVG_TIMER_WAIT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.avg_timer_wait (computed from sum_timer_wait / count_star) | Populated when statement digest tracking has observed statements. |
| `MAX_TIMER_WAIT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.max_timer_wait | Populated when statement digest tracking has observed statements. |
| `SUM_LOCK_TIME` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_lock_time | Populated when statement digest tracking has observed statements. |
| `SUM_ERRORS` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_errors | Populated when statement digest tracking has observed statements. |
| `SUM_WARNINGS` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_warnings | Populated when statement digest tracking has observed statements. |
| `SUM_ROWS_AFFECTED` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_rows_affected | Populated when statement digest tracking has observed statements. |
| `SUM_ROWS_SENT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_rows_sent | Populated when statement digest tracking has observed statements. |
| `SUM_ROWS_EXAMINED` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_rows_examined | Populated when statement digest tracking has observed statements. |
| `SUM_CREATED_TMP_DISK_TABLES` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_created_tmp_disk_tables | Populated when statement digest tracking has observed statements. |
| `SUM_CREATED_TMP_TABLES` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_created_tmp_tables | Populated when statement digest tracking has observed statements. |
| `SUM_SELECT_FULL_JOIN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_select_full_join | Populated when statement digest tracking has observed statements. |
| `SUM_SELECT_FULL_RANGE_JOIN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_select_full_range_join | Populated when statement digest tracking has observed statements. |
| `SUM_SELECT_RANGE` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_select_range | Populated when statement digest tracking has observed statements. |
| `SUM_SELECT_RANGE_CHECK` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_select_range_check | Populated when statement digest tracking has observed statements. |
| `SUM_SELECT_SCAN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_select_scan | Populated when statement digest tracking has observed statements. |
| `SUM_SORT_MERGE_PASSES` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_sort_merge_passes | Populated when statement digest tracking has observed statements. |
| `SUM_SORT_RANGE` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_sort_range | Populated when statement digest tracking has observed statements. |
| `SUM_SORT_ROWS` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_sort_rows | Populated when statement digest tracking has observed statements. |
| `SUM_SORT_SCAN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_sort_scan | Populated when statement digest tracking has observed statements. |
| `SUM_NO_INDEX_USED` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_no_index_used | Populated when statement digest tracking has observed statements. |
| `SUM_NO_GOOD_INDEX_USED` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_no_good_index_used | Populated when statement digest tracking has observed statements. |
| `SUM_CPU_TIME` | ScratchBird tracked | CatalogManager::StatementDigestEntry.sum_cpu_time | Populated when statement digest tracking has observed statements. |
| `MAX_CONTROLLED_MEMORY` | ScratchBird tracked | CatalogManager::StatementDigestEntry.max_controlled_memory | Populated when statement digest tracking has observed statements. |
| `MAX_TOTAL_MEMORY` | ScratchBird tracked | CatalogManager::StatementDigestEntry.max_total_memory | Populated when statement digest tracking has observed statements. |
| `COUNT_SECONDARY` | ScratchBird tracked | CatalogManager::StatementDigestEntry.count_secondary | Populated when statement digest tracking has observed statements. |
| `FIRST_SEEN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.first_seen | Populated when statement digest tracking has observed statements. |
| `LAST_SEEN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.last_seen | Populated when statement digest tracking has observed statements. |
| `QUANTILE_95` | ScratchBird tracked | CatalogManager::StatementDigestEntry.digestQuantileValue(histogram_counts, 0.95) | Populated when statement digest tracking has observed statements. |
| `QUANTILE_99` | ScratchBird tracked | CatalogManager::StatementDigestEntry.digestQuantileValue(histogram_counts, 0.99) | Populated when statement digest tracking has observed statements. |
| `QUANTILE_999` | ScratchBird tracked | CatalogManager::StatementDigestEntry.digestQuantileValue(histogram_counts, 0.999) | Populated when statement digest tracking has observed statements. |
| `QUERY_SAMPLE_TEXT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.query_sample_text | Populated when statement digest tracking has observed statements. |
| `QUERY_SAMPLE_SEEN` | ScratchBird tracked | CatalogManager::StatementDigestEntry.query_sample_seen | Populated when statement digest tracking has observed statements. |
| `QUERY_SAMPLE_TIMER_WAIT` | ScratchBird tracked | CatalogManager::StatementDigestEntry.query_sample_timer_wait | Populated when statement digest tracking has observed statements. |

## performance_schema.events_statements_summary_by_host_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_summary_by_program

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STATEMENTS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_STATEMENTS_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_STATEMENTS_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_STATEMENTS_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_STATEMENTS_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_summary_by_thread_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_summary_by_user_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_statements_summary_global_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_transactions_current

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | ProcessControlBlock.proc_id | Rows for sessions with active transaction (xid != 0). |
| `EVENT_ID` | ScratchBird tracked | xact_start_time or proc_id | Rows for sessions with active transaction (xid != 0). |
| `END_EVENT_ID` | Always NULL | Not populated | Never. |
| `EVENT_NAME` | ScratchBird tracked | Constant "transaction" | Rows for sessions with active transaction. |
| `STATE` | ScratchBird tracked | Constant "ACTIVE" | Rows for sessions with active transaction. |
| `TRX_ID` | ScratchBird tracked | ProcessControlBlock.xid | Rows for sessions with active transaction. |
| `GTID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XID_FORMAT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XID_GTRID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XID_BQUAL` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XA_STATE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SOURCE` | ScratchBird tracked | Constant "scratchbird" | Rows for sessions with active transaction. |
| `TIMER_START` | ScratchBird tracked | xact_start_time or start_time | Rows for sessions with active transaction. |
| `TIMER_END` | ScratchBird tracked | Current time (microseconds) | Rows for sessions with active transaction. |
| `TIMER_WAIT` | ScratchBird tracked | Current time minus TIMER_START | Rows for sessions with active transaction. |
| `ACCESS_MODE` | ScratchBird tracked | ProcessControlBlock.is_read_only | Rows for sessions with active transaction. |
| `ISOLATION_LEVEL` | ScratchBird tracked | ProcessControlBlock.isolation_level mapped via mysqlIsolationLevelName | Rows for sessions with active transaction. |
| `AUTOCOMMIT` | ScratchBird tracked | Constant "YES" | Rows for sessions with active transaction. |
| `NUMBER_OF_SAVEPOINTS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NUMBER_OF_ROLLBACK_TO_SAVEPOINT` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NUMBER_OF_RELEASE_SAVEPOINT` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |

## performance_schema.events_transactions_history

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `END_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TRX_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `GTID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `XID_FORMAT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `XID_GTRID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `XID_BQUAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `XA_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_START` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_END` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ACCESS_MODE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ISOLATION_LEVEL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AUTOCOMMIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NUMBER_OF_SAVEPOINTS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NUMBER_OF_ROLLBACK_TO_SAVEPOINT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NUMBER_OF_RELEASE_SAVEPOINT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_transactions_history_long

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | CatalogManager::TransactionHistoryEntry.thread_id | Rows for completed transactions in history. |
| `EVENT_ID` | ScratchBird tracked | CatalogManager::TransactionHistoryEntry.event_id | Rows for completed transactions in history. |
| `END_EVENT_ID` | ScratchBird tracked | CatalogManager::TransactionHistoryEntry.end_event_id | Populated when end_event_id is non-zero; NULL otherwise. |
| `EVENT_NAME` | ScratchBird tracked | Constant "transaction" | Rows for completed transactions in history. |
| `STATE` | ScratchBird tracked | TransactionHistoryEntry.committed | COMMITTED or ROLLED BACK. |
| `TRX_ID` | ScratchBird tracked | TransactionHistoryEntry.trx_id | Populated when trx_id is non-zero; NULL otherwise. |
| `GTID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XID_FORMAT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XID_GTRID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XID_BQUAL` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `XA_STATE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `SOURCE` | ScratchBird tracked | Constant "scratchbird" | Rows for completed transactions in history. |
| `TIMER_START` | ScratchBird tracked | TransactionHistoryEntry.timer_start | Populated when non-zero; NULL otherwise. |
| `TIMER_END` | ScratchBird tracked | TransactionHistoryEntry.timer_end | Populated when non-zero; NULL otherwise. |
| `TIMER_WAIT` | ScratchBird tracked | TransactionHistoryEntry.timer_wait | Populated when non-zero; NULL otherwise. |
| `ACCESS_MODE` | ScratchBird tracked | TransactionHistoryEntry.read_only | READ ONLY or READ WRITE. |
| `ISOLATION_LEVEL` | ScratchBird tracked | TransactionHistoryEntry.isolation_level mapped via mysqlIsolationLevelName | Rows for completed transactions in history. |
| `AUTOCOMMIT` | ScratchBird tracked | TransactionHistoryEntry.autocommit | YES or NO. |
| `NUMBER_OF_SAVEPOINTS` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NUMBER_OF_ROLLBACK_TO_SAVEPOINT` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NUMBER_OF_RELEASE_SAVEPOINT` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_ID` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |

## performance_schema.events_transactions_summary_by_account_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_transactions_summary_by_host_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_transactions_summary_by_thread_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_transactions_summary_by_user_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_transactions_summary_global_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_current

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | LockSnapshot.proc_id | Rows for locks that are waiting (not granted). |
| `EVENT_ID` | ScratchBird tracked | LockSnapshot.request_time or proc_id | Rows for locks that are waiting. |
| `END_EVENT_ID` | Always NULL | Not populated | Never. |
| `EVENT_NAME` | ScratchBird tracked | Constant "wait/lock/metadata" | Rows for locks that are waiting. |
| `SOURCE` | ScratchBird tracked | Constant "scratchbird" | Rows for locks that are waiting. |
| `TIMER_START` | ScratchBird tracked | LockSnapshot.request_time | Rows for locks that are waiting. |
| `TIMER_END` | ScratchBird tracked | Current time (microseconds) | Rows for locks that are waiting. |
| `TIMER_WAIT` | ScratchBird tracked | Current time minus TIMER_START | Rows for locks that are waiting. |
| `SPINS` | Always NULL | Not populated | Never. |
| `OBJECT_SCHEMA` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_NAME` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_INSTANCE_BEGIN` | ScratchBird tracked | LockSnapshot.request_time | Rows for locks that are waiting. |
| `NESTING_EVENT_ID` | Always NULL | Not populated | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Not populated | Never. |
| `OPERATION` | Always NULL | Not populated | Never. |
| `NUMBER_OF_BYTES` | Always NULL | Not populated | Never. |
| `FLAGS` | Always NULL | Not populated | Never. |

## performance_schema.events_waits_history

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `END_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_START` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_END` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SPINS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OPERATION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NUMBER_OF_BYTES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FLAGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_history_long

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.thread_id | Rows for recorded waits in history. |
| `EVENT_ID` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.event_id | Rows for recorded waits in history. |
| `END_EVENT_ID` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.timer_end | Populated when timer_end is non-zero; NULL otherwise. |
| `EVENT_NAME` | ScratchBird tracked | Constant "wait/lock/metadata" | Rows for recorded waits in history. |
| `SOURCE` | ScratchBird tracked | Constant "scratchbird" | Rows for recorded waits in history. |
| `TIMER_START` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.timer_start | Populated when non-zero; NULL otherwise. |
| `TIMER_END` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.timer_end | Populated when non-zero; NULL otherwise. |
| `TIMER_WAIT` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.timer_wait | Populated when non-zero; NULL otherwise. |
| `SPINS` | Always NULL | Not populated | Never. |
| `OBJECT_SCHEMA` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_NAME` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_TYPE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `OBJECT_INSTANCE_BEGIN` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.object_instance_begin | Populated when non-zero; NULL otherwise. |
| `NESTING_EVENT_ID` | Always NULL | Not populated | Never. |
| `NESTING_EVENT_TYPE` | Always NULL | Not populated | Never. |
| `OPERATION` | Always NULL | Not populated | Never. |
| `NUMBER_OF_BYTES` | Always NULL | Not populated | Never. |
| `FLAGS` | ScratchBird tracked | CatalogManager::WaitHistoryEntry.timed_out | Populated as "TIMEOUT" when timed_out is true; NULL otherwise. |

## performance_schema.events_waits_summary_by_account_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_summary_by_host_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_summary_by_instance

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_summary_by_thread_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_summary_by_user_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.events_waits_summary_global_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.file_instances

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `FILE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OPEN_COUNT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.file_summary_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.file_summary_by_instance

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `FILE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.global_status

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.global_variables

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.host_cache

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `IP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST_VALIDATED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CONNECT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_HOST_BLOCKED_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_NAMEINFO_TRANSIENT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_NAMEINFO_PERMANENT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FORMAT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ADDRINFO_TRANSIENT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ADDRINFO_PERMANENT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FCRDNS_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_HOST_ACL_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_NO_AUTH_PLUGIN_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_AUTH_PLUGIN_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_HANDSHAKE_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_PROXY_USER_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_PROXY_USER_ACL_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_AUTHENTICATION_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SSL_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_MAX_USER_CONNECTIONS_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_MAX_USER_CONNECTIONS_PER_HOUR_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_DEFAULT_DATABASE_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_INIT_CONNECT_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_LOCAL_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_UNKNOWN_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FIRST_ERROR_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_SEEN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.hosts

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_CONNECTIONS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TOTAL_CONNECTIONS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_SESSION_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_SESSION_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.keyring_component_status

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `STATUS_KEY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STATUS_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.keyring_keys

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `BACKEND_KEY_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.log_status

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `SERVER_UUID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REPLICATION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STORAGE_ENGINES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.memory_summary_by_account_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.memory_summary_by_host_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.memory_summary_by_thread_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.memory_summary_by_user_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.memory_summary_global_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_ALLOC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_FREE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_COUNT_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOW_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HIGH_NUMBER_OF_BYTES_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.metadata_locks

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | ScratchBird tracked | mysqlMetadataObjectType(LockSnapshot.tag.target_type) | Row per current lock. |
| `OBJECT_SCHEMA` | ScratchBird tracked | CatalogManager::getSchema for object_uuid | Populated when schema lookup succeeds; NULL otherwise. |
| `OBJECT_NAME` | ScratchBird tracked | CatalogManager::getTable for object_uuid | Populated when table lookup succeeds; NULL otherwise. |
| `COLUMN_NAME` | Always NULL | Not populated | Never. |
| `OBJECT_INSTANCE_BEGIN` | ScratchBird tracked | LockSnapshot.request_time | Populated when request_time is non-zero; NULL otherwise. |
| `LOCK_TYPE` | ScratchBird tracked | mysqlLockType(LockSnapshot.mode) | Row per current lock. |
| `LOCK_DURATION` | ScratchBird tracked | Constant "TRANSACTION" | Row per current lock. |
| `LOCK_STATUS` | ScratchBird tracked | LockSnapshot.granted | GRANTED or PENDING. |
| `SOURCE` | ScratchBird tracked | Constant "scratchbird" | Row per current lock. |
| `OWNER_THREAD_ID` | ScratchBird tracked | LockSnapshot.proc_id | Populated when proc_id is non-zero; NULL otherwise. |
| `OWNER_EVENT_ID` | ScratchBird tracked | Derived from ProcArrayManager::ProcessControlBlock.query_start_time | Populated when backend event is found; NULL otherwise. |

## performance_schema.mutex_instances

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOCKED_BY_THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.objects_summary_global_by_type

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.performance_timers

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `TIMER_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_FREQUENCY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_RESOLUTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_OVERHEAD` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.persisted_variables

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.prepared_statements_instances

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STATEMENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STATEMENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SQL_TEXT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EXECUTION_ENGINE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMER_PREPARE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_REPREPARE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_EXECUTE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_EXECUTE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_EXECUTE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_EXECUTE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_EXECUTE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_LOCK_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ERRORS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_WARNINGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_AFFECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_SENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_ROWS_EXAMINED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_DISK_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CREATED_TMP_TABLES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_FULL_RANGE_JOIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_RANGE_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SELECT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_MERGE_PASSES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_RANGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_ROWS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_SORT_SCAN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NO_GOOD_INDEX_USED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_CPU_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_SECONDARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.processlist

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `ID` | ScratchBird tracked | ProcArrayManager::ProcessControlBlock.proc_id | Row per active backend. |
| `USER` | ScratchBird tracked | CatalogManager::SessionInfo.username via listSessions | When session metadata is available; NULL if session not found. |
| `HOST` | ScratchBird tracked | Constant "local" | Always. |
| `DB` | ScratchBird tracked | Constant "scratchbird" | Always. |
| `COMMAND` | ScratchBird tracked | Derived from ProcessControlBlock.query_start_time | Query when executing, Sleep otherwise. |
| `TIME` | ScratchBird tracked | Elapsed seconds from state_change_time/query_start_time | Always. |
| `STATE` | ScratchBird tracked | Constant "executing" or NULL | Populated only while a query is active. |
| `INFO` | ScratchBird tracked | ProcessControlBlock.query_text | Populated only while a query is active and text is non-empty. |
| `EXECUTION_ENGINE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |

## performance_schema.replication_applier_configuration

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DESIRED_DELAY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PRIVILEGE_CHECKS_USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUIRE_ROW_FORMAT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REQUIRE_TABLE_PRIMARY_KEY_CHECK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_applier_filters

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FILTER_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FILTER_RULE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CONFIGURED_BY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STARTUP_OPTIONS_FOR_CHANNEL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CHANGE_REPLICATION_FILTER_FOR_CHANNEL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ACTIVE_SINCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNTER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_applier_global_filters

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `FILTER_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FILTER_RULE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CONFIGURED_BY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CHANGE_REPLICATION_FILTER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ACTIVE_SINCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_applier_status

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SERVICE_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `REMAINING_DELAY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_RETRIES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_applier_status_by_coordinator

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SERVICE_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_MESSAGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_PROCESSED_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_PROCESSED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `not` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_PROCESSED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_PROCESSED_TRANSACTION_START_BUFFER_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_PROCESSED_TRANSACTION_END_BUFFER_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PROCESSING_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PROCESSING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PROCESSING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PROCESSING_TRANSACTION_START_BUFFER_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_applier_status_by_worker

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WORKER_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SERVICE_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_MESSAGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `not` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_START_APPLY_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_RETRIES_COUNT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_RETRIES_COUNT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_asynchronous_connection_failover

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PORT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NETWORK_NAMESPACE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WEIGHT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MANAGED_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_connection_configuration

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PORT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NETWORK_INTERFACE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AUTO_POSITION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_ALLOWED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_CA_FILE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_CA_PATH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_CERTIFICATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_CIPHER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_KEY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_VERIFY_SERVER_CERTIFICATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_CRL_FILE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SSL_CRL_PATH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CONNECTION_RETRY_INTERVAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CONNECTION_RETRY_COUNT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HEARTBEAT_INTERVAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COMMENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TLS_VERSION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PUBLIC_KEY_PATH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `GET_PUBLIC_KEY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `NETWORK_NAMESPACE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COMPRESSION_ALGORITHM` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ZSTD_COMPRESSION_LEVEL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TLS_CIPHERSUITES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE_CONNECTION_AUTO_FAILOVER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `GTID_ONLY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_connection_status

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `GROUP_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOURCE_UUID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SERVICE_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_RECEIVED_HEARTBEATS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_HEARTBEAT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COMMENT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `RECEIVED_TRANSACTION_SET` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_NUMBER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_MESSAGE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_ERROR_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_QUEUED_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_QUEUED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `not` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_QUEUED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_QUEUED_TRANSACTION_START_QUEUE_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_QUEUED_TRANSACTION_END_QUEUE_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `QUEUEING_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `QUEUEING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `QUEUEING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `QUEUEING_TRANSACTION_START_QUEUE_TIMESTAMP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_group_member_stats

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VIEW_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_IN_QUEUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_CHECKED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_CONFLICTS_DETECTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_ROWS_VALIDATING` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TRANSACTIONS_COMMITTED_ALL_MEMBERS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LAST_CONFLICT_FREE_TRANSACTION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_REMOTE_IN_APPLIER_QUEUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_REMOTE_APPLIED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_LOCAL_PROPOSED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_TRANSACTIONS_LOCAL_ROLLBACK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.replication_group_members

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `CHANNEL_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_PORT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_ROLE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_VERSION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MEMBER_COMMUNICATION_STACK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.rwlock_instances

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `WRITE_LOCKED_BY_THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `READ_LOCKED_BY_COUNT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.session_account_connect_attrs

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `PROCESSLIST_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ATTR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ATTR_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ORDINAL_POSITION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.session_connect_attrs

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `PROCESSLIST_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ATTR_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ATTR_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ORDINAL_POSITION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.session_status

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.session_variables

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.setup_actors

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ROLE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENABLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HISTORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.setup_consumers

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENABLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.setup_instruments

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENABLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PROPERTIES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `FLAGS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VOLATILITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DOCUMENTATION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.setup_objects

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENABLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TIMED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.setup_threads

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `ENABLED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HISTORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PROPERTIES` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VOLATILITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DOCUMENTATION` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.socket_instances

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SOCKET_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `IP` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PORT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `STATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.socket_summary_by_event_name

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.socket_summary_by_instance

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `EVENT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_NUMBER_OF_BYTES_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_MISC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.status_by_account

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.status_by_host

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.status_by_thread

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.status_by_user

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.table_handles

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_INSTANCE_BEGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OWNER_EVENT_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `INTERNAL_LOCK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `EXTERNAL_LOCK` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.table_io_waits_summary_by_index_usage

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.table_io_waits_summary_by_table

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_FETCH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_UPDATE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_DELETE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.table_lock_waits_summary_by_table

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `OBJECT_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_SCHEMA` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `OBJECT_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_STAR` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WAIT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_WITH_SHARED_LOCKS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_WITH_SHARED_LOCKS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_WITH_SHARED_LOCKS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_WITH_SHARED_LOCKS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_WITH_SHARED_LOCKS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_HIGH_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_HIGH_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_HIGH_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_HIGH_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_HIGH_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_NO_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_NO_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_NO_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_NO_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_NO_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_READ_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_READ_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_READ_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_READ_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_READ_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE_ALLOW_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE_ALLOW_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE_ALLOW_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE_ALLOW_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE_ALLOW_WRITE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE_CONCURRENT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE_CONCURRENT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE_CONCURRENT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE_CONCURRENT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE_CONCURRENT_INSERT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE_LOW_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE_LOW_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE_LOW_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE_LOW_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE_LOW_PRIORITY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE_NORMAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COUNT_WRITE_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SUM_TIMER_WRITE_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_TIMER_WRITE_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `AVG_TIMER_WRITE_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_TIMER_WRITE_EXTERNAL` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.threads

Table status: Implemented in MySQL virtual catalog.

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | ScratchBird tracked | ProcArrayManager::ProcessControlBlock.proc_id | Row per active backend. |
| `NAME` | ScratchBird tracked | Constant "thread/<proc_id>" | Always. |
| `TYPE` | ScratchBird tracked | Constant "FOREGROUND" | Always. |
| `PROCESSLIST_ID` | ScratchBird tracked | ProcArrayManager::ProcessControlBlock.proc_id | Always. |
| `PROCESSLIST_USER` | ScratchBird tracked | CatalogManager::SessionInfo.username | When session metadata is available; NULL if session not found. |
| `PROCESSLIST_HOST` | ScratchBird tracked | Constant "local" | Always. |
| `PROCESSLIST_DB` | ScratchBird tracked | Constant "scratchbird" | Always. |
| `PROCESSLIST_COMMAND` | ScratchBird tracked | Derived from ProcessControlBlock.query_start_time | Query when executing, Sleep otherwise. |
| `PROCESSLIST_TIME` | ScratchBird tracked | Elapsed seconds from state_change_time/query_start_time | Always. |
| `PROCESSLIST_STATE` | ScratchBird tracked | Constant "executing" or NULL | Populated only while a query is active. |
| `PROCESSLIST_INFO` | ScratchBird tracked | ProcessControlBlock.query_text | Populated only while a query is active and text is non-empty. |
| `PARENT_THREAD_ID` | Always NULL | Not populated | Never. |
| `ROLE` | Always NULL | Not populated | Never. |
| `INSTRUMENTED` | ScratchBird tracked | Constant "YES" | Always. |
| `HISTORY` | ScratchBird tracked | Constant "YES" | Always. |
| `CONNECTION_TYPE` | ScratchBird tracked | Constant "LOCAL" | Always. |
| `THREAD_OS_ID` | ScratchBird tracked | ProcessControlBlock.backend_pid | Populated when backend_pid is non-zero. |
| `RESOURCE_GROUP` | Always NULL | Not populated | Never. |
| `EXECUTION_ENGINE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `CONTROLLED_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MAX_CONTROLLED_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `TOTAL_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `MAX_TOTAL_MEMORY` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |
| `TELEMETRY_ACTIVE` | Always NULL | Column not present in ScratchBird virtual catalog (schema-only emulation pending) | Never. |

## performance_schema.user_defined_functions

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `UDF_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `UDF_RETURN_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `UDF_TYPE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `UDF_LIBRARY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `UDF_USAGE_COUNT` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.user_variables_by_thread

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.users

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `CURRENT_CONNECTIONS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `TOTAL_CONNECTIONS` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_SESSION_CONTROLLED_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_SESSION_TOTAL_MEMORY` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.variables_by_thread

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `THREAD_ID` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

## performance_schema.variables_info

Table status: Required emulation (schema-only view not yet implemented).

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `VARIABLE_NAME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_SOURCE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `LOGIN` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `COMMAND_LINE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `PERSISTED` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `DYNAMIC` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `VARIABLE_PATH` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MIN_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `MAX_VALUE` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SET_TIME` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SET_USER` | Always NULL | Schema-only emulation required; not yet implemented | Never. |
| `SET_HOST` | Always NULL | Schema-only emulation required; not yet implemented | Never. |

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*