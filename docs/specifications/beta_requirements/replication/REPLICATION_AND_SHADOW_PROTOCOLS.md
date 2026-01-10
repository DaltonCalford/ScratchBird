# ScratchBird Replication and Shadow Database Specifications

**Scope Note:** ScratchBird uses MGA and does not use WAL for recovery. References to WAL here describe an optional write-after log stream for replication/PITR (post-gold).

## Shadow Database (Physical/Block-Level Replication)

### Overview
Shadow databases are **exact physical copies** maintained through page-level replication. They provide near-instant failover capability with zero data loss.

### Shadow Database Characteristics
```
- Read-only until promoted
- Physically identical to primary (same page layout, same file structure)
- Cannot accept client connections (except monitoring)
- Sub-second lag from primary
- Automatic failover capability
- No logical interpretation of changes
```

### Shadow Page Header Extension
```
Shadow-Specific Fields (in reserved area):
Offset  Size  Field
56      8     Source Database UUID
64      8     Last Applied LSN
72      8     Shadow Creation Timestamp
80      4     Shadow State (RECEIVING/PROMOTING/PROMOTED/FAILED)
84      4     Replication Lag (microseconds)
```

### Shadow Database States
```
INITIALIZING:
  - Creating shadow structure
  - Copying initial database snapshot
  - Not accepting page updates yet

RECEIVING:
  - Actively receiving page updates
  - Applying changes at page level
  - No client connections allowed

PROMOTING:
  - Transitioning to primary
  - Finishing pending page applications
  - Preparing to accept connections

PROMOTED:
  - Now a primary database
  - Accepting client connections
  - Can have its own shadows

FAILED:
  - Replication broken
  - Requires resynchronization
  - May need complete rebuild
```

### Shadow Replication Protocol

#### Dual-Channel Replication
```
Channel 1: Page-Level Replication (Primary)
  - Bulk data transfer
  - Batched pages for efficiency
  - May lag during high load
  - Typically every few seconds

Channel 2: Write-after log (WAL) Streaming (Gap Protection)
  - Continuous streaming via Kafka/MQ
  - Sub-second latency
  - Smaller packets (not constrained by page size)
  - Captures transactions between page flushes
```

#### Enhanced Protocol Flow
```
1. Page Change Detection:
   Primary -> Dirty page -> Queue for replication
   Primary -> write-after log (WAL) record -> Stream to Kafka immediately

2. Dual Transmission:
   Page Channel (TCP):
     [Header: 32 bytes]
       - Magic: "SHDW"
       - Source DB UUID
       - LSN range covered
       - Page Count
       - Checksum
     [Page Data: N * page_size]
       - Raw page bytes
       - Page-level checksum
   
   Write-after Log (WAL) Channel (Kafka):
     [Write-after Log (WAL) Packet: Variable size]
       - Magic: "SWAL"
       - Source DB UUID
       - LSN
       - Transaction ID
      - Write-after log (WAL) record type
      - Write-after log (WAL) payload (can be < 1KB)
       - Checksum

3. Shadow Processing:
   Page Apply: Write to exact file position
   Write-after Log (WAL) Buffer: Store in memory/persistent queue
   
4. Gap Detection and Recovery:
   If (WAL_LSN > Page_LSN):
     - Pages are behind
     - Keep write-after log (WAL) records for replay
   If (Page_LSN >= WAL_LSN):
     - Pages are current
     - Discard old write-after log (WAL) records

5. Promotion with Gap Recovery:
   - Stop page reception
   - Apply all buffered write-after log (WAL) records
   - Verify no gaps in LSN sequence
   - Promote to primary
```

#### Write-after log (WAL) Streaming via Kafka

##### Kafka Topic Structure
```
Topics:
  scratchbird.wal.[database_uuid]
    - Partition by table/segment for parallelism
    - Retention: 24-72 hours
    - Compression: LZ4/ZSTD
    - Replication factor: 3

Message Format:
  Key: [DB_UUID]:[LSN]
  Headers:
    - source_host
    - timestamp
    - transaction_id
    - table_uuid (if applicable)
  Value: Serialized write-after log (WAL) record
```

