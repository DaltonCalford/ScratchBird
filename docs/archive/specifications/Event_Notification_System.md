# Event Notification System Specification

## Overview

ScratchBird implements a sophisticated event notification system inspired by Firebird's POST_EVENT mechanism, but enhanced with modern features like event payloads, filtering, and guaranteed delivery options.

## Core Concepts

### 1. Event Definition and Registration

```sql
-- Define named events (optional - events can be ad-hoc too)
CREATE EVENT customer_updated;
CREATE EVENT order_placed WITH PAYLOAD;
CREATE EVENT inventory_low (
    product_id INTEGER,
    current_quantity INTEGER,
    threshold INTEGER
);

-- Events can also be implicit (created on first use)
-- No need to pre-declare simple events
```

### 2. Raising Events

```sql
-- Simple event without payload
CREATE TRIGGER customer_update_trigger
AFTER UPDATE ON customers
FOR EACH ROW
BEGIN
    POST_EVENT 'customer_updated';
END;

-- Event with simple string payload
CREATE TRIGGER order_placed_trigger
AFTER INSERT ON orders
FOR EACH ROW
BEGIN
    POST_EVENT 'order_placed' WITH NEW.order_id::VARCHAR;
END;

-- Event with structured payload
CREATE PROCEDURE process_order(order_id INTEGER)
BEGIN
    -- Process the order...
    
    -- Post event with JSON payload
    POST_EVENT 'order_processed' WITH JSON_OBJECT(
        'order_id', order_id,
        'timestamp', CURRENT_TIMESTAMP,
        'status', 'completed'
    );
END;

-- Conditional event posting
CREATE TRIGGER inventory_check
AFTER UPDATE ON inventory
FOR EACH ROW
WHEN NEW.quantity < NEW.reorder_level
BEGIN
    POST_EVENT 'inventory_low' WITH (
        NEW.product_id,
        NEW.quantity,
        NEW.reorder_level
    );
END;
```

### 3. Client-Side Event Listening

#### C/C++ API

```cpp
#include <scratchbird/events.h>

class EventExample {
public:
    void listen_for_events() {
        SDBConnection* conn = sdb_connect("localhost", "mydb", "user", "pass");
        
        // Register interest in events
        SDBEventHandle* handle = sdb_event_register(conn, 
            "customer_updated,order_placed,inventory_low", 
            event_callback, 
            this  // user data
        );
        
        // Start listening (blocks until event or timeout)
        sdb_event_wait(handle, 30000);  // 30 second timeout
        
        // Or async listening
        sdb_event_listen_async(handle);
        
        // Main application continues...
        
        // Cleanup
        sdb_event_unregister(handle);
    }
    
private:
    static void event_callback(
        const char* event_name,
        const void* payload,
        size_t payload_size,
        void* user_data
    ) {
        EventExample* self = static_cast<EventExample*>(user_data);
        
        if (strcmp(event_name, "inventory_low") == 0) {
            // Parse structured payload
            InventoryEvent evt;
            sdb_parse_event_payload(payload, payload_size, &evt);
            
            printf("Low inventory alert: Product %d has %d units (threshold: %d)\n",
                evt.product_id, evt.current_quantity, evt.threshold);
            
            // Take action
            self->reorder_product(evt.product_id);
        }
    }
};
```

#### Python API

```python
import scratchbird

class EventListener:
    def __init__(self, connection):
        self.conn = connection
        self.running = True
    
    def start_listening(self):
        # Register for multiple events
        events = ['customer_updated', 'order_placed', 'inventory_low']
        
        # Synchronous listening in thread
        def event_thread():
            with self.conn.event_listener(events) as listener:
                while self.running:
                    # Wait for event (with timeout)
                    event = listener.wait(timeout=5.0)
                    
                    if event:
                        self.handle_event(event)
        
        # Start background thread
        import threading
        thread = threading.Thread(target=event_thread)
        thread.daemon = True
        thread.start()
    
    def handle_event(self, event):
        print(f"Event: {event.name}")
        
        if event.name == 'order_placed':
            order_id = event.payload
            print(f"New order: {order_id}")
            self.process_new_order(order_id)
        
        elif event.name == 'inventory_low':
            data = event.payload_json()
            print(f"Low inventory: Product {data['product_id']}")
    
    # Async/await version
    async def async_listen(self):
        async with self.conn.async_event_listener(events) as listener:
            async for event in listener:
                await self.async_handle_event(event)
```

