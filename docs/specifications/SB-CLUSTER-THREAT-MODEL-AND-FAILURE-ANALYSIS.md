# SB-CLUSTER-THREAT-MODEL-AND-FAILURE-ANALYSIS
## Applies To: SB-CLUSTER-SWS-MGA-01
### Transaction Model: MGA
### Sharding Model: Single Writer Per Shard

---

# 1. Threat Model

## 1.1 Assumptions

- Nodes may crash.
- Network partitions may occur.
- Clock skew exists.
- Nodes may be compromised.
- Disk corruption possible.
- Replay or message duplication possible.

---

# 2. Threat Categories

---

## 2.1 Split Brain (Dual Leaders)

### Risk
Two nodes accept writes for same shard.

### Mitigation
- Leader term monotonic increment.
- Lease expiration enforcement.
- Fencing token required for every write.

### Failure Outcome if Broken
- Divergent histories.
- Irreconcilable record versions.

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

### Scenario
Cluster splits into majority/minority.

### Expected Behavior
- Majority elects leader.
- Minority cannot form quorum.
- Minority leader loses lease.
- Minority writes rejected.

---

## 2.4 Replica Lag GC Corruption

### Risk
Leader reclaims versions still required by lagging follower.

### Mitigation
- RWM_shard included in GC_safe_shard.
- Version reclamation blocked if below follower watermark.

---

## 2.5 Long-Running Snapshot Data Loss

### Risk
Sweep deletes record versions needed by active snapshot.

### Mitigation
- Snapshot registry.
- OST_shard calculation.
- GC_safe_shard enforcement.

---

## 2.6 Transaction ID Collision

### Risk
Duplicate local_txn_id across shards.

### Mitigation
- GTXID includes shard_id.
- Local txn monotonic per shard.

---

## 2.7 Control Plane Compromise

### Risk
Malicious node injects fake shard leader.

### Mitigation
- Node identity verification.
- Certificate-based authentication.
- Control-plane log signature validation.
- Security epoch enforcement.

---

## 2.8 Replay Attack on Replication Log

### Risk
Duplicate or reordered commit entries applied.

### Mitigation
- Strict ordering by local_txn_id.
- Idempotent apply logic.
- Reject lower-than-last-applied IDs.

---

# 3. Failure Modes and Handling

---

## 3.1 Leader Crash

**Behavior**
- Control plane detects heartbeat timeout.
- New leader elected.
- leader_term incremented.
- New fencing token generated.

**Recovery**
- New leader replays SCL to ensure latest committed state.

---

## 3.2 Follower Crash

**Behavior**
- Follower marked unhealthy.
- Leader continues serving writes.
- RWM_shard updated to exclude follower.

**Recovery**
- Follower replays SCL from last applied position.

---

## 3.3 Disk Corruption

**Behavior**
- Node detects corruption.
- Node leaves cluster.
- Requires manual recovery or rebuild from replica.

---

## 3.4 Snapshot Registry Failure

**Behavior**
- Node fails to heartbeat snapshot.
- Snapshot considered stale after timeout.
- Removed from OST_shard computation.

---

## 3.5 Control Plane Log Corruption

**Mitigation**
- Append-only.
- Checksum validation.
- Majority quorum required for commit.

---

# 4. Residual Risks

- Simultaneous correlated hardware failure.
- Byzantine node behavior (not mitigated in MVP).
- Operator misconfiguration.
- Delayed detection of partition under low traffic.

---

# 5. Security Hardening Recommendations

- Mutual TLS between nodes.
- Certificate pinning for control-plane communication.
- Audit logging for:
  - leader changes
  - shard reassignments
  - epoch changes
  - fencing violations
- Rate limiting join attempts.

---

# 6. Validation Requirements

Cluster must pass:

- Deterministic split-brain simulation.
- High-latency replication test.
- Snapshot retention stress test.
- Leader churn under load test.
- Forced crash recovery test.

---

# 7. Conclusion

This cluster model provides:

- Deterministic safety under partition.
- MGA-consistent visibility rules.
- GC correctness across shards.
- Single-writer enforcement.
- Clean path toward future distributed extensions.

The design prioritizes correctness and determinism over speculative multi-writer complexity.