##### Write-after log (WAL) Packet Types
```
1. Transaction Boundaries:
   - BEGIN (xid, timestamp)
   - COMMIT (xid, timestamp, checksum)
   - ROLLBACK (xid, reason)

2. Data Modifications:
   - INSERT (table, tuple_data)
   - UPDATE (table, old_tuple, new_tuple)
   - DELETE (table, old_tuple)

3. Structural Changes:
   - CREATE_TABLE
   - ALTER_TABLE
   - CREATE_INDEX
   - DROP_OBJECT

4. Checkpoints:
   - CHECKPOINT (lsn, timestamp)
   - Allows shadow to verify consistency
```

#### Failure Scenarios and Recovery

##### Scenario 1: Primary Crash with Unflushed Pages
```
Timeline:
  T1: Transaction commits, write-after log (WAL) written
  T2: Write-after log (WAL) streamed to Kafka
  T3: Shadow receives write-after log (WAL) via Kafka
  T4: Page not yet flushed to disk
  T5: Primary crashes
  T6: Page replication never happens

Recovery:
  - Shadow has write-after log (WAL) records via Kafka
  - On promotion, replay write-after log (WAL) from last page LSN
  - Reconstructs missing page changes
  - Zero data loss achieved
```

##### Scenario 2: Network Partition
```
Timeline:
  T1: Network partition begins
  T2: Page replication fails (TCP timeout)
  T3: Write-after log (WAL) continues via Kafka (different path/broker)
  T4: Partition heals
  T5: Page replication resumes

Recovery:
  - Write-after log (WAL) records fill the gap
  - Page replication catches up
  - No promotion needed
```

##### Scenario 3: Kafka Failure
```
Fallback:
  - Direct write-after log (WAL) streaming over TCP
  - Store-and-forward to local disk
  - Replay when Kafka recovers
  - Page replication continues normally
```

### Shadow File Structure
```
shadow.sbd          - Identical to primary.sbd
shadow.sbd.1        - Identical to primary.sbd.1
shadow.sbd.shadow   - Shadow-specific metadata
  - Source connection info
  - Replication state
  - Promotion history
  - Lag statistics
```

#### Benefits of Dual-Channel Approach

##### Zero Data Loss (RPO=0)
```
Without Write-after Log (WAL) Streaming:
  - Last page flush: LSN 1000
  - Current LSN: 1050
  - Potential loss: 50 transactions

With Write-after Log (WAL) Streaming:
  - Last page flush: LSN 1000
  - Write-after log (WAL) buffer has: LSN 1001-1050
  - Data loss: 0 transactions
```

##### Improved Recovery Time (RTO)
```
Traditional Shadow:
  - Wait for final page flush
  - May timeout waiting
  - Manual intervention needed

Dual-Channel Shadow:
  - Apply buffered write-after log (WAL) immediately
  - Automatic gap detection
  - Self-healing on promotion
```

##### Network Efficiency
```
Page Replication:
  - 8KB minimum transfer (even for 1 byte change)
  - Batched for efficiency
  - Higher latency acceptable

Write-after Log (WAL) Streaming:
  - Single row update: ~100 bytes
  - Immediate transmission
  - Optimized for small changes
```

#### Configuration Examples

##### High-Performance Configuration
```sql
CREATE SHADOW ha_shadow
  AT 'shadow1.local:5432'
  WITH (
    page_channel = 'TCP',
    page_batch_size = 100,        -- Send 100 pages at once
    page_interval = '1s',         -- Flush pages every second
    
    wal_channel = 'KAFKA',
    kafka_brokers = 'kafka1:9092,kafka2:9092',
    kafka_topic = 'scratchbird.wal.prod',
    kafka_compression = 'LZ4',
    wal_buffer_size = '1GB',      -- Buffer up to 1GB of write-after log (WAL)
    
    sync_mode = 'ASYNC',          -- Don't wait for shadow ACK
    max_lag = '10MB'              -- Alert if > 10MB behind
  );
```