#### Java API

```java
import com.scratchbird.events.*;

public class EventExample {
    private SDBConnection connection;
    private EventListener listener;
    
    public void startListening() {
        // Register for events
        listener = connection.createEventListener(
            Arrays.asList("customer_updated", "order_placed", "inventory_low")
        );
        
        // Add handlers
        listener.onEvent("customer_updated", this::handleCustomerUpdate);
        listener.onEvent("order_placed", this::handleOrderPlaced);
        listener.onEvent("inventory_low", this::handleLowInventory);
        
        // Start listening in background thread
        listener.startAsync();
        
        // Or use CompletableFuture
        CompletableFuture<Event> future = listener.waitForEventAsync();
        future.thenAccept(this::processEvent);
    }
    
    private void handleLowInventory(Event event) {
        InventoryPayload payload = event.getPayload(InventoryPayload.class);
        System.out.printf("Product %d low: %d units%n", 
            payload.productId, payload.quantity);
        
        // Reorder if needed
        if (payload.quantity < 10) {
            reorderProduct(payload.productId);
        }
    }
}
```

### 4. Advanced Event Features

#### Event Filtering and Patterns

```sql
-- Pattern-based event subscription
CREATE EVENT PATTERN order_events AS 'order_*';

-- Filtered events
CREATE EVENT high_value_order 
WHEN amount > 10000;

-- Composite events
CREATE EVENT rush_order 
WHEN order_placed AND shipping_type = 'EXPRESS';
```

```cpp
// Client-side filtering
SDBEventFilter* filter = sdb_create_filter();
sdb_filter_add_pattern(filter, "order_*");  // All order events
sdb_filter_add_condition(filter, "amount > 1000");  // High value only

SDBEventHandle* handle = sdb_event_register_filtered(
    conn, filter, callback, user_data
);
```

#### Event Queuing and Persistence

```sql
-- Create persistent event queue
CREATE EVENT QUEUE order_queue
    PERSISTENT = TRUE
    MAX_SIZE = 10000
    RETENTION = '7 DAYS';

-- Post to specific queue
POST_EVENT 'order_placed' TO QUEUE order_queue;

-- Consume from queue (guaranteed delivery)
CONSUME EVENT FROM order_queue;
```

#### Event Aggregation

```sql
-- Aggregate events over time window
CREATE EVENT AGGREGATOR high_traffic
    WINDOW = '1 MINUTE'
    THRESHOLD = 100
    EVENT = 'page_view';

-- Triggers when 100+ page views in 1 minute
CREATE TRIGGER traffic_alert
ON EVENT high_traffic
BEGIN
    POST_EVENT 'traffic_spike' WITH COUNT;
END;
```

### 5. Implementation Architecture

```cpp
// Core event system architecture
namespace scratchbird::events {

class EventManager {
private:
    // Event registration
    struct EventRegistration {
        std::string pattern;
        std::weak_ptr<Connection> connection;
        EventCallback callback;
        std::thread::id thread_id;
        bool is_async;
    };
    
    // Event queue
    struct EventData {
        std::string name;
        std::vector<uint8_t> payload;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        uint64_t sequence_number;
    };
    
    // Thread-safe event queue
    class EventQueue {
        std::queue<EventData> events;
        std::mutex mutex;
        std::condition_variable cv;
        
    public:
        void push(EventData event) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                events.push(std::move(event));
            }
            cv.notify_all();  // Wake all waiters
        }
        
        std::optional<EventData> wait_pop(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mutex);
            if (cv.wait_for(lock, timeout, [this] { return !events.empty(); })) {
                EventData event = std::move(events.front());
                events.pop();
                return event;
            }
            return std::nullopt;
        }
    };
    
    // Global registry
    std::map<std::string, std::vector<EventRegistration>> registrations;
    std::shared_mutex registry_mutex;
    
    // Connection queues
    std::map<ConnectionId, std::unique_ptr<EventQueue>> connection_queues;
    
    // Persistent queue support
    class PersistentQueue {
        std::string path;
        std::fstream file;
        std::mutex mutex;
        
    public:
        void append(const EventData& event);
        std::vector<EventData> read_pending();
        void mark_consumed(uint64_t sequence);
    };
    
public:
    void post_event(const std::string& name, const void* payload, size_t size) {
        EventData event{
            .name = name,
            .payload = std::vector<uint8_t>((uint8_t*)payload, (uint8_t*)payload + size),
            .timestamp = std::chrono::steady_clock::now(),
            .sequence_number = next_sequence()
        };
        
        // Find all matching registrations
        std::shared_lock<std::shared_mutex> lock(registry_mutex);
        
        for (const auto& [pattern, regs] : registrations) {
            if (matches_pattern(name, pattern)) {
                for (const auto& reg : regs) {
                    // Queue event for connection
                    if (auto conn = reg.connection.lock()) {
                        auto conn_id = conn->get_id();
                        if (connection_queues.count(conn_id)) {
                            connection_queues[conn_id]->push(event);
                        }
                    }
                }
            }
        }
    }
    
    EventHandle register_listener(
        std::shared_ptr<Connection> conn,
        const std::string& pattern,
        EventCallback callback
    ) {
        std::unique_lock<std::shared_mutex> lock(registry_mutex);
        
        EventRegistration reg{
            .pattern = pattern,
            .connection = conn,
            .callback = callback,
            .thread_id = std::this_thread::get_id(),
            .is_async = false
        };
        
        registrations[pattern].push_back(reg);
        
        // Create queue for connection if needed
        auto conn_id = conn->get_id();
        if (!connection_queues.count(conn_id)) {
            connection_queues[conn_id] = std::make_unique<EventQueue>();
        }
        
        return EventHandle{/* ... */};
    }
    
    std::optional<EventData> wait_for_event(
        ConnectionId conn_id,
        std::chrono::milliseconds timeout
    ) {
        if (connection_queues.count(conn_id)) {
            return connection_queues[conn_id]->wait_pop(timeout);
        }
        return std::nullopt;
    }
};

} // namespace scratchbird::events
```

