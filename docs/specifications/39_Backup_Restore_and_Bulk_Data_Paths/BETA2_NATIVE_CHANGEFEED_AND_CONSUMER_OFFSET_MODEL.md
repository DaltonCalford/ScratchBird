# Beta 2 Native Changefeed And Consumer Offset Model

## Purpose

Define the native ScratchBird changefeed model for committed change envelopes,
consumer cursors, resumable offsets, and policy-bounded downstream delivery.

## Governing rules

1. Changefeeds are derivative of committed MGA publication.
2. Every changefeed event has a stable commit identity and event identity.
3. Resume tokens and consumer offsets are catalog state.
4. Schema change and projection policy are explicit.
5. Changefeed replay may lag but may not invent missing commit order.

## Canonical metadata

- `sb_changefeed`
  - `feed_uuid`
  - `feed_name`
  - `source_scope`
  - `projection_policy`
  - `retention_policy`
  - `delivery_mode`
  - `enabled`
- `sb_changefeed_event`
  - `event_uuid`
  - `feed_uuid`
  - `commit_epoch`
  - `txn_uuid`
  - `change_kind`
  - `row_identity`
  - `before_locator`
  - `after_locator`
  - `schema_epoch`
- `sb_changefeed_cursor`
  - `cursor_uuid`
  - `feed_uuid`
  - `consumer_name`
  - `last_commit_epoch`
  - `last_event_uuid`
  - `status`

## Admitted delivery modes

- `ROW_IMAGE_FULL`
- `ROW_IMAGE_KEYS_ONLY`
- `ROW_IMAGE_AFTER_ONLY`
- `EVENT_QUEUE_BRIDGE`

## Publication flow

1. Transaction commits under MGA rules.
2. Changefeed publisher reads committed change markers.
3. Publisher emits ordered `sb_changefeed_event` rows.
4. Optional bridge routes those events into transactional eventing or external
   connector sinks.

## Cursor flow

1. Consumer opens or resumes a cursor.
2. Runtime selects events strictly after the cursor position.
3. Consumer acknowledges progress with the last processed event.
4. Cursor advances only on acknowledged progress.

## Refusal rules

- `CHANGEFEED_SCOPE_UNKNOWN`
- `CHANGEFEED_SCHEMA_EPOCH_INCOMPATIBLE`
- `CHANGEFEED_CURSOR_INVALID`
- `CHANGEFEED_RETENTION_EXPIRED`

## Metrics

- feed lag by commit epoch
- events emitted
- cursor lag
- schema incompatibility refusals
- bridge delivery failures

## Example

```sql
create changefeed sales_feed
on table sales.orders
mode row_image_after_only;
select * from sb_changefeed.fetch(feed_name => 'sales_feed', consumer_name => 'etl_a');
```

## Cross-section requirements

- section `39` owns changefeed emission and retention
- section `24` owns feed and cursor metadata
- section `25` owns queue bridge integration
