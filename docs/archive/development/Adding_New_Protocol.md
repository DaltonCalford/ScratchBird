# Guide: Adding a New Database Protocol

## Overview
This guide explains how to add support for a new database wire protocol to ScratchBird.

## Architecture Overview

```
Client → Protocol Listener → Y-Valve → Translator → Engine
```

## Steps to Add a New Protocol

### 1. Create Protocol Constants

```cpp
// src/protocols/redis/redis_protocol.h
namespace redis_protocol {
    
    const uint16_t DEFAULT_PORT = 6379;
    
    enum MessageType {
        SIMPLE_STRING = '+',
        ERROR = '-',
        INTEGER = ':',
        BULK_STRING = '$',
        ARRAY = '*'
    };
    
    struct ProtocolConfig {
        uint16_t port = DEFAULT_PORT;
        size_t max_clients = 10000;
        size_t buffer_size = 16384;
        bool inline_commands = true;
    };
}
```

### 2. Implement Protocol Parser

```cpp
// src/protocols/redis/redis_parser.cpp
class RedisParser {
private:
    vector<uint8_t> buffer;
    size_t position = 0;
    
public:
    optional<RedisMessage> parse_message(const uint8_t* data, size_t len) {
        buffer.insert(buffer.end(), data, data + len);
        
        if (buffer.empty()) return nullopt;
        
        switch (buffer[0]) {
            case '*':
                return parse_array();
            case '$':
                return parse_bulk_string();
            default:
                return parse_inline();
        }
    }
    
private:
    optional<RedisMessage> parse_array() {
        // *2\r\n$3\r\nGET\r\n$3\r\nkey\r\n
        if (!has_complete_line()) return nullopt;
        
        auto line = read_line();
        int count = stoi(line.substr(1));
        
        vector<string> elements;
        for (int i = 0; i < count; i++) {
            auto elem = parse_bulk_string();
            if (!elem) return nullopt;
            elements.push_back(elem->data);
        }
        
        return RedisMessage{
            .type = ARRAY,
            .elements = elements
        };
    }
};
```

### 3. Implement Protocol Listener

```cpp
// src/protocols/redis/redis_listener.cpp
class RedisListener : public ProtocolListener {
private:
    tcp::acceptor acceptor;
    YValve* yvalve;
    
public:
    void start(uint16_t port) override {
        acceptor.bind(port);
        acceptor.listen();
        
        while (running) {
            auto client = acceptor.accept();
            thread([this, client]() {
                handle_client(client);
            }).detach();
        }
    }
    
private:
    void handle_client(tcp::socket client) {
        // Detect Redis protocol
        if (!detect_redis_protocol(client)) {
            client.close();
            return;
        }
        
        // Register with Y-Valve
        auto translator = make_unique<RedisTranslator>();
        yvalve->register_connection(client, move(translator));
        
        // Process messages
        RedisParser parser;
        while (client.is_connected()) {
            auto data = client.read();
            auto msg = parser.parse_message(data);
            if (msg) {
                yvalve->process_message(client, *msg);
            }
        }
    }
    
    bool detect_redis_protocol(tcp::socket& client) {
        // Redis clients often send PING or INFO first
        auto initial = client.peek(1);
        return initial[0] == '*' || 
               initial[0] == '$' ||
               (initial[0] >= 'A' && initial[0] <= 'Z');
    }
};
```

### 4. Implement Translator

