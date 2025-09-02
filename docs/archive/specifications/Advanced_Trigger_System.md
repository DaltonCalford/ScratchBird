# Advanced Trigger System Specification

## Overview

ScratchBird implements a comprehensive trigger system supporting both database-level and table-level triggers, including the rare SELECT trigger for read auditing. Triggers execute in a deterministic order based on position values.

## Core Concepts

### 1. Trigger Types and Hierarchy

```
Database Triggers (Global Scope)
├── ON CONNECT
├── ON DISCONNECT  
├── ON TRANSACTION START
├── ON TRANSACTION COMMIT
├── ON TRANSACTION ROLLBACK
└── ON TWO_PHASE_COMMIT

Table Triggers (Table Scope)
├── BEFORE INSERT/UPDATE/DELETE/SELECT
└── AFTER INSERT/UPDATE/DELETE/SELECT
```

### 2. Trigger Position and Execution Order

```sql
-- Triggers execute in position order (lowest first)
-- Default position is 0 if not specified

CREATE TRIGGER audit_trigger
AFTER UPDATE ON customers
POSITION 10  -- Executes first
FOR EACH ROW
BEGIN
    INSERT INTO audit_log VALUES (USER, CURRENT_TIMESTAMP, 'UPDATE');
END;

CREATE TRIGGER validate_trigger  
BEFORE UPDATE ON customers
POSITION 20  -- Executes second
FOR EACH ROW
BEGIN
    IF NEW.credit_limit > 100000 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Credit limit too high';
    END IF;
END;

CREATE TRIGGER notify_trigger
AFTER UPDATE ON customers  
POSITION 30  -- Executes third
FOR EACH ROW
BEGIN
    POST_EVENT 'customer_updated' WITH NEW.customer_id;
END;

-- View trigger execution order
SELECT 
    trigger_name,
    position,
    event_manipulation,
    action_timing
FROM information_schema.triggers
WHERE event_object_table = 'customers'
ORDER BY position, trigger_name;
```

### 3. Database-Level Triggers

#### ON CONNECT Trigger

```sql
-- Track all connections
CREATE TRIGGER log_connections
ON CONNECT
POSITION 1
BEGIN
    INSERT INTO connection_log (
        user_name,
        client_address,
        connect_time,
        application_name,
        session_id
    ) VALUES (
        CURRENT_USER,
        CLIENT_ADDRESS(),
        CURRENT_TIMESTAMP,
        APPLICATION_NAME(),
        SESSION_ID()
    );
    
    -- Enforce connection limits
    IF (SELECT COUNT(*) FROM connection_log 
        WHERE user_name = CURRENT_USER 
        AND connect_time > CURRENT_TIMESTAMP - INTERVAL '1 HOUR') > 10 THEN
        SIGNAL SQLSTATE '08004' 
            SET MESSAGE_TEXT = 'Too many connections from user';
    END IF;
END;

-- Security check on connect
CREATE TRIGGER security_check
ON CONNECT
POSITION 2
BEGIN
    -- Check if user is on blocklist
    IF EXISTS (SELECT 1 FROM blocked_users WHERE username = CURRENT_USER) THEN
        INSERT INTO security_log VALUES (CURRENT_USER, 'BLOCKED_CONNECTION', CURRENT_TIMESTAMP);
        SIGNAL SQLSTATE '28000' SET MESSAGE_TEXT = 'User blocked';
    END IF;
    
    -- Check maintenance window
    IF EXISTS (SELECT 1 FROM maintenance_schedule 
               WHERE CURRENT_TIMESTAMP BETWEEN start_time AND end_time
               AND CURRENT_USER NOT IN ('admin', 'system')) THEN
        SIGNAL SQLSTATE '08006' SET MESSAGE_TEXT = 'Database in maintenance mode';
    END IF;
END;
```

#### ON DISCONNECT Trigger

```sql
CREATE TRIGGER cleanup_session
ON DISCONNECT
POSITION 1
BEGIN
    -- Log session duration
    UPDATE connection_log 
    SET disconnect_time = CURRENT_TIMESTAMP,
        duration = CURRENT_TIMESTAMP - connect_time
    WHERE session_id = SESSION_ID()
    AND disconnect_time IS NULL;
    
    -- Clean up temporary data
    DELETE FROM session_temp_data WHERE session_id = SESSION_ID();
    
    -- Release any held resources
    CALL release_session_locks(SESSION_ID());
END;
```