##### Disaster Recovery Configuration
```sql
CREATE SHADOW dr_shadow
  AT 'dr-site.remote:5432'
  WITH (
    page_channel = 'TCP_COMPRESSED',
    page_compression = 'ZSTD',    -- Compress for WAN
    page_batch_size = 1000,       -- Large batches for WAN
    page_interval = '10s',        -- Less frequent over WAN
    
    wal_channel = 'KAFKA',
    kafka_brokers = 'dr-kafka:9092',
    kafka_topic = 'scratchbird.wal.dr',
    kafka_retention = '72h',      -- Keep 3 days of write-after log (WAL)
    
    sync_mode = 'ASYNC',
    max_lag = '100MB'             -- Tolerate more lag for DR
  );
```

##### Synchronous Shadow (Zero Data Loss)
```sql
CREATE SHADOW sync_shadow
  AT 'shadow2.local:5432'
  WITH (
    page_channel = 'TCP',
    page_interval = '100ms',      -- Aggressive page flushing
    
    wal_channel = 'DIRECT_TCP',   -- Skip Kafka for low latency
    wal_sync = 'SYNC',            -- Wait for write-after log (WAL) acknowledgment
    
    sync_mode = 'SYNC',           -- Transaction waits for shadow
    sync_timeout = '5s',          -- Fail transaction if no ACK
    sync_fallback = 'ASYNC'       -- Fall back to async if timeout
  );
```

### Shadow Operations

#### Creating a Shadow
```sql
-- On primary
CREATE SHADOW shadow1 
  AT 'server2.domain.com:5432'
  AUTHENTICATION KEY 'encrypted_key'
  COMPRESSION ZSTD
  ASYNC;  -- or SYNC for synchronous replication

-- On shadow server (command line)
scratchbird --shadow-receive \
  --from primary.domain.com:5432 \
  --path /data/shadow/db.sbd \
  --auth-key encrypted_key
```

#### Monitoring Shadow
```sql
-- On primary
SELECT * FROM sys.shadow_databases;
SELECT * FROM sys.shadow_replication_status;

-- Returns:
-- shadow_name, target_host, lag_bytes, lag_time, state, last_lsn
```

#### Promoting Shadow
```sql
-- On shadow (requires physical/console access)
ALTER DATABASE PROMOTE FROM SHADOW;

-- Or emergency promotion (if primary is dead)
ALTER DATABASE FORCE PROMOTE FROM SHADOW;
```

### Multiple Shadow Support
```
Primary Database
    ├── Shadow 1 (same datacenter, synchronous)
    ├── Shadow 2 (remote DC, asynchronous) 
    └── Shadow 3 (DR site, asynchronous)

Cascade Shadows:
Primary -> Shadow 1 -> Shadow 1.1
                    -> Shadow 1.2
```

## Logical Replication (Table-Level)

### Overview
Logical replication operates at the SQL/row level, allowing:
- Selective table replication
- Cross-version replication
- Bi-directional replication
- Multi-master configurations
- Different schemas between databases

### Logical Replication Types

#### 1. One-Way Replication (Master-Slave)
```sql
-- On source
CREATE PUBLICATION sales_pub 
  FOR TABLE orders, customers, products
  WITH (publish = 'insert,update,delete');

-- On target
CREATE SUBSCRIPTION sales_sub
  CONNECTION 'host=primary.com dbname=sales'
  PUBLICATION sales_pub
  WITH (slot_name = 'sales_slot');
```

#### 2. Bi-Directional Replication
```sql
-- Both servers publish and subscribe
-- Requires conflict resolution

CREATE PUBLICATION bi_pub FOR ALL TABLES;

CREATE SUBSCRIPTION bi_sub
  CONNECTION 'host=peer.com dbname=db'
  PUBLICATION bi_pub
  WITH (
    conflict_resolution = 'last_write_wins',
    track_commit_timestamp = on
  );
```

#### 3. Multi-Master (N-Way)
```
     Node A
      /  \
     /    \
  Node B--Node C

Each node publishes to and subscribes from others
Requires vector clocks or CRDT for conflict resolution
```

### Logical Replication Components