### 6. Network Protocol

```cpp
// Wire protocol for event notifications
struct EventPacket {
    uint8_t  packet_type = PACKET_EVENT;
    uint32_t event_name_length;
    char     event_name[256];
    uint32_t payload_size;
    uint8_t  payload[];  // Variable length
    uint64_t sequence_number;
    uint64_t timestamp_micros;
};

// Registration packet
struct EventRegisterPacket {
    uint8_t  packet_type = PACKET_EVENT_REGISTER;
    uint32_t pattern_count;
    struct {
        uint32_t pattern_length;
        char pattern[256];
    } patterns[];
};

// Acknowledgment for guaranteed delivery
struct EventAckPacket {
    uint8_t  packet_type = PACKET_EVENT_ACK;
    uint64_t sequence_number;
};
```

### 7. Performance Optimizations

```cpp
class OptimizedEventSystem {
    // Lock-free queue for high throughput
    using LockFreeQueue = boost::lockfree::queue<EventData>;
    
    // Memory pool for event objects
    ObjectPool<EventData> event_pool;
    
    // Batch notifications
    void batch_notify() {
        std::vector<EventData> batch;
        batch.reserve(100);
        
        // Collect events
        while (batch.size() < 100 && !queue.empty()) {
            batch.push_back(queue.pop());
        }
        
        // Send batch to listeners
        notify_listeners(batch);
    }
    
    // Zero-copy payload transfer
    class ZeroCopyPayload {
        std::shared_ptr<const std::vector<uint8_t>> data;
    public:
        std::span<const uint8_t> view() const {
            return *data;
        }
    };
};
```

## Configuration

```sql
-- System-wide event settings
ALTER SYSTEM SET event_queue_size = 10000;
ALTER SYSTEM SET event_retention_days = 7;
ALTER SYSTEM SET event_batch_size = 100;
ALTER SYSTEM SET event_notification_threads = 4;

-- Per-database settings
ALTER DATABASE mydb SET event_persistence = TRUE;
ALTER DATABASE mydb SET event_compression = TRUE;

-- Per-session settings
SET SESSION event_timeout = 30000;  -- 30 seconds
SET SESSION event_filter = 'order_*,inventory_*';
```

## Use Cases

### 1. Real-Time Dashboard

```python
# Dashboard updates in real-time
class Dashboard:
    def __init__(self, db_connection):
        self.conn = db_connection
        self.listener = db_connection.event_listener([
            'order_placed',
            'order_shipped', 
            'payment_received'
        ])
    
    async def run(self):
        async for event in self.listener:
            if event.name == 'order_placed':
                await self.update_order_count()
            elif event.name == 'payment_received':
                await self.update_revenue(event.payload['amount'])
```

### 2. Cache Invalidation