#### ON TRANSACTION Triggers

```sql
-- Transaction start trigger
CREATE TRIGGER txn_start_monitor
ON TRANSACTION START
POSITION 1
BEGIN
    INSERT INTO transaction_log (
        txn_id,
        user_name,
        start_time,
        isolation_level
    ) VALUES (
        CURRENT_TRANSACTION_ID(),
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        TRANSACTION_ISOLATION_LEVEL()
    );
    
    -- Set transaction-level configuration
    SET LOCAL work_mem = '256MB';
    SET LOCAL statement_timeout = '30min';
END;

-- Transaction commit trigger
CREATE TRIGGER txn_commit_audit
ON TRANSACTION COMMIT
POSITION 1
BEGIN
    -- Log successful transaction
    UPDATE transaction_log
    SET end_time = CURRENT_TIMESTAMP,
        status = 'COMMITTED',
        rows_affected = ROW_COUNT(),
        duration = CURRENT_TIMESTAMP - start_time
    WHERE txn_id = CURRENT_TRANSACTION_ID();
    
    -- Post event for downstream systems
    POST_EVENT 'transaction_committed' WITH JSON_OBJECT(
        'txn_id', CURRENT_TRANSACTION_ID(),
        'user', CURRENT_USER,
        'duration', CURRENT_TIMESTAMP - (SELECT start_time FROM transaction_log 
                                         WHERE txn_id = CURRENT_TRANSACTION_ID())
    );
END;

-- Transaction rollback trigger
CREATE TRIGGER txn_rollback_alert
ON TRANSACTION ROLLBACK
POSITION 1
BEGIN
    -- Log rollback with reason
    INSERT INTO rollback_log (
        txn_id,
        user_name,
        rollback_time,
        rollback_reason,
        error_code,
        error_message
    ) VALUES (
        CURRENT_TRANSACTION_ID(),
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        ROLLBACK_REASON(),
        SQLSTATE,
        SQLERRM
    );
    
    -- Alert on suspicious rollbacks
    IF ROLLBACK_REASON() = 'DEADLOCK' THEN
        POST_EVENT 'deadlock_detected' WITH CURRENT_TRANSACTION_ID();
    END IF;
END;

-- Two-phase commit trigger (distributed transactions)
CREATE TRIGGER txn_2pc_coordinator
ON TWO_PHASE_COMMIT
POSITION 1
BEGIN
    -- Log distributed transaction
    INSERT INTO distributed_txn_log (
        global_txn_id,
        local_txn_id,
        phase,
        participants,
        status
    ) VALUES (
        GLOBAL_TRANSACTION_ID(),
        CURRENT_TRANSACTION_ID(),
        COMMIT_PHASE(),  -- 'PREPARE' or 'COMMIT'
        PARTICIPANT_NODES(),
        'IN_PROGRESS'
    );
    
    -- Coordinate with other nodes
    IF COMMIT_PHASE() = 'PREPARE' THEN
        -- Check all participants ready
        IF NOT all_participants_ready() THEN
            SIGNAL SQLSTATE '40001' SET MESSAGE_TEXT = 'Not all participants ready';
        END IF;
    END IF;
END;
```

### 4. Table-Level Triggers with SELECT Support

#### SELECT Triggers (Unique Feature)

