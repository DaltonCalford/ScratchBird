# Beta Replication Testing Strategy

**Document:** 07_TESTING_STRATEGY.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Testing Philosophy](#testing-philosophy)
3. [Unit Testing Strategy](#unit-testing-strategy)
4. [Integration Testing Strategy](#integration-testing-strategy)
5. [Chaos Testing](#chaos-testing)
6. [Performance Benchmarks](#performance-benchmarks)
7. [Correctness Verification](#correctness-verification)
8. [Test Infrastructure](#test-infrastructure)
9. [Continuous Integration](#continuous-integration)
10. [Test Coverage Requirements](#test-coverage-requirements)

---

## Executive Summary

### Purpose

This document defines the comprehensive testing strategy for ScratchBird Beta replication features (UUIDv8-HLC, leaderless quorum, schema colocation, time-partitioned Merkle forests, distributed MGA). The strategy ensures correctness, performance, and reliability before Beta release.

### Testing Levels

| Level | Focus | Coverage Target | Test Count (Estimated) |
|-------|-------|----------------|------------------------|
| **Unit** | Individual components | 100% code coverage | ~500 tests |
| **Integration** | Cross-component scenarios | All feature paths | ~200 tests |
| **Chaos** | Failure scenarios | Network, node, clock | ~50 tests |
| **Performance** | Throughput, latency, overhead | All critical paths | ~30 benchmarks |
| **Correctness** | Distributed invariants | Convergence, causality | ~50 tests |

**Total**: ~830 tests + 30 benchmarks

### Success Criteria

**Beta is test-ready when**:
- [ ] All unit tests pass (100% code coverage)
- [ ] All integration tests pass (all features working together)
- [ ] All chaos tests pass (system resilient to failures)
- [ ] All performance targets met (or documented deviations)
- [ ] Correctness verification passes (no data loss, no divergence)

---

## Testing Philosophy

### Principles

1. **Test Early, Test Often**: Tests written alongside implementation (TDD encouraged)
2. **Fail Fast**: Tests catch bugs before code review (pre-commit hooks)
3. **Deterministic Tests**: No flaky tests (seed RNGs, mock time, control network)
4. **Comprehensive Coverage**: Test happy paths, edge cases, failure paths
5. **Performance as Correctness**: Latency/throughput regressions are bugs

### Test Pyramid

```
           /\
          /  \
         / UI \           ← N/A for Beta (no UI changes)
        /______\
       /        \
      /  E2E     \        ← Integration tests (~200)
     /____________\
    /              \
   /  Integration   \     ← Cross-component (~100)
  /__________________\
 /                    \
/       Unit           \   ← Component-level (~500)
/________________________\
```

**Beta Focus**: Unit + Integration + Chaos (no E2E UI testing needed)

### Test Environment

**Local Development**:
- Single-machine 3-node cluster (Docker Compose)
- In-memory storage (fast test execution)
- Mocked time (control clock advancement)

**CI Environment**:
- 3-machine cluster (real network latency)
- Persistent storage (test durability)
- Real time (detect time-dependent bugs)

**Chaos Environment**:
- 5-machine cluster (tolerate failures)
- Network chaos (Toxiproxy for partitions, latency injection)
- Node failures (kill -9, disk full, memory exhaustion)

---

## Unit Testing Strategy

### UUIDv8-HLC Tests

**File**: `tests/unit/test_uuidv8_hlc.cpp`

**Test Cases**:

```cpp
TEST(UuidV8HLC, MonotonicitySequential) {
    // Generate 10K UUIDs sequentially, verify monotonicity
}

TEST(UuidV8HLC, MonotonicityParallel) {
    // Generate UUIDs from 4 threads, verify per-thread monotonicity
}

TEST(UuidV8HLC, ClockSkewHandling) {
    // Simulate clock going backward (NTP adjustment), verify HLC correction
}

TEST(UuidV8HLC, CounterOverflow) {
    // Generate 5000 UUIDs in same millisecond, verify stall occurs
}

TEST(UuidV8HLC, ComparisonDeterministic) {
    // Verify UUIDv8-HLC comparison (timestamp > counter > subsec > node_id > random)
}

TEST(UuidV8HLC, FieldExtraction) {
    // Extract timestamp, counter, node_id, verify correctness
}

TEST(UuidV8HLC, RFC9562Compliance) {
    // Verify version bits (0b1000), variant bits (0b10)
}
```

**Coverage Target**: 100% code coverage in `uuidv8_hlc.cpp`

### Quorum Coordinator Tests

**File**: `tests/unit/test_quorum_coordinator.cpp`

**Test Cases**:

```cpp
TEST(QuorumCoordinator, WriteQuorumSuccess) {
    // W=2, N=3, all nodes online → Write succeeds
}

TEST(QuorumCoordinator, WriteQuorumFailure) {
    // W=3, N=3, 1 node offline → Write fails
}

TEST(QuorumCoordinator, WriteTimeout) {
    // W=2, N=3, 1 node slow (>timeout) → Write succeeds with 2 ACKs
}

TEST(QuorumCoordinator, ReadQuorumReconciliation) {
    // R=2, N=3, versions differ → Return latest (HLC-based)
}

TEST(QuorumCoordinator, ConflictResolution) {
    // Two versions with different HLCs → Winner determined correctly
}

TEST(QuorumCoordinator, ReadRepairTriggered) {
    // Read detects divergence → Read repair scheduled (async)
}
```

**Coverage Target**: 100% code coverage in `quorum_coordinator.cpp`

### Hinted Handoff Tests

**File**: `tests/unit/test_hinted_handoff.cpp`

**Test Cases**:

```cpp
TEST(HintedHandoff, HintStorage) {
    // Store hint to disk, verify durable
}

TEST(HintedHandoff, HintDeliveryOnRecovery) {
    // Node recovers, hints delivered within 10 seconds
}

TEST(HintedHandoff, HintExpiration) {
    // Hints older than 7 days removed
}

TEST(HintedHandoff, HintDeliveryFailureRetry) {
    // Delivery fails, hint remains in log for retry
}
```

**Coverage Target**: 100% code coverage in `hinted_handoff.cpp`

### Merkle Tree Tests

**File**: `tests/unit/test_merkle_tree.cpp`

**Test Cases**:

```cpp
TEST(MerkleTree, InsertLeaf) {
    // Insert leaf, verify leaf count increments
}

TEST(MerkleTree, ComputeRootHash) {
    // Insert 1000 leaves, compute root, verify deterministic
}

TEST(MerkleTree, RootHashDeterministic) {
    // Insert leaves in different orders, verify same root hash
}

TEST(MerkleTree, Serialization) {
    // Serialize tree to disk, deserialize, verify identical
}

TEST(MerkleTree, EmptyTree) {
    // Empty tree root hash = zero hash
}
```

**Coverage Target**: 100% code coverage in `merkle_tree.cpp`

### Merkle Forest Tests

**File**: `tests/unit/test_merkle_forest.cpp`

**Test Cases**:

```cpp
TEST(MerkleForest, WindowAssignment) {
    // Insert row with ts=5000ms, verify assigned to Window 1
}

TEST(MerkleForest, TreeSealing) {
    // Time advances, verify previous window sealed
}

TEST(MerkleForest, SealedTreeImmutable) {
    // After sealing, insert new row, verify sealed tree unchanged
}

TEST(MerkleForest, ActiveTreeUpdates) {
    // Insert row in active window, verify root hash changes
}
```

**Coverage Target**: 100% code coverage in `merkle_forest.cpp`

### Partition Key Tests

**File**: `tests/unit/test_partition_keys.cpp`

**Test Cases**:

```cpp
TEST(PartitionKeys, ParsePartitionBy) {
    // Parse "PARTITION BY user_id", verify AST node
}

TEST(PartitionKeys, ComputeShardId) {
    // Given partition key value, compute shard_id deterministically
}

TEST(PartitionKeys, ImmutabilityEnforcement) {
    // UPDATE partition key column → Error
}

TEST(PartitionKeys, ForeignKeyColocationSuggestion) {
    // FK detected, verify colocation suggestion logged
}
```

**Coverage Target**: 100% code coverage in partition key logic

### Distributed TIP Tests

**File**: `tests/unit/test_distributed_tip.cpp`

**Test Cases**:

```cpp
TEST(DistributedTIP, GlobalXidAllocation) {
    // Node 1 and Node 2 allocate XIDs, verify no collision
}

TEST(DistributedTIP, TIPReplication) {
    // Node A commits, TIP entry replicated to Node B
}

TEST(DistributedTIP, DistributedVisibility) {
    // Version written on Node A, read on Node B, visibility correct
}

TEST(DistributedTIP, HLCCausalOrdering) {
    // Version with HLC (1000, 5) visible to reader with HLC (1000, 10)
}
```

**Coverage Target**: 100% code coverage in distributed TIP logic

---

## Integration Testing Strategy

### Multi-Component Scenarios

**Goal**: Test interactions between components (not just isolated units).

### Leaderless Quorum Integration Tests

**File**: `tests/integration/test_leaderless_quorum_integration.cpp`

**Test Cases**:

```cpp
TEST(LeaderlessQuorumIntegration, WriteAndRead) {
    // Write with W=2, read with R=2, verify consistency
}

TEST(LeaderlessQuorumIntegration, ConcurrentWrites) {
    // Two nodes write same row concurrently, verify convergence
}

TEST(LeaderlessQuorumIntegration, HintedHandoffAfterFailure) {
    // Node offline during write, recovers, receives hint
}

TEST(LeaderlessQuorumIntegration, ReadRepairConvergence) {
    // Divergent replicas, read triggers repair, verify convergence
}

TEST(LeaderlessQuorumIntegration, QuorumNotAchieved) {
    // W=3, N=3, 2 nodes offline → Write fails gracefully
}
```

### Schema Colocation Integration Tests

**File**: `tests/integration/test_schema_colocation_integration.cpp`

**Test Cases**:

```cpp
TEST(SchemaColocationIntegration, ColocatedMultiTableTransaction) {
    // Transaction: INSERT users + INSERT orders (same partition key)
    // Verify ACID, single-shard execution
}

TEST(SchemaColocationIntegration, CrossShardTransaction2PC) {
    // Transaction: UPDATE account_A + UPDATE account_B (different shards)
    // Verify 2PC commit
}

TEST(SchemaColocationIntegration, ColocatedJoin) {
    // Query: JOIN users and orders on user_id
    // Verify single-shard join (no shuffle)
}

TEST(SchemaColocationIntegration, NonColocatedJoinShuffle) {
    // Query: JOIN orders and products on product_id
    // Verify multi-shard join (shuffle required)
}
```

### Merkle Forest Integration Tests

**File**: `tests/integration/test_merkle_forest_integration.cpp`

**Test Cases**:

```cpp
TEST(MerkleForestIntegration, AntiEntropyDetectsDivergence) {
    // Node C missing row, anti-entropy detects, repairs
}

TEST(MerkleForestIntegration, LSMCompactionSealsTree) {
    // Trigger LSM compaction, verify Merkle tree sealed
}

TEST(MerkleForestIntegration, VerifySealedTrees720Trees) {
    // 720 sealed trees (30 days), verify comparison < 100ms
}

TEST(MerkleForestIntegration, RepairDivergentWindow) {
    // Window 5 diverged, drill down, find row, repair
}
```

### MGA Integration Tests

**File**: `tests/integration/test_mga_integration.cpp`

**Test Cases**:

```cpp
TEST(MGAIntegration, CrossNodeTransactionVisibility) {
    // Node A writes, Node B reads, verify TIP-based visibility
}

TEST(MGAIntegration, DistributedGC) {
    // Long-running transaction on Node A, GC on Node B respects global OIT
}

TEST(MGAIntegration, CommitLogStreaming) {
    // Node A commits, commit log streamed to Node B, TIP replicated
}

TEST(MGAIntegration, 2PCPreparedState) {
    // Cross-shard transaction, verify TIP PREPARED state on both shards
}
```

### End-to-End Scenario Tests

**File**: `tests/integration/test_e2e_scenarios.cpp`

**Test Cases**:

```cpp
TEST(E2EScenario, IoTDataIngestion) {
    // Scenario: 100K sensor writes (multi-node), verify all data visible
}

TEST(E2EScenario, EcommercecOrderFlow) {
    // Scenario: User + Orders + Payments (colocated), ACID transaction
}

TEST(E2EScenario, BankTransfer) {
    // Scenario: Transfer between accounts (cross-shard 2PC)
}

TEST(E2EScenario, NodeFailureRecovery) {
    // Scenario: Write, kill node, restart, verify data recovered
}
```

---

## Chaos Testing

### Purpose

Verify system resilience under extreme failure conditions (network partitions, node crashes, clock skew).

### Chaos Test Environment

**Infrastructure**:
- 5-node cluster (tolerate 2 node failures)
- Toxiproxy for network chaos (latency injection, packet loss, partitions)
- Controlled time (advance clock, simulate NTP adjustments)

**Tools**:
- **Toxiproxy**: Network proxy for chaos injection
- **Jepsen** (optional): Distributed systems verification framework
- **Custom Chaos Controller**: Orchestrate failures, verify invariants

### Network Partition Tests

**File**: `tests/chaos/test_network_partition.cpp`

**Test Cases**:

```cpp
TEST(ChaosTest, NetworkPartitionQuorumAvailability) {
    // Partition: {Node A, Node B, Node C} vs {Node D, Node E}
    // Verify: Majority partition (3 nodes) writable, minority (2 nodes) read-only
}

TEST(ChaosTest, NetworkPartitionHealing) {
    // Partition cluster, wait 30 seconds, heal partition
    // Verify: Anti-entropy converges, no data loss
}

TEST(ChaosTest, NetworkPartitionConcurrentWrites) {
    // Partition: Both sides write same row (conflict)
    // Heal partition, verify: UUIDv8-HLC conflict resolution, convergence
}

TEST(ChaosTest, FlappingPartition) {
    // Partition, heal, partition again (10 cycles)
    // Verify: System stable, no data loss
}
```

### Node Failure Tests

**File**: `tests/chaos/test_node_failure.cpp`

**Test Cases**:

```cpp
TEST(ChaosTest, NodeCrashDuringWrite) {
    // Kill node mid-write (after W-1 ACKs), verify write succeeds
}

TEST(ChaosTest, NodeCrashDuringRead) {
    // Kill node mid-read (after R-1 responses), verify read succeeds
}

TEST(ChaosTest, NodeRecoveryViaHints) {
    // Node offline 1 minute, recovers, verify hints delivered
}

TEST(ChaosTest, NodeRecoveryViaAntiEntropy) {
    // Node offline 10 minutes (hints expired), verify anti-entropy repairs
}

TEST(ChaosTest, CoordinatorCrash) {
    // Coordinator crashes after sending writes, client retries
    // Verify: Idempotency (duplicate write detected)
}

TEST(ChaosTest, CascadingFailures) {
    // Kill 2 nodes simultaneously (out of 5), verify system operational
}
```

### Clock Skew Tests

**File**: `tests/chaos/test_clock_skew.cpp`

**Test Cases**:

```cpp
TEST(ChaosTest, ClockSkewHLCCorrection) {
    // Node A clock: 1000ms, Node B clock: 900ms (100ms skew)
    // Verify: HLC corrects ordering, no causality violations
}

TEST(ChaosTest, ClockSkewCounterIncrement) {
    // Clock goes backward (NTP adjustment), verify counter increments
}

TEST(ChaosTest, ExtremeClockSkew) {
    // Node A clock: 1000ms, Node B clock: 500ms (500ms skew)
    // Verify: HLC counter grows, alert logged
}

TEST(ChaosTest, LeapSecond) {
    // Simulate leap second (time jumps forward), verify no issues
}
```

### Disk Full Tests

**File**: `tests/chaos/test_disk_full.cpp`

**Test Cases**:

```cpp
TEST(ChaosTest, DiskFullDuringWrite) {
    // Fill disk during write, verify graceful failure (no corruption)
}

TEST(ChaosTest, DiskFullHintLog) {
    // Fill disk, hint log append fails, verify error logged
}

TEST(ChaosTest, DiskFullRecovery) {
    // Fill disk, free space, verify writes resume
}
```

---

## Performance Benchmarks

### Benchmarking Framework

**Tool**: Google Benchmark (`benchmark::State`)

**Metrics**:
- **Throughput**: Operations per second (TPS)
- **Latency**: p50, p99, p999 (milliseconds)
- **Overhead**: CPU usage, memory usage

**Environment**:
- 3-node cluster
- 16-core machines (isolate benchmark processes)
- 1 Gbps network

### Write Throughput Benchmark

**File**: `tests/benchmark/benchmark_write_throughput.cpp`

**Scenario**: Write 1M rows to table with `replication_mode='leaderless_quorum'`

```cpp
BENCHMARK(WriteQuorumN3W2) {
    // Config: N=3, W=2, R=2
    for (auto _ : state) {
        coordinator.coordinateWrite(write_request, replicas, W=2);
    }
    state.SetItemsProcessed(state.iterations());
}

// Target: > 50K TPS per shard
```

**Expected Results**:
- **W=1, N=3**: 80K+ TPS (minimal coordination)
- **W=2, N=3**: 50K+ TPS (balanced)
- **W=3, N=3**: 30K+ TPS (high durability)

### Read Latency Benchmark

**File**: `tests/benchmark/benchmark_read_latency.cpp`

**Scenario**: Read 100K rows with `replication_mode='leaderless_quorum'`

```cpp
BENCHMARK(ReadQuorumN3R2) {
    // Config: N=3, W=2, R=2
    for (auto _ : state) {
        coordinator.coordinateRead(read_request, replicas, R=2);
    }
}

// Target: p99 < 20ms (same DC)
```

**Expected Results**:
- **p50**: < 5ms
- **p99**: < 20ms
- **p999**: < 50ms

### Anti-Entropy Overhead Benchmark

**File**: `tests/benchmark/benchmark_anti_entropy_overhead.cpp`

**Scenario**: Run anti-entropy verification on 1M rows with 720 sealed trees

```cpp
BENCHMARK(AntiEntropyVerification) {
    MerkleForest forest(/*num_sealed_trees=*/720);
    for (auto _ : state) {
        forest.verifySealedTrees(remote_trees);
    }
}

// Target: < 2% CPU overhead
```

**Expected Results**:
- **CPU Usage**: < 2% (during verification)
- **Verification Time**: < 100ms (720 trees)

### Conflict Resolution Benchmark

**File**: `tests/benchmark/benchmark_conflict_resolution.cpp`

**Scenario**: Resolve 1000 conflicts (UUIDv8-HLC comparison)

```cpp
BENCHMARK(ConflictResolution) {
    for (auto _ : state) {
        auto winner = resolveConflict(uuid_a, uuid_b);
    }
}

// Target: < 1ms per conflict
```

**Expected Results**:
- **Per-Conflict Latency**: < 1ms

---

## Correctness Verification

### Distributed Invariants

**Goal**: Verify distributed system properties hold under all conditions.

### Convergence Verification

**Test**: `tests/correctness/test_convergence.cpp`

**Invariant**: Given no new writes, all replicas converge to same state.

```cpp
TEST(CorrectnessTest, EventualConvergence) {
    // 1. Write 1000 rows to 3-node cluster
    // 2. Introduce network partition (split cluster)
    // 3. Write different data on both sides (conflicts)
    // 4. Heal partition, wait for anti-entropy
    // 5. Verify: All replicas have identical state (UUIDv8-HLC winners)
}
```

**Verification Method**:
- Compute hash of all rows on each replica
- Compare hashes (should be identical after convergence)

### Causality Verification

**Test**: `tests/correctness/test_causality.cpp`

**Invariant**: If event A causally precedes event B, then `hlc(A) < hlc(B)`.

```cpp
TEST(CorrectnessTest, CausalOrdering) {
    // 1. Node A writes row (event A, hlc_a)
    // 2. Node B reads row, then writes (event B, hlc_b)
    // 3. Verify: hlc_b > hlc_a (B causally depends on A)
}
```

**Verification Method**:
- Trace event dependencies (happens-before relationships)
- Verify HLC ordering preserves causality

### No Data Loss Verification

**Test**: `tests/correctness/test_no_data_loss.cpp`

**Invariant**: Every write that returned success is eventually visible on all replicas.

```cpp
TEST(CorrectnessTest, NoDataLoss) {
    // 1. Write 10K rows (track successful writes)
    // 2. Inject chaos (node failures, partitions)
    // 3. Wait for convergence (anti-entropy completes)
    // 4. Verify: All successful writes visible on all replicas
}
```

**Verification Method**:
- Log all successful write XIDs
- Query all replicas, verify all XIDs present

### Linearizability Verification (Optional)

**Test**: `tests/correctness/test_linearizability.cpp`

**Invariant**: Operations appear to execute atomically at some point between invocation and response.

**Method**: Use Jepsen linearizability checker (advanced).

---

## Test Infrastructure

### Test Harness

**File**: `tests/common/test_harness.h`

**Purpose**: Shared test utilities (cluster setup, mocking, assertions).

```cpp
class TestCluster {
public:
    TestCluster(size_t num_nodes);
    auto getNode(NodeId id) -> Node&;
    auto createPartition(std::vector<NodeId> group1, std::vector<NodeId> group2) -> void;
    auto healPartition() -> void;
    auto killNode(NodeId id) -> void;
    auto restartNode(NodeId id) -> void;
    auto performAntiEntropy() -> void;
    auto waitForConvergence() -> void;
};
```

### Mocking and Fixtures

**Time Mocking**:

```cpp
class MockClock {
public:
    auto currentTimeMs() -> uint64_t { return time_ms_; }
    auto advance(uint64_t delta_ms) -> void { time_ms_ += delta_ms; }
private:
    uint64_t time_ms_{0};
};
```

**Network Mocking**:

```cpp
class MockNetwork {
public:
    auto send(NodeId from, NodeId to, Message msg) -> void;
    auto injectLatency(uint64_t latency_ms) -> void;
    auto injectPartition(std::vector<NodeId> group1, std::vector<NodeId> group2) -> void;
};
```

### Assertions

```cpp
// Custom assertions for distributed systems
ASSERT_EVENTUALLY(condition, timeout_ms);  // Wait for eventual consistency
ASSERT_CONVERGED(replicas);               // All replicas have same state
ASSERT_NO_DATA_LOSS(written_xids, replicas);  // All writes visible
```

---

## Continuous Integration

### CI Pipeline

**Platform**: GitHub Actions (or equivalent)

**Pipeline Stages**:

```
1. Build
   - Compile C++ code
   - Link test binaries

2. Unit Tests
   - Run all unit tests
   - Generate coverage report (lcov)
   - Require: 100% coverage

3. Integration Tests
   - Spin up 3-node cluster (Docker Compose)
   - Run integration tests
   - Require: All tests pass

4. Chaos Tests
   - Spin up 5-node cluster
   - Inject network chaos (Toxiproxy)
   - Run chaos tests
   - Require: All tests pass

5. Performance Benchmarks
   - Run benchmarks
   - Compare with baseline (detect regressions)
   - Require: No regression > 10%

6. Coverage Report
   - Upload to Codecov (or equivalent)
   - Display badge in README
```

### CI Configuration

**File**: `.github/workflows/beta_replication_tests.yml`

```yaml
name: Beta Replication Tests

on: [push, pull_request]

jobs:
  unit_tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: cmake . && make
      - name: Unit Tests
        run: ./tests/unit_tests --gtest_output=xml:unit_results.xml
      - name: Coverage
        run: lcov --capture --directory . --output-file coverage.info

  integration_tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Start Cluster
        run: docker-compose -f tests/docker-compose-3node.yml up -d
      - name: Integration Tests
        run: ./tests/integration_tests

  chaos_tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Start Cluster with Toxiproxy
        run: docker-compose -f tests/docker-compose-5node-chaos.yml up -d
      - name: Chaos Tests
        run: ./tests/chaos_tests

  benchmarks:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run Benchmarks
        run: ./tests/benchmarks --benchmark_format=json > bench_results.json
      - name: Compare with Baseline
        run: python scripts/compare_benchmarks.py bench_results.json baseline.json
```

---

## Test Coverage Requirements

### Code Coverage Targets

| Component | Line Coverage | Branch Coverage |
|-----------|---------------|-----------------|
| UUIDv8-HLC | 100% | 100% |
| Quorum Coordinator | 100% | 100% |
| Hinted Handoff | 100% | 95% (error paths hard to trigger) |
| Merkle Tree | 100% | 100% |
| Merkle Forest | 100% | 100% |
| Partition Keys | 100% | 100% |
| Distributed TIP | 100% | 100% |
| Global GC | 95% | 90% (race conditions hard to test) |

**Overall Target**: 99%+ line coverage, 95%+ branch coverage

### Test Count Requirements

| Test Type | Minimum Count |
|-----------|---------------|
| Unit Tests | 500 |
| Integration Tests | 200 |
| Chaos Tests | 50 |
| Correctness Tests | 50 |
| Performance Benchmarks | 30 |

**Total**: 830 tests + 30 benchmarks

### Beta Readiness Checklist

**Testing Sign-off**:
- [ ] All unit tests pass (500+)
- [ ] All integration tests pass (200+)
- [ ] All chaos tests pass (50+)
- [ ] All correctness tests pass (50+)
- [ ] All performance benchmarks meet targets (30+)
- [ ] Code coverage ≥ 99% (line), ≥ 95% (branch)
- [ ] No known critical bugs (severity P0/P1)
- [ ] Chaos testing passed 100 consecutive runs (no flaky failures)

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** All implementation phases (01-06)

---

**End of Document 07**