```cpp
// Invalidate cache when data changes
class CacheManager {
    void setup_invalidation() {
        db->register_event("table_updated", [this](Event e) {
            std::string table = e.payload_string();
            cache->invalidate(table);
            log->info("Cache invalidated for table: {}", table);
        });
    }
};
```

### 3. Workflow Orchestration

```sql
-- Trigger workflow steps via events
CREATE PROCEDURE process_order_workflow()
BEGIN
    -- Step 1: Validate
    CALL validate_order();
    POST_EVENT 'order_validated';
    
    -- Step 2: Reserve inventory
    CALL reserve_inventory();
    POST_EVENT 'inventory_reserved';
    
    -- Step 3: Process payment
    CALL process_payment();
    POST_EVENT 'payment_processed';
    
    -- Step 4: Ship
    CALL create_shipment();
    POST_EVENT 'order_shipped';
END;
```

### 4. Monitoring and Alerting

```java
// Monitor database events for anomalies
public class SecurityMonitor {
    public void monitorFailedLogins() {
        db.onEvent("login_failed", event -> {
            LoginAttempt attempt = event.getPayload(LoginAttempt.class);
            
            if (failedAttempts.count(attempt.username) > 5) {
                // Too many failures
                alertAdmin(attempt.username, attempt.ipAddress);
                blockUser(attempt.username);
            }
        });
    }
}
```

## Testing

```cpp
TEST(EventSystem, BasicEventDelivery) {
    auto conn = connect_to_test_db();
    
    std::promise<std::string> event_received;
    auto future = event_received.get_future();
    
    // Register listener
    conn->register_event("test_event", [&](Event e) {
        event_received.set_value(e.name);
    });
    
    // Post event from another connection
    auto conn2 = connect_to_test_db();
    conn2->execute("POST_EVENT 'test_event'");
    
    // Wait for event
    ASSERT_EQ(future.get(), "test_event");
}

TEST(EventSystem, PayloadDelivery) {
    auto conn = connect_to_test_db();
    
    struct TestPayload {
        int id;
        char message[100];
    };
    
    std::promise<TestPayload> payload_received;
    
    conn->register_event("payload_event", [&](Event e) {
        payload_received.set_value(e.get_payload<TestPayload>());
    });
    
    // Post with payload
    TestPayload payload{42, "Hello Event"};
    conn->post_event("payload_event", &payload, sizeof(payload));
    
    auto received = payload_received.get_future().get();
    ASSERT_EQ(received.id, 42);
    ASSERT_STREQ(received.message, "Hello Event");
}

TEST(EventSystem, MultipleListeners) {
    // Test that multiple listeners receive same event
    std::atomic<int> received_count{0};
    
    auto listener1 = create_listener("test_event", [&] { received_count++; });
    auto listener2 = create_listener("test_event", [&] { received_count++; });
    auto listener3 = create_listener("test_event", [&] { received_count++; });
    
    post_event("test_event");
    
    std::this_thread::sleep_for(100ms);
    ASSERT_EQ(received_count.load(), 3);
}
```

## Comparison with Other Databases

| Feature | Firebird | PostgreSQL | Oracle | ScratchBird |
|---------|----------|------------|--------|-------------|
| Named Events | ✅ | ❌ (NOTIFY) | ✅ (Alerts) | ✅ |
| Event Payload | ❌ | ✅ (text) | ✅ | ✅ (any) |
| Pattern Matching | ❌ | ❌ | ❌ | ✅ |
| Persistent Queue | ❌ | ❌ | ✅ (AQ) | ✅ |
| Guaranteed Delivery | ❌ | ❌ | ✅ | ✅ |
| Event Aggregation | ❌ | ❌ | ❌ | ✅ |
| Async/Await API | ❌ | ❌ | ❌ | ✅ |

## Summary

ScratchBird's event system provides:

1. **Simple API** - Easy POST_EVENT in SQL
2. **Rich Payloads** - Any data type, including JSON
3. **Pattern Matching** - Subscribe to event patterns
4. **Guaranteed Delivery** - Optional persistence
5. **High Performance** - Lock-free queues, batching
6. **Modern APIs** - Async/await, callbacks, futures
7. **Cross-Language** - C/C++, Python, Java, .NET, etc.

This makes ScratchBird ideal for:
- Real-time applications
- Event-driven architectures
- Microservice communication
- Cache invalidation
- Monitoring and alerting
- Workflow orchestration