# UUIDv8 with Hybrid Logical Clock Architecture

**Document:** 01_UUIDV8_HLC_ARCHITECTURE.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [UUID Format Specification](#uuid-format-specification)
3. [Hybrid Logical Clock Design](#hybrid-logical-clock-design)
4. [Generation Algorithm](#generation-algorithm)
5. [Conflict Resolution](#conflict-resolution)
6. [Coexistence with UUIDv7](#coexistence-with-uuidv7)
7. [API Design](#api-design)
8. [Testing Requirements](#testing-requirements)
9. [Migration and Adoption](#migration-and-adoption)

---

## Executive Summary

### Purpose

This specification defines **UUIDv8-HLC**, a custom UUID format (RFC 9562 version 8) that embeds Hybrid Logical Clock (HLC) counters for causal ordering in distributed multi-master replication scenarios. UUIDv8-HLC addresses the clock skew and causality challenges inherent in leaderless quorum replication while maintaining time-ordering properties.

**Critical Design Decision**: UUIDv7 remains the standard system identifier (RFC 9562 compliant, unchanged). UUIDv8-HLC is a **separate, additional** format used specifically for multi-master conflict resolution in Beta cluster features.

### Key Requirements

1. **Standards Preservation**: UUIDv7 (RFC 9562 v7) remains unchanged and standard
2. **Expansion Capability**: UUIDv8 (RFC 9562 v8) adds HLC for multi-master scenarios
3. **Causality Tracking**: HLC counter provides causal ordering across nodes
4. **Clock Skew Resilience**: Logical counter handles physical clock drift
5. **Backward Compatibility**: Tables can use UUIDv7 or UUIDv8 (user choice)

### Use Cases

**Use UUIDv7 (Standard)**:
- Single-node deployments
- Primary-replica replication (Mode 1: Logical, Mode 2: Physical)
- Tables that don't require multi-master writes
- Existing data (no migration needed)

**Use UUIDv8-HLC (Beta)**:
- Leaderless quorum replication (Mode 3)
- Multi-master write scenarios
- Tables requiring conflict resolution across nodes
- Distributed transactions with causal dependencies

---

## UUID Format Specification

### RFC 9562 UUIDv8 Structure

UUIDv8 is defined by RFC 9562 as a custom format with version bits `0b1000` (8) and variant bits `0b10` (RFC 9562 variant).

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤
│                unix_ts_ms (48 bits)                           │
├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤
│    hlc_counter (12 bits)      │ ver=8 │      subsec (12)      │
├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤
│v=10│    node_id (16 bits)    │        random (44 bits)       │
├─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┤

Field Layout (128 bits total):
  Bits   0-47  : unix_ts_ms     (48) - Milliseconds since Unix epoch
  Bits  48-59  : hlc_counter    (12) - Hybrid logical clock counter
  Bits  60-63  : version        (4)  - RFC 9562 version = 0b1000 (8)
  Bits  64-75  : subsec_precision(12)- Sub-millisecond precision (microseconds)
  Bits  76-77  : variant        (2)  - RFC 9562 variant = 0b10
  Bits  78-93  : node_id        (16) - Cluster node identifier
  Bits  94-127 : random         (34) - Cryptographic entropy
```

### Field Descriptions

#### 1. `unix_ts_ms` (48 bits)

**Purpose**: Physical wall clock timestamp in milliseconds since Unix epoch (1970-01-01 00:00:00 UTC).

**Properties**:
- Provides time-ordering for UUIDs (sortable by time)
- 48 bits = 281,474,976,710,655 milliseconds ≈ 8,925 years from epoch
- Overflow date: ~10,895 CE (no practical concern)
- Monotonic guarantee: Must never decrease on same node

**Update Rules**:
- If `current_wall_clock >= last_ts_ms`: Use `current_wall_clock`, reset `hlc_counter` to 0
- If `current_wall_clock < last_ts_ms`: Use `last_ts_ms`, increment `hlc_counter`

#### 2. `hlc_counter` (12 bits)

**Purpose**: Hybrid Logical Clock counter for causal ordering when physical clocks are equal or skewed.

**Properties**:
- 12 bits = 4,096 possible values (0-4095)
- Incremented when:
  - Multiple events occur in same millisecond on same node
  - Receiving event with timestamp ≥ local clock (causal increment)
  - Clock skew detected (physical clock went backward)

**Overflow Handling**:
- If counter reaches 4095 and needs increment: **Stall for 1ms** to advance `unix_ts_ms`
- Alternative: Reduce precision to 1ms (accept timestamp collision)
- Detection: Log warning if counter exceeds threshold (e.g., 3000) → indicates clock issues

**Why 12 bits?**
- 4096 events/ms = 4M events/sec per node (sufficient for OLTP)
- Fits within standard UUID layout (compatible with RFC 9562)
- Larger counter (16 bits) reduces entropy, increasing collision risk

#### 3. `version` (4 bits)

**Value**: `0b1000` (8) - RFC 9562 UUIDv8 (custom implementation-specific format)

**Immutable**: Always set to 8 for UUIDv8-HLC identifiers.

#### 4. `subsec_precision` (12 bits)

**Purpose**: Sub-millisecond timestamp precision (microseconds within current millisecond).

**Properties**:
- 12 bits = 4,096 values (0-4095 microseconds within millisecond)
- Provides finer-grained ordering when `unix_ts_ms` and `hlc_counter` are equal
- Optional: Can be set to 0 if microsecond precision not available

**Use Case**:
- High-frequency trading systems
- Event sourcing with microsecond-level event ordering
- Reduces `hlc_counter` churn in high-throughput scenarios

#### 5. `variant` (2 bits)

**Value**: `0b10` - RFC 9562 variant (standard UUID variant bits)

**Immutable**: Always set to `0b10` for RFC 9562 compliance.

#### 6. `node_id` (16 bits)

**Purpose**: Unique cluster node identifier to prevent collisions across nodes.

**Properties**:
- 16 bits = 65,536 possible node IDs (0-65535)
- Assigned during cluster join (SBCLUSTER-02 Membership)
- Must be unique within cluster (enforced by control plane)
- Persisted in node configuration (survives restarts)

**Assignment Strategy**:
- Control plane (Raft) assigns node IDs sequentially
- Node ID reclaimed after node leaves cluster (grace period: 7 days)
- Node ID 0 reserved for system/bootstrap operations

#### 7. `random` (34 bits)

**Purpose**: Cryptographic entropy to prevent collisions when all other fields are equal.

**Properties**:
- 34 bits = 17,179,869,184 possible values (~17 billion)
- Generated via cryptographically secure PRNG (e.g., `/dev/urandom`, `crypto::rand`)
- Collision probability: ~1 in 17B for same (timestamp, counter, node_id)

**Why 34 bits?**
- Balance between collision resistance and field space
- More bits = fewer bits for `node_id` (need 16 bits for 64K nodes)
- UUID birthday paradox: ~130K UUIDs before 0.01% collision chance (acceptable)

---

## Hybrid Logical Clock Design

### HLC Theory

Hybrid Logical Clocks (Kulkarni et al., 2014) combine physical timestamps with logical counters to provide:

1. **Causal Ordering**: Event A → Event B implies `hlc(A) < hlc(B)` (causality preserved)
2. **Clock Skew Resilience**: Logical counter compensates for physical clock drift
3. **Time Proximity**: HLC stays close to physical time (useful for queries)

**Key Invariant**: If event A causally precedes event B, then `hlc(A) < hlc(B)`.

### HLC Update Rules

#### Local Event Generation

When generating a new UUIDv8-HLC for a local event (e.g., INSERT, UPDATE):

```
local_timestamp = current_wall_clock_ms()
last_hlc = node.last_generated_hlc

IF local_timestamp > last_hlc.timestamp:
    new_hlc.timestamp = local_timestamp
    new_hlc.counter = 0
ELSE IF local_timestamp == last_hlc.timestamp:
    new_hlc.timestamp = last_hlc.timestamp
    new_hlc.counter = last_hlc.counter + 1
ELSE:  // Clock went backward (NTP adjustment, leap second, etc.)
    new_hlc.timestamp = last_hlc.timestamp
    new_hlc.counter = last_hlc.counter + 1
    LOG_WARNING("Clock skew detected: physical clock moved backward")
END IF

// Check counter overflow
IF new_hlc.counter > 4095:
    STALL_FOR(1ms)  // Wait for physical clock to advance
    new_hlc.timestamp = current_wall_clock_ms()
    new_hlc.counter = 0
END IF

node.last_generated_hlc = new_hlc
RETURN new_hlc
```

#### Remote Event Receipt

When receiving a replication message with embedded HLC (e.g., gossip, quorum writes):

```
remote_hlc = extract_hlc_from_message()
local_timestamp = current_wall_clock_ms()
last_hlc = node.last_generated_hlc

new_timestamp = MAX(local_timestamp, remote_hlc.timestamp, last_hlc.timestamp)

IF new_timestamp == last_hlc.timestamp AND new_timestamp == remote_hlc.timestamp:
    new_counter = MAX(last_hlc.counter, remote_hlc.counter) + 1
ELSE IF new_timestamp == last_hlc.timestamp:
    new_counter = last_hlc.counter + 1
ELSE IF new_timestamp == remote_hlc.timestamp:
    new_counter = remote_hlc.counter + 1
ELSE:
    new_counter = 0
END IF

// Check counter overflow
IF new_counter > 4095:
    STALL_FOR(1ms)
    new_timestamp = current_wall_clock_ms()
    new_counter = 0
END IF

node.last_generated_hlc = (new_timestamp, new_counter)
RETURN (new_timestamp, new_counter)
```

### HLC Properties

**Monotonicity**: HLC values on a single node are strictly increasing.

**Proof**:
- If `local_timestamp > last_hlc.timestamp`: New HLC > last HLC (timestamp increased)
- If `local_timestamp == last_hlc.timestamp`: New HLC > last HLC (counter incremented)
- If `local_timestamp < last_hlc.timestamp`: Use `last_hlc.timestamp`, increment counter → New HLC > last HLC

**Causality**: If event A happens-before event B, then `hlc(A) < hlc(B)`.

**Proof**:
- Local causality: A → B on same node → B sees A's HLC → B's HLC ≥ A's HLC + increment
- Remote causality: A on node X → message to node Y → B on node Y → B's HLC ≥ A's HLC (via remote receipt rule)

**Proximity to Physical Time**: HLC timestamp stays within clock skew bounds of physical time.

**Bound**: `|hlc.timestamp - physical_time| ≤ ε` where ε = maximum clock skew in cluster (typically <100ms with NTP).

---

## Generation Algorithm

### Implementation: `generateUuidV8WithHLC()`

```cpp
namespace scratchbird::core {

struct UuidV8HLC {
    std::array<uint8_t, 16> bytes{};

    // Comparison operators (for time-ordering)
    auto operator<(const UuidV8HLC& other) const -> bool;
    auto operator==(const UuidV8HLC& other) const -> bool;

    // Field extraction
    auto getTimestamp() const -> uint64_t;     // unix_ts_ms (48 bits)
    auto getHlcCounter() const -> uint16_t;    // hlc_counter (12 bits)
    auto getSubsecPrecision() const -> uint16_t; // subsec (12 bits)
    auto getNodeId() const -> uint16_t;        // node_id (16 bits)
    auto getRandom() const -> uint64_t;        // random (34 bits, zero-extended to 64)
};

class UuidV8Generator {
public:
    UuidV8Generator(uint16_t node_id);

    // Generate new UUIDv8-HLC for local event
    auto generate() -> UuidV8HLC;

    // Update HLC after receiving remote event
    auto updateHlcFromRemote(uint64_t remote_ts_ms, uint16_t remote_counter) -> void;

private:
    uint16_t node_id_;                // Cluster node ID (from SBCLUSTER-02)
    std::atomic<uint64_t> last_ts_ms_{0};
    std::atomic<uint16_t> last_counter_{0};
    std::mutex generation_mutex_;     // Serialize UUID generation on same node
    std::random_device rd_;
    std::mt19937_64 rng_;

    auto getCurrentWallClockMs() -> uint64_t;
    auto getCurrentSubsecMicros() -> uint16_t;
    auto generateRandomBits() -> uint64_t;  // 34 bits of entropy
};

} // namespace scratchbird::core
```

### Generation Steps

```cpp
auto UuidV8Generator::generate() -> UuidV8HLC {
    std::lock_guard<std::mutex> lock(generation_mutex_);

    // 1. Get current physical time
    uint64_t wall_ts_ms = getCurrentWallClockMs();
    uint16_t subsec_us = getCurrentSubsecMicros();

    // 2. Load last HLC values
    uint64_t last_ts = last_ts_ms_.load(std::memory_order_relaxed);
    uint16_t last_cnt = last_counter_.load(std::memory_order_relaxed);

    // 3. Apply HLC update rules
    uint64_t new_ts;
    uint16_t new_counter;

    if (wall_ts_ms > last_ts) {
        // Physical clock advanced
        new_ts = wall_ts_ms;
        new_counter = 0;
    } else if (wall_ts_ms == last_ts) {
        // Same millisecond, increment counter
        new_ts = last_ts;
        new_counter = last_cnt + 1;
    } else {
        // Clock went backward (NTP adjustment, etc.)
        new_ts = last_ts;
        new_counter = last_cnt + 1;
        LOG_WARNING("Clock skew detected: wall={} < last={}", wall_ts_ms, last_ts);
    }

    // 4. Check counter overflow
    if (new_counter > 4095) {
        // Stall for 1ms to advance physical clock
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        new_ts = getCurrentWallClockMs();
        new_counter = 0;
        subsec_us = getCurrentSubsecMicros();
        LOG_WARNING("HLC counter overflow, stalled 1ms: ts={}", new_ts);
    }

    // 5. Store new HLC
    last_ts_ms_.store(new_ts, std::memory_order_relaxed);
    last_counter_.store(new_counter, std::memory_order_relaxed);

    // 6. Build UUID bytes
    UuidV8HLC uuid;

    // Bits 0-47: unix_ts_ms (48 bits)
    uuid.bytes[0] = (new_ts >> 40) & 0xFF;
    uuid.bytes[1] = (new_ts >> 32) & 0xFF;
    uuid.bytes[2] = (new_ts >> 24) & 0xFF;
    uuid.bytes[3] = (new_ts >> 16) & 0xFF;
    uuid.bytes[4] = (new_ts >> 8) & 0xFF;
    uuid.bytes[5] = new_ts & 0xFF;

    // Bits 48-59: hlc_counter (12 bits)
    uuid.bytes[6] = (new_counter >> 4) & 0xFF;
    uuid.bytes[7] = ((new_counter & 0x0F) << 4) | 0x08;  // Version 8 in lower nibble

    // Bits 64-75: subsec_precision (12 bits, upper 12 of bytes[8-9])
    uuid.bytes[8] = 0x80 | ((subsec_us >> 6) & 0x3F);  // Variant bits (0b10) + upper 6 bits of subsec
    uuid.bytes[9] = ((subsec_us & 0x3F) << 2);

    // Bits 78-93: node_id (16 bits, lower 2 of byte[9] + byte[10])
    uuid.bytes[9] |= (node_id_ >> 14) & 0x03;
    uuid.bytes[10] = (node_id_ >> 6) & 0xFF;
    uuid.bytes[11] = ((node_id_ & 0x3F) << 2);

    // Bits 94-127: random (34 bits)
    uint64_t random_bits = generateRandomBits();  // 34 bits
    uuid.bytes[11] |= (random_bits >> 32) & 0x03;
    uuid.bytes[12] = (random_bits >> 24) & 0xFF;
    uuid.bytes[13] = (random_bits >> 16) & 0xFF;
    uuid.bytes[14] = (random_bits >> 8) & 0xFF;
    uuid.bytes[15] = random_bits & 0xFF;

    return uuid;
}
```

### Remote HLC Update

```cpp
auto UuidV8Generator::updateHlcFromRemote(
    uint64_t remote_ts_ms,
    uint16_t remote_counter
) -> void {
    std::lock_guard<std::mutex> lock(generation_mutex_);

    uint64_t wall_ts_ms = getCurrentWallClockMs();
    uint64_t last_ts = last_ts_ms_.load(std::memory_order_relaxed);
    uint16_t last_cnt = last_counter_.load(std::memory_order_relaxed);

    // Take max of all timestamps
    uint64_t new_ts = std::max({wall_ts_ms, remote_ts_ms, last_ts});
    uint16_t new_counter;

    // Determine counter value
    if (new_ts == last_ts && new_ts == remote_ts_ms) {
        new_counter = std::max(last_cnt, remote_counter) + 1;
    } else if (new_ts == last_ts) {
        new_counter = last_cnt + 1;
    } else if (new_ts == remote_ts_ms) {
        new_counter = remote_counter + 1;
    } else {
        new_counter = 0;
    }

    // Check overflow
    if (new_counter > 4095) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        new_ts = getCurrentWallClockMs();
        new_counter = 0;
    }

    last_ts_ms_.store(new_ts, std::memory_order_relaxed);
    last_counter_.store(new_counter, std::memory_order_relaxed);
}
```

---

## Conflict Resolution

### Last-Write-Wins (LWW) Semantics

UUIDv8-HLC enables deterministic conflict resolution for concurrent writes to the same row across multiple nodes.

**Rule**: For two versions of a row with UUIDs `uuid_a` and `uuid_b`, the winner is determined by:

```
IF uuid_a.timestamp > uuid_b.timestamp:
    WINNER = uuid_a
ELSE IF uuid_a.timestamp < uuid_b.timestamp:
    WINNER = uuid_b
ELSE:  // Timestamps equal
    IF uuid_a.hlc_counter > uuid_b.hlc_counter:
        WINNER = uuid_a
    ELSE IF uuid_a.hlc_counter < uuid_b.hlc_counter:
        WINNER = uuid_b
    ELSE:  // Counter equal (rare: subsec + node_id + random differ)
        IF uuid_a.subsec_precision > uuid_b.subsec_precision:
            WINNER = uuid_a
        ELSE IF uuid_a.subsec_precision < uuid_b.subsec_precision:
            WINNER = uuid_b
        ELSE:  // Subsec equal
            IF uuid_a.node_id > uuid_b.node_id:
                WINNER = uuid_a  // Deterministic tiebreak
            ELSE IF uuid_a.node_id < uuid_b.node_id:
                WINNER = uuid_b
            ELSE:  // Same node (impossible in multi-master)
                WINNER = uuid_a.random > uuid_b.random ? uuid_a : uuid_b
            END IF
        END IF
    END IF
END IF
```

**Implementation**:

```cpp
auto UuidV8HLC::operator<(const UuidV8HLC& other) const -> bool {
    uint64_t ts_a = this->getTimestamp();
    uint64_t ts_b = other.getTimestamp();
    if (ts_a != ts_b) return ts_a < ts_b;

    uint16_t cnt_a = this->getHlcCounter();
    uint16_t cnt_b = other.getHlcCounter();
    if (cnt_a != cnt_b) return cnt_a < cnt_b;

    uint16_t subsec_a = this->getSubsecPrecision();
    uint16_t subsec_b = other.getSubsecPrecision();
    if (subsec_a != subsec_b) return subsec_a < subsec_b;

    uint16_t node_a = this->getNodeId();
    uint16_t node_b = other.getNodeId();
    if (node_a != node_b) return node_a < node_b;

    // Final tiebreak: random bits (should never reach here in practice)
    return this->getRandom() < other.getRandom();
}
```

### Conflict Detection

**Scenario**: Two nodes (A, B) concurrently update row with `row_id = X`.

```
Node A: UPDATE users SET name='Alice' WHERE user_id='X'
        → Generates UUIDv8-HLC: uuid_a (version_id)

Node B: UPDATE users SET name='Bob' WHERE user_id='X'
        → Generates UUIDv8-HLC: uuid_b (version_id)
```

**Gossip/Quorum Exchange**:
- Node A learns about `uuid_b` from Node B
- Node B learns about `uuid_a` from Node A

**Resolution**:
- Both nodes apply comparison: `uuid_a < uuid_b` or `uuid_b < uuid_a`
- Winner determined by HLC ordering (causality + clock)
- Loser's version discarded (or moved to conflict log if user-facing resolution needed)

**Example**:

```
uuid_a = {timestamp=1704067200000, counter=5, node_id=1, ...}
uuid_b = {timestamp=1704067200001, counter=0, node_id=2, ...}

Comparison:
  uuid_a.timestamp (1704067200000) < uuid_b.timestamp (1704067200001)
  → uuid_b WINS

Result: Both nodes converge to name='Bob'
```

### Causal Conflicts

HLC ensures that if Node B's write causally depends on Node A's write (e.g., B read A's value before writing), then `hlc(A) < hlc(B)`, and B's write always wins.

**Proof**:
1. Node A writes at `hlc(A) = (t_a, c_a)`
2. Node B receives A's write, updates its HLC: `hlc(B) ≥ (t_a, c_a + 1)`
3. Node B writes with new HLC: `hlc(B') = (t_b, c_b)` where `(t_b, c_b) > (t_a, c_a)`
4. Conflict resolution: `hlc(A) < hlc(B')` → B's write wins

**Outcome**: Causal order preserved (B's write based on A's state → B wins).

---

## Coexistence with UUIDv7

### Design Principle

**UUIDv7 and UUIDv8-HLC coexist as separate, independent identifier types**. Tables can use either format based on replication mode:

| Replication Mode | Recommended UUID Type | Reason |
|------------------|----------------------|--------|
| Mode 1: Logical (Primary-Replica) | UUIDv7 | Single writer, no conflicts |
| Mode 2: Physical (Primary-Replica) | UUIDv7 | Single writer, no conflicts |
| Mode 3: Leaderless Quorum | UUIDv8-HLC | Multi-master, conflict resolution |
| Single-node (no replication) | UUIDv7 | Simplicity, standard compliance |

### Schema Declaration

```sql
-- UUIDv7 table (standard, single-writer)
CREATE TABLE users (
    user_id UUID PRIMARY KEY,  -- Defaults to UUIDv7
    name VARCHAR(100),
    email VARCHAR(200)
) WITH (uuid_type = 'v7');

-- UUIDv8-HLC table (multi-master, conflict resolution)
CREATE TABLE sensor_data (
    sensor_id UUID PRIMARY KEY,  -- UUIDv8-HLC for leaderless writes
    timestamp BIGINT,
    value DOUBLE
) WITH (uuid_type = 'v8_hlc', replication_mode = 'leaderless_quorum');
```

### Migration Path

**No Breaking Changes**: Existing tables with UUIDv7 continue to work unchanged.

**Opt-In for UUIDv8-HLC**:

```sql
-- Alter existing table to use UUIDv8-HLC (requires rewrite)
ALTER TABLE sensor_data SET uuid_type = 'v8_hlc';

-- Dual-format transition (Beta feature)
ALTER TABLE users ADD COLUMN hlc_version_id UUID GENERATED ALWAYS AS (uuidv8_from_uuidv7(user_id));
```

### Type System Integration

```cpp
namespace scratchbird::core {

enum class UuidType : uint8_t {
    V7_STANDARD = 7,    // RFC 9562 UUIDv7
    V8_HLC = 8          // RFC 9562 UUIDv8 with HLC
};

// Unified UUID interface (std::variant)
using UUID = std::variant<UuidV7Bytes, UuidV8HLC>;

auto getUuidType(const UUID& uuid) -> UuidType;
auto compareUuids(const UUID& a, const UUID& b) -> std::strong_ordering;

} // namespace scratchbird::core
```

### Catalog Storage

**`sys_tables` table extension**:

```sql
CREATE TABLE sys_tables (
    table_id UUID PRIMARY KEY,
    table_name VARCHAR(255),
    uuid_type VARCHAR(10) DEFAULT 'v7',  -- 'v7' or 'v8_hlc'
    replication_mode VARCHAR(20) DEFAULT 'primary_replica',
    ...
);
```

**Enforcement**:
- Parser checks `uuid_type` during INSERT/UPDATE to generate correct UUID format
- Replication layer checks `uuid_type` to apply correct conflict resolution
- UUIDv7 tables in leaderless mode → ERROR (require UUIDv8-HLC)

---

## API Design

### Core Functions

```cpp
namespace scratchbird::core {

// UUIDv8-HLC generation
auto generateUuidV8WithHLC(uint16_t node_id) -> UuidV8HLC;

// Extract HLC components
auto extractHlc(const UuidV8HLC& uuid) -> std::pair<uint64_t, uint16_t>;  // (timestamp, counter)

// Update local HLC after remote event
auto updateHlcFromRemote(uint64_t remote_ts_ms, uint16_t remote_counter) -> void;

// Conflict resolution
auto resolveConflict(const UuidV8HLC& uuid_a, const UuidV8HLC& uuid_b) -> UuidV8HLC;

// Conversion (Beta feature)
auto uuidv7_to_uuidv8(const UuidV7Bytes& v7) -> UuidV8HLC;  // Migrate identifier

// Comparison
auto compareUuids(const UUID& a, const UUID& b) -> std::strong_ordering;

} // namespace scratchbird::core
```

### SQL Functions

```sql
-- Generate UUIDv8-HLC
SELECT gen_uuidv8_hlc();  -- Uses local node_id

-- Extract HLC timestamp
SELECT uuidv8_timestamp(sensor_id) FROM sensor_data;

-- Extract HLC counter
SELECT uuidv8_counter(sensor_id) FROM sensor_data;

-- Check UUID version
SELECT uuid_version(user_id) FROM users;  -- Returns 7 or 8
```

### SBLR Opcode Support

**New opcodes for UUIDv8-HLC**:

```cpp
enum class Opcode : uint8_t {
    // Existing UUIDv7 opcodes
    GEN_UUID_V7 = 0x8A,

    // New UUIDv8-HLC opcodes (Beta)
    GEN_UUID_V8_HLC = 0x8B,          // Generate UUIDv8 with HLC
    UUID_V8_TIMESTAMP = 0x8C,        // Extract timestamp from UUIDv8
    UUID_V8_COUNTER = 0x8D,          // Extract HLC counter
    UUID_V8_NODE_ID = 0x8E,          // Extract node_id
    UUID_COMPARE = 0x8F,             // Compare two UUIDs (any version)
};
```

---

## Testing Requirements

### Unit Tests

**Monotonicity Test**:

```cpp
TEST(UuidV8HLC, Monotonicity) {
    UuidV8Generator gen(1);  // Node ID = 1

    UuidV8HLC prev = gen.generate();
    for (int i = 0; i < 10000; ++i) {
        UuidV8HLC curr = gen.generate();
        ASSERT_LT(prev, curr) << "UUIDs not monotonic at iteration " << i;
        prev = curr;
    }
}
```

**Clock Skew Test**:

```cpp
TEST(UuidV8HLC, ClockSkewHandling) {
    UuidV8Generator gen(1);

    // Generate UUID at t=1000ms
    auto uuid1 = gen.generate();  // ts=1000, counter=0

    // Simulate clock going backward (NTP adjustment)
    // Force wall clock to return 999ms
    auto uuid2 = gen.generate();  // Should use ts=1000, counter=1

    ASSERT_EQ(uuid1.getTimestamp(), uuid2.getTimestamp());
    ASSERT_EQ(uuid2.getHlcCounter(), uuid1.getHlcCounter() + 1);
}
```

**Conflict Resolution Test**:

```cpp
TEST(UuidV8HLC, ConflictResolution) {
    UuidV8HLC uuid_a{/* ts=1000, counter=5, node=1 */};
    UuidV8HLC uuid_b{/* ts=1000, counter=10, node=2 */};

    ASSERT_LT(uuid_a, uuid_b);  // uuid_b wins (higher counter)

    auto winner = resolveConflict(uuid_a, uuid_b);
    ASSERT_EQ(winner, uuid_b);
}
```

### Integration Tests

**Multi-Node HLC Synchronization**:

```cpp
TEST(UuidV8HLCIntegration, MultiNodeHlcSync) {
    UuidV8Generator gen_a(1);
    UuidV8Generator gen_b(2);

    // Node A generates UUID
    auto uuid_a = gen_a.generate();  // ts=1000, counter=0

    // Node B receives A's HLC
    gen_b.updateHlcFromRemote(uuid_a.getTimestamp(), uuid_a.getHlcCounter());

    // Node B generates UUID (should be > A's)
    auto uuid_b = gen_b.generate();  // ts=1000, counter=1 (or ts=1001, counter=0)

    ASSERT_LT(uuid_a, uuid_b);  // Causal order preserved
}
```

**Concurrent Write Conflict**:

```cpp
TEST(UuidV8HLCIntegration, ConcurrentWriteConflict) {
    Database db;
    db.execute("CREATE TABLE users (id UUID PRIMARY KEY, name VARCHAR(100)) WITH (uuid_type='v8_hlc')");

    // Node A: INSERT user
    auto uuid_a = db.execute("INSERT INTO users (id, name) VALUES (gen_uuidv8_hlc(), 'Alice')");

    // Node B: INSERT same user (concurrent, before replication)
    auto uuid_b = db.execute("INSERT INTO users (id, name) VALUES (gen_uuidv8_hlc(), 'Bob')");

    // Trigger anti-entropy (gossip)
    db.performAntiEntropy();

    // Verify convergence: both nodes have same winner
    auto result_a = db.query("SELECT name FROM users WHERE id=?", uuid_a);
    auto result_b = db.query("SELECT name FROM users WHERE id=?", uuid_b);

    // One winner (higher HLC), one loser discarded
    ASSERT_TRUE((result_a.empty() && !result_b.empty()) || (!result_a.empty() && result_b.empty()));
}
```

### Performance Tests

**Throughput Benchmark**:

```cpp
BENCHMARK(UuidV8HLCGeneration) {
    UuidV8Generator gen(1);
    for (auto _ : state) {
        benchmark::DoNotOptimize(gen.generate());
    }
}
// Target: > 1M UUIDs/sec per thread
```

**Counter Overflow Test**:

```cpp
TEST(UuidV8HLC, CounterOverflowHandling) {
    UuidV8Generator gen(1);

    // Generate 5000 UUIDs in same millisecond (exceeds 4095 counter limit)
    auto start_ts = gen.generate().getTimestamp();

    for (int i = 0; i < 5000; ++i) {
        auto uuid = gen.generate();
        ASSERT_TRUE(uuid.getHlcCounter() <= 4095);  // Never exceeds limit
    }

    // Verify timestamp advanced (stall occurred)
    auto end_ts = gen.generate().getTimestamp();
    ASSERT_GT(end_ts, start_ts);  // Timestamp moved forward
}
```

---

## Migration and Adoption

### Phase 1: Foundation (Weeks 1-2)

**Implementation**:
- [ ] `UuidV8HLC` struct with 128-bit byte array
- [ ] `UuidV8Generator` class with HLC state
- [ ] `generateUuidV8WithHLC()` function
- [ ] `updateHlcFromRemote()` function
- [ ] Unit tests (monotonicity, clock skew, overflow)

**Deliverable**: UUIDv8-HLC generation library, 100% test coverage

### Phase 2: Catalog Integration (Weeks 3-4)

**Implementation**:
- [ ] Add `uuid_type` column to `sys_tables`
- [ ] Parser support for `WITH (uuid_type='v8_hlc')`
- [ ] SBLR opcodes: `GEN_UUID_V8_HLC`, `UUID_V8_TIMESTAMP`, `UUID_V8_COUNTER`
- [ ] Executor integration for UUIDv8 generation
- [ ] Integration tests (multi-table, replication modes)

**Deliverable**: Tables can declare UUIDv8-HLC type, system enforces semantics

### Phase 3: Conflict Resolution (Weeks 5-6)

**Implementation**:
- [ ] `resolveConflict()` function (LWW semantics)
- [ ] Anti-entropy integration (Merkle tree comparison with HLC)
- [ ] Replication layer: Detect conflicts, apply resolution
- [ ] Conflict log (optional: store discarded versions for user review)
- [ ] Performance tests (conflict resolution overhead)

**Deliverable**: Multi-master writes converge deterministically

### Phase 4: Hardening (Weeks 7-8)

**Implementation**:
- [ ] Clock skew detection and alerting
- [ ] Counter overflow monitoring (Prometheus metrics)
- [ ] NTP synchronization validation
- [ ] Chaos testing (clock drift, network partitions)
- [ ] Documentation (user guide, operational runbook)

**Deliverable**: Production-ready UUIDv8-HLC system with observability

### Adoption Strategy

**Single-Node Users**: Continue using UUIDv7 (no changes required)

**Primary-Replica Users**: Continue using UUIDv7 (Mode 1/2 replication)

**Leaderless Quorum Users** (Beta):
1. Create new tables with `uuid_type='v8_hlc'`
2. Or migrate existing tables:
   ```sql
   ALTER TABLE sensor_data SET uuid_type='v8_hlc';
   ```
3. Configure replication mode:
   ```sql
   ALTER TABLE sensor_data SET replication_mode='leaderless_quorum';
   ```

**Educational Guidance**:
- Documentation: When to use UUIDv7 vs UUIDv8-HLC
- Trade-offs: UUIDv8-HLC adds 12-bit counter overhead, requires NTP
- Best practices: Use UUIDv8-HLC only for tables with multi-master writes

---

## Appendix: HLC Research References

### Primary Sources

1. **"Logical Physical Clocks and Consistent Snapshots in Globally Distributed Databases"**
   Kulkarni et al., 2014
   [https://cse.buffalo.edu/tech-reports/2014-04.pdf](https://cse.buffalo.edu/tech-reports/2014-04.pdf)

2. **RFC 9562: Universally Unique IDentifiers (UUIDs)**
   [https://www.rfc-editor.org/rfc/rfc9562.html](https://www.rfc-editor.org/rfc/rfc9562.html)

### Industry Implementations

- **CockroachDB**: Uses HLC for distributed transactions ([Docs](https://www.cockroachlabs.com/docs/stable/architecture/transaction-layer.html#hybrid-logical-clocks))
- **YugabyteDB**: HLC in DocDB for multi-region consistency ([Docs](https://docs.yugabyte.com/preview/architecture/transactions/transactions-overview/))
- **MongoDB**: Hybrid logical clock for causal consistency ([Docs](https://www.mongodb.com/docs/manual/core/causal-consistency-read-write-concerns/))

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** SBCLUSTER-02 (Node IDs), Existing UUIDv7 implementation

---

**End of Document 01**
