# TODO: Checkpoint Policy (Non-WAL MGA)

Goal: Define checkpoint behavior in a non-WAL MGA system to clarify flush/consistency semantics, especially for shadow/live backups.

Requirements:
- Clarify what constitutes a checkpoint (e.g., flushing dirty pages to disk, advancing catalog root/generation).
- Define frequency/triggers (time-based, dirty-page thresholds, manual).
- Specify interaction with shadow/live backup: ensure consistent snapshot boundaries.
- Document guarantees: what is durable at checkpoint vs between checkpoints.
- Expose minimal diagnostics for checkpoint events (last checkpoint time, duration, pages flushed).

Deliverables:
- Short spec of checkpoint triggers/behavior and diagnostics.