```sql
-- Audit all reads on sensitive tables
CREATE TRIGGER audit_sensitive_reads
AFTER SELECT ON employee_salaries
POSITION 1
FOR EACH STATEMENT
BEGIN
    INSERT INTO read_audit_log (
        table_name,
        user_name,
        query_text,
        rows_returned,
        access_time,
        client_ip
    ) VALUES (
        'employee_salaries',
        CURRENT_USER,
        CURRENT_QUERY(),
        ROW_COUNT(),
        CURRENT_TIMESTAMP,
        CLIENT_ADDRESS()
    );
    
    -- Alert on suspicious access patterns
    IF ROW_COUNT() > 1000 OR CURRENT_QUERY() LIKE '%*%' THEN
        POST_EVENT 'suspicious_read' WITH JSON_OBJECT(
            'user', CURRENT_USER,
            'table', 'employee_salaries',
            'rows', ROW_COUNT()
        );
    END IF;
END;

-- Row-level SELECT trigger for fine-grained auditing
CREATE TRIGGER track_vip_access
BEFORE SELECT ON vip_customers
POSITION 1
FOR EACH ROW
WHEN OLD.security_level = 'TOP_SECRET'
BEGIN
    -- Log each row accessed
    INSERT INTO vip_access_log (
        customer_id,
        accessed_by,
        access_time,
        access_reason
    ) VALUES (
        OLD.customer_id,
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        SESSION_VARIABLE('access_reason')
    );
    
    -- Require access reason
    IF SESSION_VARIABLE('access_reason') IS NULL THEN
        SIGNAL SQLSTATE '42000' 
            SET MESSAGE_TEXT = 'Access reason required for VIP customers';
    END IF;
END;

-- Performance monitoring via SELECT trigger
CREATE TRIGGER monitor_slow_queries
AFTER SELECT ON large_table
POSITION 10
FOR EACH STATEMENT
WHEN EXECUTION_TIME() > INTERVAL '5 seconds'
BEGIN
    INSERT INTO slow_query_log (
        table_name,
        query_text,
        execution_time,
        rows_examined,
        rows_returned
    ) VALUES (
        'large_table',
        CURRENT_QUERY(),
        EXECUTION_TIME(),
        ROWS_EXAMINED(),
        ROW_COUNT()
    );
END;
```

#### Traditional DML Triggers with Position

```sql
-- Multiple triggers on same event, ordered by position
CREATE TRIGGER validate_customer
BEFORE INSERT ON customers
POSITION 10  -- Runs first
FOR EACH ROW
BEGIN
    -- Validation logic
    IF NEW.email NOT LIKE '%@%.%' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Invalid email format';
    END IF;
END;

CREATE TRIGGER normalize_customer
BEFORE INSERT ON customers  
POSITION 20  -- Runs second
FOR EACH ROW
BEGIN
    -- Normalization
    SET NEW.email = LOWER(NEW.email);
    SET NEW.phone = REGEXP_REPLACE(NEW.phone, '[^0-9]', '');
END;

CREATE TRIGGER enrich_customer
BEFORE INSERT ON customers
POSITION 30  -- Runs third
FOR EACH ROW
BEGIN
    -- Enrichment
    IF NEW.country_code IS NULL THEN
        SET NEW.country_code = lookup_country_code(NEW.phone);
    END IF;
    SET NEW.credit_score = get_credit_score(NEW.ssn);
END;
```

### 5. Trigger Management

#### Active/Inactive State

```sql
-- Create inactive trigger
CREATE TRIGGER archive_old_records
AFTER INSERT ON transactions
POSITION 100
INACTIVE  -- Trigger created but not executing
FOR EACH ROW
BEGIN
    -- Archive logic
END;

-- Activate trigger
ALTER TRIGGER archive_old_records ACTIVE;

-- Deactivate trigger temporarily
ALTER TRIGGER validate_customer INACTIVE;

-- Bulk trigger management
ALTER ALL TRIGGERS ON customers INACTIVE;
ALTER ALL TRIGGERS ON customers ACTIVE;

-- Conditional activation
ALTER TRIGGER security_check 
    ACTIVE WHEN database_mode() = 'PRODUCTION';
```

#### Position Management

```sql
-- Change trigger position
ALTER TRIGGER validate_customer POSITION 5;

-- Insert trigger between others
ALTER TRIGGER new_trigger POSITION BETWEEN validate_customer AND normalize_customer;

-- Move to end
ALTER TRIGGER cleanup_trigger POSITION LAST;

-- View trigger chain
SELECT 
    trigger_name,
    position,
    is_active,
    action_timing,
    event_manipulation
FROM information_schema.triggers
WHERE event_object_table = 'customers'
ORDER BY 
    event_manipulation,
    action_timing,
    position;
```

### 6. Advanced Trigger Features

#### Trigger Dependencies

```sql
-- Trigger with dependencies
CREATE TRIGGER dependent_trigger
AFTER UPDATE ON orders
POSITION 50
DEPENDS ON (validate_trigger, audit_trigger)  -- Won't run if dependencies fail
FOR EACH ROW
BEGIN
    -- Process only if dependencies succeeded
END;
```

#### Conditional Triggers

```sql
-- Environment-aware triggers
CREATE TRIGGER production_only_audit
AFTER INSERT ON sensitive_table
POSITION 1
WHEN CURRENT_DATABASE() = 'production'
AND CURRENT_TIME BETWEEN TIME '08:00' AND TIME '18:00'
FOR EACH ROW
BEGIN
    -- Only runs in production during business hours
END;
```

