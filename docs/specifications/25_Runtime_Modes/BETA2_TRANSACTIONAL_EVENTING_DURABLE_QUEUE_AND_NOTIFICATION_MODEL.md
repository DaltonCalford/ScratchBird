# Beta 2 Transactional Eventing Durable Queue And Notification Model

## Purpose

Define the native ScratchBird event publication, durable queue, consumer cursor,
and notification model for transactional messaging without weakening MGA truth.

## Governing rules

1. Event publication is derivative of committed MGA state and may never become
   recovery authority.
2. Queue delivery is at-least-once unless a stronger policy is explicitly
   certified for one queue family.
3. Every publication has a stable event envelope and a stable queue message id.
4. Consumer progress is explicit catalog state, not hidden session memory.
5. Queue activation and notifications are operator-visible background work.

## Canonical metadata

- `sb_event_topic`
  - `topic_uuid`
  - `topic_name`
  - `event_family`
  - `retention_policy`
  - `enabled`
- `sb_queue`
  - `queue_uuid`
  - `queue_name`
  - `topic_uuid`
  - `delivery_policy`
  - `max_attempts`
  - `dead_letter_queue_uuid`
  - `activation_policy`
  - `enabled`
- `sb_event_publication`
  - `event_uuid`
  - `topic_uuid`
  - `publish_txn_uuid`
  - `commit_epoch`
  - `event_type`
  - `headers_json`
  - `payload_locator`
  - `created_at`
- `sb_queue_message`
  - `message_uuid`
  - `queue_uuid`
  - `event_uuid`
  - `visible_after`
  - `lease_owner`
  - `lease_until`
  - `attempt_count`
  - `state`
- `sb_queue_consumer_cursor`
  - `cursor_uuid`
  - `queue_uuid`
  - `consumer_name`
  - `last_acked_message_uuid`
  - `last_seen_commit_epoch`
  - `status`
- `sb_queue_activation_job`
  - `job_uuid`
  - `queue_uuid`
  - `worker_class`
  - `max_parallelism`
  - `backoff_policy`
  - `enabled`

## Event envelope

Every event shall carry:

- `event_uuid`
- `topic_name`
- `event_type`
- `publish_txn_uuid`
- `commit_epoch`
- `producer_identity`
- `headers_json`
- `payload_locator`

The canonical envelope shall be renderable as a native rowset and may be
projected into `CloudEvents`-style external shapes where policy allows.

## Publication flow

1. User transaction mutates committed data.
2. Transaction also records one or more pending event intents.
3. MGA commit publishes data truth first.
4. Event publisher converts committed intents into `sb_event_publication` rows.
5. Queue fanout creates `sb_queue_message` rows for every subscribed queue.
6. Queue activation signals are emitted only after publication succeeds.

If publication fanout fails, the transaction remains committed and the event
publisher enters retry state. No queue design may roll back committed data.

## Delivery flow

1. Consumer requests a batch from one queue.
2. Runtime selects visible messages by queue policy.
3. Selected messages receive a lease with `lease_owner` and `lease_until`.
4. Consumer acknowledges, releases, or dead-letters the leased messages.
5. Cursor state is updated only on acknowledged progress.

Expired leases return messages to visible state.

## Activation workers

Activation workers are ordinary managed jobs and must publish:

- queue backlog
- lease count
- retry count
- dead-letter count
- worker launch count

Activation may run:

- on backlog threshold
- on event-family threshold
- on schedule

## Notification classes

- `LOCAL_SESSION_SIGNAL`
- `QUEUE_BACKLOG_ALERT`
- `OPERATOR_AUDIT_EVENT`
- `CONNECTOR_SINK_EVENT`

No notification class may bypass queue durability rules when a durable queue is
configured for the same event family.

## Refusal rules

- `EVENT_TOPIC_UNKNOWN`
- `QUEUE_DISABLED`
- `QUEUE_DELIVERY_POLICY_UNSUPPORTED`
- `QUEUE_CONSUMER_CURSOR_CONFLICT`
- `QUEUE_LEASE_STALE`
- `EVENT_PUBLICATION_RETRY_REQUIRED`

## Metrics

- events published
- queue messages created
- visible backlog
- leased backlog
- dead-letter backlog
- average delivery lag
- retry and poison-message counts

## Examples

```sql
create event topic order_events event family transactional_json;
create durable queue order_dispatch on topic order_events;
call sb_event.publish(
    topic_name => 'order_events',
    event_type => 'order.created',
    payload_json => '{"order_id":"6f1b..."}'
);
select * from sb_queue.dequeue(queue_name => 'order_dispatch', max_messages => 100);
```

## Cross-section requirements

- section `25` owns queue runtime, leases, and activation
- section `20` owns audit visibility and event diagnostics
- section `24` owns queue and topic catalog publication
- section `31` owns conformance and poison-message certification