```cpp
// src/yvalve/translators/redis_translator.cpp
class RedisTranslator : public Translator {
public:
    NativeQuery to_native(const Request& request) override {
        auto redis_cmd = static_cast<const RedisMessage&>(request);
        
        if (redis_cmd.elements.empty()) {
            throw protocol_error("Empty command");
        }
        
        string command = to_upper(redis_cmd.elements[0]);
        
        // Map Redis commands to SQL
        if (command == "GET") {
            return translate_get(redis_cmd);
        } else if (command == "SET") {
            return translate_set(redis_cmd);
        } else if (command == "DEL") {
            return translate_del(redis_cmd);
        } else if (command == "KEYS") {
            return translate_keys(redis_cmd);
        }
        // ... more commands
        
        throw unsupported_command(command);
    }
    
    Response from_native(const Result& result) override {
        // Convert SQL result to Redis response
        RedisResponse response;
        
        if (result.error) {
            response.type = ERROR;
            response.data = "-ERR " + result.error_message + "\r\n";
        } else if (result.rows.empty()) {
            response.type = BULK_STRING;
            response.data = "$-1\r\n";  // NULL
        } else {
            response.type = BULK_STRING;
            auto value = result.rows[0][0];
            response.data = format("${}\r\n{}\r\n", 
                                  value.length(), value);
        }
        
        return response;
    }
    
private:
    NativeQuery translate_get(const RedisMessage& cmd) {
        if (cmd.elements.size() != 2) {
            throw protocol_error("GET requires exactly 1 argument");
        }
        
        string key = cmd.elements[1];
        
        // Redis GET -> SQL SELECT
        return NativeQuery{
            .sql = "SELECT value FROM redis_keyspace WHERE key = ?",
            .params = {key}
        };
    }
    
    NativeQuery translate_set(const RedisMessage& cmd) {
        if (cmd.elements.size() < 3) {
            throw protocol_error("SET requires at least 2 arguments");
        }
        
        string key = cmd.elements[1];
        string value = cmd.elements[2];
        
        // Handle optional EX/PX for expiry
        optional<int> ttl;
        for (size_t i = 3; i < cmd.elements.size(); i += 2) {
            if (cmd.elements[i] == "EX") {
                ttl = stoi(cmd.elements[i + 1]);
            }
        }
        
        // Redis SET -> SQL UPSERT
        return NativeQuery{
            .sql = "INSERT INTO redis_keyspace (key, value, expires_at) "
                   "VALUES (?, ?, ?) "
                   "ON CONFLICT (key) DO UPDATE SET value = ?, expires_at = ?",
            .params = {key, value, calculate_expiry(ttl), value, calculate_expiry(ttl)}
        };
    }
};
```

### 5. Add System Catalog Support

```cpp
// src/protocols/redis/redis_catalog.cpp
class RedisCatalog {
    void initialize_schema(Database* db) {
        // Create Redis-compatible schema
        db->execute(R"(
            CREATE SCHEMA IF NOT EXISTS redis;
            
            CREATE TABLE IF NOT EXISTS redis.keyspace (
                key VARCHAR(512) PRIMARY KEY,
                value BLOB,
                type VARCHAR(16) DEFAULT 'string',
                expires_at TIMESTAMP,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                accessed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE INDEX idx_redis_expires ON redis.keyspace(expires_at);
            
            -- Redis-compatible views
            CREATE VIEW redis.keys AS 
                SELECT key FROM redis.keyspace 
                WHERE expires_at IS NULL OR expires_at > CURRENT_TIMESTAMP;
        )");
    }
    
    void setup_redis_functions(Database* db) {
        // Add Redis-specific functions
        db->register_function("REDIS_TTL", [](const string& key) {
            // Return time-to-live in seconds
        });
        
        db->register_function("REDIS_TYPE", [](const string& key) {
            // Return type of key
        });
    }
};
```

### 6. Handle Protocol-Specific Features

```cpp
// src/protocols/redis/redis_features.cpp
class RedisFeatures {
    // Pub/Sub support
    class PubSubManager {
        multimap<string, tcp::socket*> subscriptions;
        
        void publish(const string& channel, const string& message) {
            auto range = subscriptions.equal_range(channel);
            for (auto it = range.first; it != range.second; ++it) {
                send_message(it->second, format_pubsub_message(channel, message));
            }
        }
        
        void subscribe(tcp::socket* client, const string& channel) {
            subscriptions.emplace(channel, client);
        }
    };
    
    // Transactions (MULTI/EXEC)
    class RedisTransaction {
        vector<NativeQuery> queued_commands;
        
        void multi() {
            queued_commands.clear();
        }
        
        void queue(const NativeQuery& query) {
            queued_commands.push_back(query);
        }
        
        vector<Result> exec() {
            // Execute all queued commands in a transaction
            auto txn = begin_transaction();
            vector<Result> results;
            
            for (const auto& query : queued_commands) {
                results.push_back(execute(query));
            }
            
            commit(txn);
            return results;
        }
    };
};
```

### 7. Write Protocol Tests