#### Trigger Recursion Control

```sql
-- Prevent recursive triggers
CREATE TRIGGER prevent_recursion
BEFORE UPDATE ON accounts
POSITION 1
FOR EACH ROW
WHEN TRIGGER_DEPTH() = 0  -- Only runs if not called from another trigger
BEGIN
    -- Safe from recursion
END;

-- Allow limited recursion
CREATE TRIGGER limited_recursion
AFTER INSERT ON tree_table
POSITION 1
FOR EACH ROW
WHEN TRIGGER_DEPTH() < 5  -- Max 5 levels deep
BEGIN
    -- Recursive logic
END;
```

### 7. Implementation Architecture

```cpp
namespace scratchbird::triggers {

class TriggerManager {
private:
    // Trigger definition
    struct Trigger {
        std::string name;
        TriggerType type;  // DATABASE or TABLE
        TriggerEvent event;  // CONNECT, INSERT, UPDATE, etc.
        TriggerTiming timing;  // BEFORE, AFTER
        int position;  // Execution order
        bool is_active;
        bool for_each_row;
        std::string condition;  // WHEN clause
        std::string body;  // Trigger logic
        std::vector<std::string> dependencies;
    };
    
    // Trigger chains organized by event
    struct TriggerChain {
        std::multimap<int, std::shared_ptr<Trigger>> triggers;  // position -> trigger
        
        void execute(TriggerContext& context) {
            for (auto& [position, trigger] : triggers) {
                if (!trigger->is_active) continue;
                
                if (!evaluate_condition(trigger->condition, context)) continue;
                
                if (!check_dependencies(trigger->dependencies)) continue;
                
                execute_trigger(trigger, context);
            }
        }
    };
    
    // Database-level triggers
    std::map<DatabaseEvent, TriggerChain> database_triggers;
    
    // Table-level triggers
    std::map<TableId, std::map<TableEvent, TriggerChain>> table_triggers;
    
    // Execution context
    thread_local int trigger_depth = 0;
    thread_local std::stack<TriggerContext> context_stack;
    
public:
    void fire_database_trigger(DatabaseEvent event) {
        TriggerDepthGuard guard(trigger_depth);
        
        if (database_triggers.count(event)) {
            TriggerContext context{
                .event = event,
                .user = current_user(),
                .timestamp = current_timestamp()
            };
            
            database_triggers[event].execute(context);
        }
    }
    
    void fire_table_trigger(
        TableId table_id,
        TableEvent event,
        const Row* old_row,
        Row* new_row
    ) {
        TriggerDepthGuard guard(trigger_depth);
        
        if (trigger_depth > max_trigger_depth) {
            throw TriggerRecursionError("Maximum trigger depth exceeded");
        }
        
        auto& chains = table_triggers[table_id];
        if (chains.count(event)) {
            TriggerContext context{
                .event = event,
                .old_row = old_row,
                .new_row = new_row,
                .table_id = table_id
            };
            
            chains[event].execute(context);
        }
    }
    
    // Special handling for SELECT triggers
    void fire_select_trigger(
        TableId table_id,
        const Query& query,
        const ResultSet& results
    ) {
        if (!has_select_triggers(table_id)) {
            return;  // Fast path - no overhead if no SELECT triggers
        }
        
        TriggerContext context{
            .event = TableEvent::SELECT,
            .query = query,
            .row_count = results.size(),
            .execution_time = query.execution_time()
        };
        
        // BEFORE SELECT triggers
        fire_table_trigger_phase(table_id, TableEvent::BEFORE_SELECT, context);
        
        // For row-level SELECT triggers
        if (has_row_level_select_triggers(table_id)) {
            for (const auto& row : results) {
                context.old_row = &row;  // Read-only access
                fire_row_trigger(table_id, TableEvent::SELECT_ROW, context);
            }
        }
        
        // AFTER SELECT triggers
        fire_table_trigger_phase(table_id, TableEvent::AFTER_SELECT, context);
    }
    
    void alter_trigger_position(const std::string& name, int new_position) {
        auto trigger = find_trigger(name);
        if (trigger) {
            remove_from_chain(trigger);
            trigger->position = new_position;
            add_to_chain(trigger);
        }
    }
    
    void set_trigger_active(const std::string& name, bool active) {
        auto trigger = find_trigger(name);
        if (trigger) {
            trigger->is_active = active;
            
            // Log state change
            log_trigger_state_change(name, active);
        }
    }
};

// RAII guard for trigger depth
class TriggerDepthGuard {
    int& depth;
public:
    TriggerDepthGuard(int& d) : depth(d) { ++depth; }
    ~TriggerDepthGuard() { --depth; }
};

} // namespace scratchbird::triggers
```

