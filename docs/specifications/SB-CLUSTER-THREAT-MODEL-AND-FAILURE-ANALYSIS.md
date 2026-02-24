# SB-CLUSTER-THREAT-MODEL-AND-FAILURE-ANALYSIS
## Applies To: SB-CLUSTER-SWS-MGA-01
### Model: Single-Writer-Per-Shard (MGA)

---

# 1. Threat Model

## 1.1 Assumptions

- Nodes may crash unexpectedly.
- Network partitions may occur.
- Clock skew exists.
- Nodes may be compromised.
- Disk corruption is possible.
- Replay or duplicate replication messages may occur.

---

# 2. Threat Categories

## 2.1 Split Brain

### Risk
Two nodes accept writes for same shard.

### Mitigation
- Leader term monotonic increment.
- Lease expiration enforcement.
- Fencing token required for every write.

---

## 2.2 Stale Leader Writes

### Risk
Old leader continues writing after demotion.

### Mitigation
- Engine-level fencing validation.
- Lease expiration enforced locally.
- Write path checks leader_term.

---

## 2.3 Network Partition

### Expected Behavior
- Majority elects leader.
- Minority loses lease.
- Minority writes rejected.

---

## 2.4 Replica Lag GC Corruption

### Risk
Leader reclaims versions still required by lagging follower.

### Mitigation
- RWM_shard included in GC_safe_shard.

---

## 2.5 Long-Running Snapshot Loss

### Risk
Sweep deletes versions required by snapshot.

### Mitigation
- Snapshot registry.
- OST_shard computation.
- GC_safe_shard enforcement.

---

## 2.6 Transaction Identity Collision

### Risk
Duplicate local_txn_id across shards.

### Mitigation
- GTXID includes shard_id.

---

## 2.7 Control Plane Compromise

### Mitigation
- Mutual TLS between nodes.
- Certificate validation.
- Signed control-plane log entries.

---

## 2.8 Replication Replay Attack

### Mitigation
- Strict ordering by local_txn_id.
- Idempotent apply logic.
- Reject lower-than-last-applied IDs.

---

# 3. Failure Modes

## 3.1 Leader Crash
- Election triggered.
- leader_term incremented.
- Fencing prevents stale writes.

## 3.2 Follower Crash
- Follower marked unhealthy.
- Leader continues writes.
- Follower replays SCL on recovery.

## 3.3 Disk Corruption
- Node leaves cluster.
- Requires rebuild from replica.

## 3.4 Snapshot Registry Failure
- Snapshot expires after timeout.

---

# 4. Residual Risks

- Correlated hardware failures.
- Byzantine behavior (not mitigated in MVP).
- Operator misconfiguration.

---

# 5. Validation Requirements

Cluster must pass:

- Deterministic split-brain simulation.
- High-latency replication stress.
- Snapshot retention under load.
- Leader churn under load.
- Crash recovery consistency test.

---

# 6. Conclusion

This cluster model provides:
- Deterministic single-writer enforcement.
- MGA-consistent snapshot behavior.
- GC safety across shards.
- Clean failover behavior.
- Deterministic domain replication.

This document defines the cluster threat envelope and required safeguards for ScratchBird Beta.