#### Change Data Capture (CDC)
```
Transaction Log Entry:
  - Operation: INSERT/UPDATE/DELETE
  - Table UUID
  - Old values (for UPDATE/DELETE)
  - New values (for INSERT/UPDATE)
  - Transaction ID
  - Timestamp
  - User/Application context
```

#### Replication Slots
```
Slot = {
  Name: "regional_replication"
  Type: LOGICAL/PHYSICAL
  Plugin: "scratchbird_output"
  LSN: Current position
  Active: true/false
  Retained_Write_after_Log_Size: bytes
}
```

#### Conflict Resolution Strategies
```
1. LAST_WRITE_WINS:
   - Use commit timestamp
   - Simple but may lose updates

2. FIRST_WRITE_WINS:
   - Earlier timestamp prevails
   - Rejects later updates

3. CUSTOM_FUNCTION:
   - User-defined resolution
   - Can merge changes

4. MANUAL_RESOLUTION:
   - Queue conflicts
   - Administrator resolves

5. CRDT (Conflict-free Replicated Data Types):
   - Automatic merging
   - Eventually consistent
```

### Logical Replication Metadata Tables
```sql
sys.publications
  - publication_id (UUID)
  - publication_name
  - owner
  - all_tables (boolean)
  - publish_insert/update/delete/truncate

sys.subscriptions  
  - subscription_id (UUID)
  - subscription_name
  - connection_string
  - slot_name
  - publications[]
  - enabled

sys.replication_conflicts
  - conflict_id
  - timestamp
  - table_name
  - operation
  - local_values
  - remote_values
  - resolution_status
```

## Comparison: Shadow vs Logical Replication

| Feature | Shadow Database | Logical Replication |
|---------|----------------|-------------------|
| Granularity | Page/Block level | Row level |
| Schema changes | Automatically replicated | Manual coordination |
| Selective replication | No (entire DB) | Yes (specific tables) |
| Cross-version | No | Yes |
| Failover time | Seconds | Minutes |
| Network bandwidth | High | Lower |
| Conflict resolution | N/A | Required for bi-directional |
| Use case | DR/HA failover | Load distribution, reporting |

## Hybrid Approach

### Best Practice Configuration
```
Production Primary
    ├── Shadow (same DC)         -- Instant failover
    ├── Shadow (remote DC)        -- DR failover
    ├── Logical Replica (reporting) -- Read queries
    └── Logical Replica (analytics) -- Heavy queries

Benefits:
- Shadow for HA/DR (RPO=0, RTO<1min)
- Logical for scaling reads
- Logical for data warehouse ETL
```

## Implementation Phases

### Phase 1: Shadow Database (Physical)
- Page-level replication protocol
- Shadow state management
- Promotion mechanism
- Basic monitoring

### Phase 2: Logical Replication (One-Way)
- Change data capture
- Publication/Subscription
- Initial data sync
- Basic conflict detection

### Phase 3: Advanced Replication
- Bi-directional logical
- Multi-master support
- Conflict resolution strategies
- Hybrid shadow+logical

## Testing Requirements

### Shadow Database Tests
1. Create shadow, verify byte-identical pages
2. Apply 1000 page changes, verify application
3. Promote shadow, verify becomes primary
4. Cascade shadow (shadow of shadow)
5. Network failure handling
6. Corruption detection and handling

### Logical Replication Tests
1. Single table replication
2. Schema change handling
3. Conflict resolution (all strategies)
4. Initial sync of large table
5. Bi-directional updates
6. Circular replication prevention

## Security Considerations

### Shadow Replication
- Encrypted page transmission (TLS)
- Authentication keys (not passwords)
- IP whitelist for shadow sources
- Separate port from client connections

### Logical Replication
- Row-level security preservation
- Encrypted logical change stream
- Publication access control
- Subscription authentication

## Performance Targets

### Shadow Database
- Lag: < 1 second (same DC)
- Lag: < 5 seconds (cross-DC)
- Promotion time: < 10 seconds
- Page application: > 10,000 pages/second

### Logical Replication
- Change capture overhead: < 5%
- Apply rate: > 50,000 rows/second
- Initial sync: > 1GB/minute
- Conflict resolution: < 10ms per conflict