### 8. Performance Considerations

```cpp
class OptimizedTriggerSystem {
    // Cache compiled trigger bodies
    std::unordered_map<TriggerId, CompiledTrigger> compiled_triggers;
    
    // Fast path for tables without triggers
    std::bitset<MAX_TABLES> tables_with_triggers;
    std::bitset<MAX_TABLES> tables_with_select_triggers;
    
    // Batch trigger execution
    void execute_triggers_batch(std::vector<TriggerEvent>& events) {
        // Group by trigger for better cache locality
        std::sort(events.begin(), events.end(), 
                  [](auto& a, auto& b) { return a.trigger_id < b.trigger_id; });
        
        // Execute in batches
        for (auto& event : events) {
            execute_cached_trigger(event);
        }
    }
    
    // Skip trigger check optimization
    inline bool should_fire_triggers(TableId table_id, EventType event) {
        // Quick bitset check
        if (!tables_with_triggers[table_id]) return false;
        
        // Special case for SELECT
        if (event == SELECT && !tables_with_select_triggers[table_id]) {
            return false;
        }
        
        return true;
    }
};
```

### 9. Testing

```sql
-- Test trigger position ordering
CREATE TABLE test_trigger_order (
    id INTEGER,
    value TEXT,
    log_order TEXT
);

CREATE TRIGGER trig_pos_30
BEFORE INSERT ON test_trigger_order
POSITION 30
FOR EACH ROW
BEGIN
    SET NEW.log_order = COALESCE(NEW.log_order, '') || '30,';
END;

CREATE TRIGGER trig_pos_10
BEFORE INSERT ON test_trigger_order
POSITION 10
FOR EACH ROW
BEGIN
    SET NEW.log_order = COALESCE(NEW.log_order, '') || '10,';
END;

CREATE TRIGGER trig_pos_20
BEFORE INSERT ON test_trigger_order
POSITION 20
FOR EACH ROW
BEGIN
    SET NEW.log_order = COALESCE(NEW.log_order, '') || '20,';
END;

-- Test: Should execute in order 10, 20, 30
INSERT INTO test_trigger_order (id, value) VALUES (1, 'test');
SELECT log_order FROM test_trigger_order WHERE id = 1;
-- Result: '10,20,30,'

-- Test SELECT triggers
CREATE TRIGGER test_select_audit
AFTER SELECT ON sensitive_table
POSITION 1
FOR EACH STATEMENT
BEGIN
    INSERT INTO select_audit VALUES (CURRENT_USER, ROW_COUNT(), CURRENT_TIMESTAMP);
END;

SELECT * FROM sensitive_table LIMIT 10;
-- Check audit log was created
SELECT * FROM select_audit WHERE user_name = CURRENT_USER;
```

## Comparison with Other Databases

| Feature | Firebird | PostgreSQL | Oracle | MySQL | MSSQL | ScratchBird |
|---------|----------|------------|--------|-------|-------|-------------|
| Trigger Position | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| ON CONNECT | ✅ | ❌ | ✅ | ❌ | ✅ | ✅ |
| ON DISCONNECT | ✅ | ❌ | ✅ | ❌ | ❌ | ✅ |
| ON TRANSACTION | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| SELECT Triggers | ❌ | ❌ | ✅(FGA) | ❌ | ❌ | ✅ |
| Active/Inactive | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| Dependencies | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Recursion Control | Limited | ❌ | ❌ | ❌ | ❌ | ✅ |

## Summary

ScratchBird's trigger system provides:

1. **Deterministic Execution** - Position-based ordering
2. **Complete Coverage** - Database and table-level events
3. **SELECT Triggers** - Unique read auditing capability
4. **Transaction Triggers** - Full transaction lifecycle hooks
5. **Fine Control** - Active/inactive states, dependencies
6. **Performance** - Optimized with caching and fast paths
7. **Safety** - Recursion control and depth limits

This comprehensive trigger system enables:
- Complete audit trails including reads
- Complex business logic enforcement
- Security monitoring and enforcement
- Performance monitoring
- Distributed transaction coordination
- Session management and cleanup