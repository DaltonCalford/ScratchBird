# Follower Apply Idempotence Results

Validated by `FollowerApplyPipelineTest.ReplayIsIdempotentAndOrderingIsEnforced`:
- first apply of `local_txn_id=1` succeeds and advances RWM.
- replay of `local_txn_id=1` returns `ALREADY_APPLIED` without side effects.
- replay-safe behavior is deterministic and keeps RWM stable.