```cpp
// tests/protocols/test_redis_protocol.cpp
TEST_F(RedisProtocolTest, BasicCommands) {
    // Connect with Redis client
    auto client = redis_connect("localhost", 6379);
    
    // Test SET
    EXPECT_EQ(redis_set(client, "key1", "value1"), "OK");
    
    // Test GET
    EXPECT_EQ(redis_get(client, "key1"), "value1");
    
    // Test DEL
    EXPECT_EQ(redis_del(client, "key1"), 1);
    
    // Test GET after DEL
    EXPECT_EQ(redis_get(client, "key1"), nullptr);
}

TEST_F(RedisProtocolTest, Expiry) {
    auto client = redis_connect("localhost", 6379);
    
    // SET with expiry
    redis_setex(client, "temp", 1, "value");
    EXPECT_EQ(redis_get(client, "temp"), "value");
    
    // Wait for expiry
    sleep(2);
    EXPECT_EQ(redis_get(client, "temp"), nullptr);
}

TEST_F(RedisProtocolTest, PubSub) {
    auto pub = redis_connect("localhost", 6379);
    auto sub = redis_connect("localhost", 6379);
    
    // Subscribe
    redis_subscribe(sub, "channel1");
    
    // Publish
    redis_publish(pub, "channel1", "message");
    
    // Receive
    auto msg = redis_get_message(sub);
    EXPECT_EQ(msg.channel, "channel1");
    EXPECT_EQ(msg.data, "message");
}
```

### 8. Add Configuration

```yaml
# config/protocols.yaml
protocols:
  redis:
    enabled: true
    port: 6379
    max_clients: 10000
    compatibility_mode: redis_6  # Redis 6.x compatibility
    features:
      pubsub: true
      transactions: true
      lua_scripting: false  # Not implemented yet
      cluster: false        # Not implemented yet
    
    # Redis-specific settings
    maxmemory: 1GB
    maxmemory_policy: lru
    timeout: 300  # Client timeout in seconds
```

### 9. Update Y-Valve Router

```cpp
// src/yvalve/yvalve.cpp
class YValve {
    void initialize_protocols() {
        // Add Redis to supported protocols
        if (config.redis.enabled) {
            auto redis_listener = make_unique<RedisListener>();
            redis_listener->start(config.redis.port);
            listeners.push_back(move(redis_listener));
        }
    }
    
    ClientType detect_client_type(tcp::socket& client) {
        auto initial = client.peek(10);
        
        // Existing detections...
        
        // Redis detection
        if (initial[0] == '*' || initial[0] == '$' ||
            starts_with(initial, "PING") ||
            starts_with(initial, "INFO")) {
            return ClientType::REDIS;
        }
    }
};
```

## Protocol Implementation Checklist

- [ ] Protocol constants defined
- [ ] Message parser implemented
- [ ] Network listener created
- [ ] Y-Valve translator written
- [ ] System catalog initialized
- [ ] Authentication handled
- [ ] Error codes mapped
- [ ] Protocol tests written
- [ ] Client library tested
- [ ] Performance benchmarked
- [ ] Documentation complete

## Common Protocol Patterns

### Request-Response
Most database protocols follow request-response:
```cpp
while (connected) {
    auto request = read_request();
    auto result = process_request(request);
    send_response(result);
}
```

### Pipelining
Some protocols support pipelining:
```cpp
queue<Request> pending;
while (connected) {
    while (has_data()) {
        pending.push(read_request());
    }
    while (!pending.empty()) {
        auto result = process_request(pending.front());
        pending.pop();
        send_response(result);
    }
}
```

### Async Notifications
Some protocols support unsolicited messages:
```cpp
// PostgreSQL LISTEN/NOTIFY
// Redis Pub/Sub
void send_notification(const string& channel, const string& payload) {
    for (auto& client : subscribed_clients[channel]) {
        client.send_async(format_notification(channel, payload));
    }
}
```

## Performance Considerations

1. **Buffer Management**: Use circular buffers for network I/O
2. **Parser Efficiency**: Avoid copying data during parsing
3. **Connection Pooling**: Reuse internal database connections
4. **Caching**: Cache protocol translations
5. **Vectorization**: Batch similar operations

## Security Considerations

1. **Authentication**: Map protocol auth to internal auth
2. **TLS Support**: Implement protocol-specific TLS negotiation
3. **Input Validation**: Validate all protocol messages
4. **Rate Limiting**: Prevent protocol-level DoS
5. **Audit Logging**: Log protocol-specific events