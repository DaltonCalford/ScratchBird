# ScratchBird IPC & Engine Remediation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** 2026-02-06  
**Based on:** ScratchBird_Engine_Agent_Report_2026-02-06.md  
**Status:** VERIFIED & ACTIONABLE (UPDATED WITH TEAM DECISIONS)

---

## Executive Summary

This document provides a comprehensive remediation plan for critical issues identified in the ScratchBird IPC subsystem and external protocol agents. The issues range from build system failures to non-functional IPC transport and stubbed protocol implementations.

**Severity Distribution:**
- **P0 (Blocking):** 4 issues - Build failure, no IPC channels, connection drops, protocol mismatch
- **P1 (Critical):** 7 issues - Flow control dead, parser agents stubbed, COPY non-functional
- **P2 (High):** 1 issue - SCRAM authentication missing
- **Secondary:** Documentation mismatches, platform gaps

**Key Architectural Decisions (see Section 8):**
- IPCChannelFactory implementation stays in `unix_socket_channel.cpp`
- Class name: `UnixSocketIPCChannel`
- IPCChannel is **not thread-safe** - requires explicit locking
- STREAM_CONTROL: Update spec to match credits model (short-term)
- Session lifecycle: ACTIVE → CLOSING → CLOSED state machine

---

## Phase 1: Build System & Foundation (Week 1)

### 1.1 Fix IPC Source Inclusion in CMake
**Priority:** P0 - BLOCKING  
**File:** `src/CMakeLists.txt:559-561`

**Problem:**
```cmake
file(GLOB SCRATCHBIRD_IPC_SOURCES
    ipc/*.cpp
)
```
This glob only includes `src/ipc/*.cpp` but external agents are in `src/ipc/external_agents/*.cpp`.

**Impact:**
- Build fails: `No rule to make target ... src/ipc/copy_flow_control.cpp`
- Parser agents not linked into `scratchbird_ipc`
- COPY flow control absent at runtime

**Fix:**
```cmake
file(GLOB SCRATCHBIRD_IPC_SOURCES CONFIGURE_DEPENDS
    ipc/*.cpp
    ipc/external_agents/*.cpp
)
```

**Testing:**
```bash
cd build && rm -rf *
cmake ..
make scratchbird_ipc -j$(nproc)
nm -C libscratchbird_ipc.a | grep -E "(PostgreSQL|MySQL|Firebird)ParserAgent"
```

---

### 1.2 Fix IPCChannelFactory Duplicate Definition
**Priority:** P0 - BLOCKING  
**Files:** 
- `src/ipc/ipc_contract_v1_1.cpp:258-263` (stub to remove)
- `src/ipc/unix_socket_channel.cpp` (canonical implementation)

**Decision:** Keep the real implementation in `unix_socket_channel.cpp`. Remove/redirect the stub in `ipc_contract_v1_1.cpp` to avoid duplicate symbols.

**Rationale:** Keeps channel-specific logic next to the channel implementation.

**Patch for `src/ipc/ipc_contract_v1_1.cpp`:**
```cpp
std::unique_ptr<IPCChannel> IPCChannelFactory::create(IPCChannelType type) {
    // Implementation lives in unix_socket_channel.cpp
    return IPCChannelFactory::createDefault();
}
```

**Note:** Do NOT implement the factory here. The working implementation is already in `unix_socket_channel.cpp`.

---

### 1.3 Fix IPC Server Accept Loop
**Priority:** P0 - BLOCKING  
**File:** `src/ipc/ipc_server.cpp:614-638`

**Current Code (Stub):**
```cpp
void IPCServer::acceptLoop() {
    while (running_) {
        int client_fd = accept(listen_fd_, ...);
        // STUB: Just close the connection!
        ::close(client_fd);
        stats_.total_sessions++;  // False statistic
    }
}
```

**Implementation:**

1. **Add to `IPCServer` header** (`include/scratchbird/ipc/ipc_server.h`):
```cpp
private:
    void sessionReadLoop(uint32_t session_id);
    void enqueueMessage(uint32_t session_id, IPCMessage&& msg);
    uint32_t createSession(std::unique_ptr<IPCChannel> channel);
    void destroySession(uint32_t session_id);
```

2. **Update accept loop** in `src/ipc/ipc_server.cpp`:
```cpp
#include "scratchbird/ipc/unix_socket_channel.h"

void IPCServer::acceptLoop() {
    while (running_) {
#if defined(__linux__) || defined(__APPLE__)
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!running_) break;
            continue;
        }

        // Create channel and accept connection
        auto channel = std::make_unique<UnixSocketIPCChannel>();
        core::ErrorContext ctx;
        if (channel->accept(client_fd, &ctx) != core::Status::OK) {
            ::close(client_fd);
            continue;
        }

        // Create session
        uint32_t session_id = createSession(std::move(channel));

        // Start reader thread for this session
        std::thread([this, session_id]() { sessionReadLoop(session_id); }).detach();
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.active_sessions++;
        stats_.total_sessions++;
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }
}
```

3. **Add session management methods:**
```cpp
uint32_t IPCServer::createSession(std::unique_ptr<IPCChannel> channel) {
    uint32_t session_id = next_session_id_++;
    auto session = std::make_shared<IPCSession>(
        session_id,
        std::move(channel),
        handler_,
        this
    );
    
    std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
    sessions_[session_id] = std::move(session);
    return session_id;
}

void IPCServer::destroySession(uint32_t session_id) {
    std::shared_ptr<IPCSession> session;
    {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second;
        }
    }
    
    if (session) {
        session->setState(SessionState::CLOSING);
        session->shutdown();
        
        std::lock_guard<std::shared_mutex> lock(sessions_mutex_);
        sessions_.erase(session_id);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.active_sessions--;
    }
}

void IPCServer::enqueueMessage(uint32_t session_id, IPCMessage&& msg) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push({session_id, std::move(msg)});
    }
    queue_cv_.notify_one();
}
```

---

### 1.4 Add Missing Reader Loop
**Priority:** P0 - BLOCKING  
**Rationale:** `IPCServer::workerLoop()` consumes `message_queue_`, but nothing reads from IPC channels to populate the queue.

**Implementation:**

1. **Add to `IPCSession` header** (`include/scratchbird/ipc/ipc_server.h`):
```cpp
// Session state machine
enum class SessionState {
    ACTIVE,
    CLOSING,
    CLOSED
};

class IPCSession {
public:
    // ... existing methods ...
    
    SessionState getState() const { return state_; }
    void setState(SessionState state) { state_ = state; }
    bool isActive() const { return state_ == SessionState::ACTIVE; }
    void shutdown();
    
protected:
    IPCChannel* getChannel() const { return channel_.get(); }
    
private:
    SessionState state_ = SessionState::ACTIVE;
    std::mutex io_mutex_;  // Protects channel I/O (thread safety)
};
```

2. **Add reader loop** in `src/ipc/ipc_server.cpp`:
```cpp
void IPCServer::sessionReadLoop(uint32_t session_id) {
    std::shared_ptr<IPCSession> session;
    {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second;
        }
    }

    if (!session || !session->isActive()) return;

    core::ErrorContext ctx;
    while (running_ && session->isActive()) {
        IPCMessage msg;
        
        // Lock during receive (channel is not thread-safe)
        {
            std::lock_guard<std::mutex> io_lock(session->io_mutex_);
            auto status = session->getChannel()->receive(msg, &ctx);
            if (status != core::Status::OK) {
                break; // disconnect or error
            }
        }
        
        enqueueMessage(session_id, std::move(msg));
    }

    // Cleanup on disconnect
    destroySession(session_id);
}
```

3. **Add shutdown method** in `src/ipc/ipc_server.cpp`:
```cpp
void IPCSession::shutdown() {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (channel_) {
        channel_->close();
    }
    state_ = SessionState::CLOSED;
}
```

---

### 1.5 Thread Safety for IPCChannel I/O
**Priority:** P0 - BLOCKING  
**Decision:** Treat IPCChannel as **not thread-safe**. Add per-session mutex for all channel I/O.

**Implementation:**

All channel operations must hold `io_mutex_`:

```cpp
// In IPCSession methods
core::Status IPCSession::sendMessage(const IPCMessage& msg, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(io_mutex_);
    if (!channel_ || !channel_->isConnected()) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Channel not connected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    return channel_->send(msg, ctx);
}
```

**Note:** The reader loop (1.4) already holds `io_mutex_` during receive. Send operations must also hold it.

---

## Phase 2: IPC Contract (Week 1-2)

### 2.1 STREAM_CONTROL Handler Implementation
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/ipc_server.cpp:72-119`

**Decision:** The current code uses a **credits model**. Update the specification to match the implementation (short-term). Long-term can migrate to windowed model if needed.

**Implementation:**

1. **Add to switch statement** in `IPCSession::handleMessage`:
```cpp
case IPCMessageType::STREAM_CONTROL:
    return handleStreamControl(msg, ctx);
```

2. **Add handler declaration** in `include/scratchbird/ipc/ipc_server.h`:
```cpp
private:
    core::Status handleStreamControl(const IPCMessage& msg, core::ErrorContext* ctx);
    
    struct FlowControlState {
        int32_t credits = 0;          // Positive=grant, negative=revoke
        uint32_t buffer_avail = 0;    // Available buffer space
        std::chrono::steady_clock::time_point last_update;
        std::mutex mutex;
        std::condition_variable cv;
    } flow_control_;
```

3. **Add handler implementation** in `src/ipc/ipc_server.cpp`:
```cpp
core::Status IPCSession::handleStreamControl(const IPCMessage& msg, 
                                             core::ErrorContext* ctx) {
    if (msg.payload.size() < sizeof(IPCStreamControlPayload)) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "Invalid STREAM_CONTROL payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    const auto* payload = reinterpret_cast<const IPCStreamControlPayload*>(
        msg.payload.data()
    );
    
    {
        std::lock_guard<std::mutex> lock(flow_control_.mutex);
        flow_control_.credits = payload->credits;
        flow_control_.buffer_avail = payload->buffer_avail;
        flow_control_.last_update = std::chrono::steady_clock::now();
    }
    flow_control_.cv.notify_all();
    
    return core::Status::OK;
}
```

4. **Update spec document** (`/docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md:430-435`):
```
STREAM_CONTROL payload (credits model):
int32 credits          // Positive=grant, negative=revoke
uint32 buffer_avail    // Available buffer space
```

---

## Phase 3: COPY Flow Control (Week 2)

### 3.1 Fix COPY Flow Control Logic
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/external_agents/copy_flow_control.cpp:272-338`

**Issues to Fix:**
1. **Off-by-one error:** Line 284 - use `header->length` instead of size calculation
2. **Credits never decremented:** Flow control doesn't actually throttle

**Implementation:**

```cpp
core::Status IPCSessionWithFlowControl::handleCopyDataWithFlowControl(
    const IPCMessage& msg,
    core::ErrorContext* ctx
) {
    // Validate payload
    if (msg.payload.size() < sizeof(IPCCopyDataPayload)) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    const auto* header = reinterpret_cast<const IPCCopyDataPayload*>(
        msg.payload.data()
    );
    
    // FIX: Use declared length from header (not size calculation)
    uint32_t data_len = header->length;
    
    // Validate length
    if (data_len > msg.payload.size() - sizeof(IPCCopyDataPayload)) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "COPY data length exceeds payload",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Get or create flow controller
    auto controller = CopyFlowControlManager::instance().getController(getId());
    if (!controller) {
        controller = CopyFlowControlManager::instance().createController(
            getId(), 10, 1024 * 1024);
    }
    
    // Check credits before accepting data
    if (!controller->canSend(data_len)) {
        // Send flow control pause
        IPCMessage control_msg;
        control_msg.setType(IPCMessageType::STREAM_CONTROL);
        control_msg.header.request_id = msg.header.request_id;
        
        IPCStreamControlPayload control_payload;
        control_payload.credits = 0;
        control_payload.buffer_avail = controller->getStats().buffer_available;
        
        control_msg.payload.resize(sizeof(control_payload));
        std::memcpy(control_msg.payload.data(), &control_payload, sizeof(control_payload));
        
        sendMessage(control_msg, ctx);
        
        // Wait for credits
        if (!controller->waitForCreditsWithTimeout(1, data_len, 30000)) {
            return core::Status::LOCK_TIMEOUT;
        }
    }
    
    // FIX: Decrement credits when accepting data
    controller->recordReceived(data_len);
    
    // Process the COPY data
    const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(header) + sizeof(IPCCopyDataPayload);
    if (getHandler()) {
        auto status = getHandler()->onCopyData(getId(), data_ptr, data_len, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    
    // Send updated flow control
    auto stats = controller->getStats();
    
    IPCMessage control_msg;
    control_msg.setType(IPCMessageType::STREAM_CONTROL);
    control_msg.header.request_id = msg.header.request_id;
    
    IPCStreamControlPayload control_payload;
    control_payload.credits = stats.credits_available;
    control_payload.buffer_avail = stats.buffer_available;
    
    control_msg.payload.resize(sizeof(control_payload));
    std::memcpy(control_msg.payload.data(), &control_payload, sizeof(control_payload));
    
    return sendMessage(control_msg, ctx);
}
```

---

### 3.2 Wire Flow Control into IPCSession
**Priority:** P1 - CRITICAL  

**Implementation:**

Enable flow control by default for COPY operations:

```cpp
// In IPCSession::handleCopyData
core::Status IPCSession::handleCopyData(const IPCMessage& msg, core::ErrorContext* ctx) {
    // Enable flow control for COPY operations
    if (!flow_controller_) {
        flow_controller_ = CopyFlowControlManager::instance().createController(
            getId(), 10, 1024 * 1024);
    }
    
    return handleCopyDataWithFlowControl(msg, ctx);
}
```

---

## Phase 4: Parser Agent Implementation (Week 2-3)

### 4.1 Implement sendToEngine/receiveFromEngine
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/parser_agent.cpp:288-303`

**Decision:** Use `IPCChannel::send/receive(IPCMessage&)` directly. Do not hand-serialize headers.

**Implementation:**

```cpp
core::Status ParserAgent::sendToEngine(uint32_t client_id,
                                      const IPCMessage& msg,
                                      core::ErrorContext* ctx) {
    auto channel = getChannelForClient(client_id);
    if (!channel) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "No IPC channel for client",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Use IPCChannel API directly
    return channel->send(msg, ctx);
}

core::Status ParserAgent::receiveFromEngine(uint32_t client_id,
                                           IPCMessage& msg,
                                           core::ErrorContext* ctx,
                                           uint32_t timeout_ms) {
    auto channel = getChannelForClient(client_id);
    if (!channel) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "No IPC channel for client",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Use IPCChannel API directly
    return channel->receive(msg, ctx, timeout_ms);
}
```

---

### 4.2 Replace Simulated Responses in PostgreSQL Parser Agent
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/external_agents/postgresql_parser_agent.cpp:460-489`

**Implementation:**

```cpp
core::Status PostgreSQLParserAgent::handleQueryMessage(PGClientState& state, 
                                                       const std::vector<uint8_t>& msg,
                                                       core::ErrorContext* ctx) {
    if (msg.size() < 5) {
        return sendErrorResponse(state, "08P01", "Invalid query message");
    }
    
    const char* sql = reinterpret_cast<const char*>(msg.data() + 5);
    
    // Create IPC message
    IPCMessage ipc_msg;
    ipc_msg.setType(IPCMessageType::SIMPLE_QUERY);
    
    // Build payload
    IPCSimpleQueryPayload query_payload;
    query_payload.query_length = std::strlen(sql);
    
    ipc_msg.payload.resize(sizeof(query_payload) + query_payload.query_length);
    std::memcpy(ipc_msg.payload.data(), &query_payload, sizeof(query_payload));
    std::memcpy(ipc_msg.payload.data() + sizeof(query_payload), 
                sql, query_payload.query_length);
    
    // Send to engine via IPC
    auto status = sendToEngine(state.client_id, ipc_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "58000", "Failed to send query to engine");
    }
    
    // Receive response
    IPCMessage response;
    status = receiveFromEngine(state.client_id, response, ctx, 30000);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "58000", "Failed to receive response from engine");
    }
    
    // Translate and send response to client
    return translateAndSendResponse(state, response, ctx);
}
```

---

### 4.3 Implement Message Translation Functions
**Priority:** P1 - CRITICAL  
**Files:** All parser agent files

**Implementation Pattern:**

```cpp
core::Status PostgreSQLParserAgent::translateAndSendResponse(
    PGClientState& state,
    const IPCMessage& ipc_response,
    core::ErrorContext* ctx
) {
    switch (ipc_response.getType()) {
        case IPCMessageType::ROW_DESCRIPTION:
            return translateRowDescription(state, ipc_response, ctx);
        case IPCMessageType::DATA_ROW:
            return translateDataRow(state, ipc_response, ctx);
        case IPCMessageType::COMMAND_COMPLETE:
            return translateCommandComplete(state, ipc_response, ctx);
        case IPCMessageType::ERROR_RESPONSE:
            return translateErrorResponse(state, ipc_response, ctx);
        case IPCMessageType::READY_FOR_QUERY:
            return translateReadyForQuery(state, ipc_response, ctx);
        default:
            return sendErrorResponse(state, "08P01", 
                "Unexpected response type from engine");
    }
}
```

---

### 4.4 Implement COPY Forwarding (PostgreSQL)
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/external_agents/postgresql_parser_agent.cpp:769-795`

**Implementation:**

```cpp
core::Status PostgreSQLParserAgent::handleCopyDataMessage(
    PGClientState& state,
    const std::vector<uint8_t>& msg,
    core::ErrorContext* ctx
) {
    if (msg.size() < 5) {
        return sendErrorResponse(state, "08P01", "Invalid COPY data message");
    }
    
    uint32_t length = ntohl(*reinterpret_cast<const uint32_t*>(msg.data() + 1));
    const uint8_t* data = msg.data() + 5;
    uint32_t data_len = length - 4;
    
    // Create IPC COPY_DATA message
    IPCMessage ipc_msg;
    ipc_msg.setType(IPCMessageType::COPY_DATA);
    
    IPCCopyDataPayload header;
    header.chunk_id = state.copy_chunk_id++;
    header.length = data_len;
    
    ipc_msg.payload.resize(sizeof(header) + data_len);
    std::memcpy(ipc_msg.payload.data(), &header, sizeof(header));
    std::memcpy(ipc_msg.payload.data() + sizeof(header), data, data_len);
    
    // Send to engine with flow control
    return sendToEngineWithFlowControl(state.client_id, ipc_msg, ctx);
}
```

---

## Phase 5: Engine COPY Handling (Week 3)

### 5.1 Stream COPY Data Instead of Buffering
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/external_agents/engine_ipc_session_handler.cpp:914-934`

**Implementation:**

```cpp
core::Status EngineIPCSessionHandler::onCopyData(uint32_t session_id,
                                                const uint8_t* data, size_t len,
                                                core::ErrorContext* ctx) {
    SessionState* session = getSession(session_id);
    if (!session) {
        return core::Status::NOT_FOUND;
    }
    
    if (!session->in_copy_in && !session->in_copy_out) {
        return sendError(session_id, "57014", "Not in COPY mode");
    }
    
    if (session->in_copy_in) {
        // Stream to executor instead of buffering
        if (session->copy_executor) {
            auto status = session->copy_executor->processData(data, len, ctx);
            
            if (status != core::Status::OK) {
                return status;
            }
            
            // Update statistics
            session->copy_stats.rows_processed += estimateRowCount(data, len);
            session->copy_stats.bytes_processed += len;
        } else {
            // Small temporary buffer only
            if (session->copy_buffer.size() + len > MAX_COPY_BUFFER_SIZE) {
                auto status = flushCopyBuffer(session);
                if (status != core::Status::OK) {
                    return status;
                }
            }
            session->copy_buffer.insert(session->copy_buffer.end(), data, data + len);
        }
        
        // Send flow control update
        if (shouldSendFlowControlUpdate(session)) {
            sendFlowControlUpdate(session_id);
        }
    }
    
    return core::Status::OK;
}
```

---

## Phase 6: Native SB Parser Agent (Week 3-4)

### 6.1 Implement Native SB Protocol Operations
**Priority:** P1 - CRITICAL  
**File:** `src/ipc/parser_agent.cpp:581-721`

**Implementation Priority:**
1. `handleQuery` - Basic query execution
2. `handleParse` - Statement preparation
3. `handleBind` - Parameter binding
4. `handleExecute` - Statement execution
5. `handleClose` - Resource cleanup

**Example:**
```cpp
core::Status NativeSBParserAgent::handleQuery(ClientConnection& client, 
                                             const std::string& sql,
                                             core::ErrorContext* ctx) {
    IPCMessage msg;
    msg.setType(IPCMessageType::SIMPLE_QUERY);
    
    IPCSimpleQueryPayload payload;
    payload.query_length = sql.length();
    
    msg.payload.resize(sizeof(payload) + sql.length());
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    std::memcpy(msg.payload.data() + sizeof(payload), sql.data(), sql.length());
    
    auto status = sendToEngine(client.id, msg, ctx);
    if (status != core::Status::OK) {
        return sendError(client, status, "Failed to send query", ctx);
    }
    
    // Receive and forward responses until READY_FOR_QUERY
    IPCMessage response;
    while (receiveFromEngine(client.id, response, ctx, 30000) == core::Status::OK) {
        status = forwardResponseToClient(client, response, ctx);
        if (status != core::Status::OK) return status;
        if (response.getType() == IPCMessageType::READY_FOR_QUERY) break;
    }
    
    return core::Status::OK;
}
```

---

## Phase 7: UDR Authentication (Week 4)

### 7.1 Implement SCRAM-SHA-256 for PostgreSQL UDR
**Priority:** P2 - HIGH  
**File:** `src/udr/postgresql_udr.cpp:328-340`

**Implementation:**

```cpp
core::Status PostgreSQLConnection::handleAuthSASL(const std::string& password,
                                                 const std::vector<uint8_t>& data,
                                                 core::ErrorContext* ctx) {
    // Parse SASL mechanisms from server
    std::vector<std::string> mechanisms = parseSASLMechanisms(data);
    
    if (std::find(mechanisms.begin(), mechanisms.end(), "SCRAM-SHA-256") == mechanisms.end()) {
        if (ctx) {
            ctx->set(core::Status::NOT_SUPPORTED,
                    "Server does not support SCRAM-SHA-256",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_SUPPORTED;
    }
    
    // Initialize SCRAM-SHA-256 client
    ScramSha256Client scram_client;
    
    // client-first-message
    std::string client_first = scram_client.generateClientFirst(username_);
    
    std::vector<uint8_t> payload(client_first.begin(), client_first.end());
    auto status = writeMessage(pg::MSG_PASSWORD_MESSAGE, payload, ctx);
    if (status != core::Status::OK) return status;
    
    // Receive server-first-message
    std::vector<uint8_t> server_response;
    status = readMessage(server_response, ctx);
    if (status != core::Status::OK) return status;
    
    std::string server_first(server_response.begin(), server_response.end());
    
    // client-final-message
    std::string client_final = scram_client.generateClientFinal(server_first, password);
    
    payload.assign(client_final.begin(), client_final.end());
    status = writeMessage(pg::MSG_PASSWORD_MESSAGE, payload, ctx);
    if (status != core::Status::OK) return status;
    
    // Receive server-final-message and verify
    status = readMessage(server_response, ctx);
    if (status != core::Status::OK) return status;
    
    std::string server_final(server_response.begin(), server_response.end());
    
    if (!scram_client.verifyServerSignature(server_final)) {
        if (ctx) {
            ctx->set(core::Status::AUTH_FAILURE,
                    "SCRAM-SHA-256 server signature verification failed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::AUTH_FAILURE;
    }
    
    return core::Status::OK;
}
```

**Note:** Implement `ScramSha256Client` class or use libsasl2.

---

## 8. Architectural Decisions Summary

This section documents the key design decisions made by the team.

### 8.1 IPCChannelFactory Location
- **Decision:** Keep implementation in `unix_socket_channel.cpp`, remove stub from `ipc_contract_v1_1.cpp`
- **Rationale:** Avoids duplicate symbols; keeps channel-specific logic with channel implementation

### 8.2 Class Naming
- **Decision:** Use `UnixSocketIPCChannel` everywhere
- **Rationale:** Matches current header and implementation

### 8.3 Thread Safety
- **Decision:** Treat `IPCChannel` as **not thread-safe**
- **Implementation:** Add `io_mutex_` to `IPCSession` for all channel I/O
- **Rationale:** Safest assumption; avoids undefined concurrency on socket I/O

### 8.4 STREAM_CONTROL Payload Model
- **Decision (Short-term):** Update spec to match current **credits model**
- **Decision (Long-term):** Optional migration to windowed model if team prefers
- **Rationale:** Fastest path to working system; credits model already implemented

### 8.5 Session Lifecycle Management
- **Decision:** Add simple state machine: **ACTIVE → CLOSING → CLOSED**
- **Implementation:** 
  - `destroySession()` sets CLOSING, stops I/O
  - Erases session after reader thread exits
- **Rationale:** Avoids races between message processing and session teardown

### 8.6 ParserAgent API
- **Decision:** Use `IPCChannel::send/receive(IPCMessage&)` directly
- **Rationale:** Matches the interface; avoids divergence with transport implementation

### 8.7 Missing Reader Loop
- **Decision:** Implement per-session reader loop in `IPCServer`
- **Implementation:** 
  - `sessionReadLoop()` calls `getChannel()->receive()` and enqueues to `message_queue_`
  - Add protected `IPCSession::getChannel()` accessor
- **Rationale:** Worker threads already assume centralized queue; preserves that design

---

## Implementation Checklist

### Phase 1: Build System & Foundation
- [ ] 1.1 Fix CMake IPC source inclusion (add `ipc/external_agents/*.cpp`)
- [ ] 1.1 Add `CONFIGURE_DEPENDS` to GLOB
- [ ] 1.2 Fix IPCChannelFactory duplicate (remove stub, keep unix_socket_channel.cpp impl)
- [ ] 1.3 Fix IPC server accept loop (use `UnixSocketIPCChannel::accept()`)
- [ ] 1.3 Add `createSession()`, `destroySession()`, `enqueueMessage()`
- [ ] 1.4 Add `sessionReadLoop()` to populate message queue
- [ ] 1.4 Add `SessionState` enum (ACTIVE, CLOSING, CLOSED)
- [ ] 1.4 Add `IPCSession::getChannel()` accessor
- [ ] 1.5 Add `io_mutex_` to IPCSession for thread-safe channel I/O

### Phase 2: IPC Contract
- [ ] 2.1 Add STREAM_CONTROL handler to message dispatch switch
- [ ] 2.1 Add `handleStreamControl()` method
- [ ] 2.1 Add `FlowControlState` struct with credits model
- [ ] 2.2 Update spec document to match credits model

### Phase 3: COPY Flow Control
- [ ] 3.1 Fix data length calculation (use `header->length`)
- [ ] 3.1 Add credit decrement in `recordReceived()`
- [ ] 3.2 Wire flow control into IPCSession

### Phase 4: Parser Agents
- [ ] 4.1 Implement `sendToEngine()` using `IPCChannel::send()`
- [ ] 4.1 Implement `receiveFromEngine()` using `IPCChannel::receive()`
- [ ] 4.2 Replace PostgreSQL simulated responses with real IPC
- [ ] 4.3 Replace MySQL simulated responses with real IPC
- [ ] 4.4 Implement message translation functions
- [ ] 4.5 Implement COPY forwarding (PostgreSQL)

### Phase 5: Engine COPY
- [ ] 5.1 Stream COPY data to executor (don't buffer all)
- [ ] 5.1 Add flow control integration

### Phase 6: Native SB Agent
- [ ] 6.1 Implement `handleQuery`
- [ ] 6.1 Implement `handleParse`
- [ ] 6.1 Implement `handleBind`
- [ ] 6.1 Implement `handleExecute`
- [ ] 6.1 Implement `handleClose`

### Phase 7: UDR Authentication
- [ ] 7.1 Implement SCRAM-SHA-256 client
- [ ] 7.1 Add `handleAuthSASL()` implementation

---

## Testing Strategy

### Unit Tests
1. **IPC Channel Factory:** Verify channel creation
2. **Session State Machine:** Test ACTIVE → CLOSING → CLOSED transitions
3. **Thread Safety:** Verify mutex protects channel I/O
4. **Flow Control:** Test credit-based throttling

### Integration Tests
1. **End-to-End Query:** Client → Parser Agent → IPC → Engine → Response
2. **COPY Flow:** Large data transfer with backpressure
3. **Concurrent Sessions:** Multiple clients, thread safety verification
4. **Session Cleanup:** Verify no leaks on disconnect

### Protocol Compliance
1. **PostgreSQL:** Use pgproto test suite
2. **MySQL:** Use mysqltest
3. **Native SB:** Custom test suite

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Thread safety bugs | Medium | High | TSAN testing, code review |
| Session lifecycle races | Medium | High | State machine, proper cleanup |
| IPC performance issues | Medium | High | Benchmark early, optimize later |
| Protocol compatibility | High | Medium | Extensive compliance testing |
| Memory leaks | Medium | High | Valgrind/ASan testing |

---

## Timeline Summary

| Phase | Duration | Key Deliverables |
|-------|----------|------------------|
| 1 | Week 1 | Build fixes, functional IPC with thread safety |
| 2 | Week 1-2 | Contract alignment, STREAM_CONTROL handler |
| 3 | Week 2 | Working COPY flow control |
| 4 | Week 2-3 | Parser agents with real IPC |
| 5 | Week 3 | Streaming COPY handling |
| 6 | Week 3-4 | Native SB protocol support |
| 7 | Week 4 | SCRAM-SHA-256 authentication |

**Total Estimated Duration:** 4 weeks with 1-2 developers

---

## Appendix: File References

### Build/CMake
- `src/CMakeLists.txt:559-561` - IPC source glob
- `tests/CMakeLists.txt:1892-1900` - Test linkage

### IPC Core
- `src/ipc/ipc_contract_v1_1.cpp:258-263` - Factory stub (to remove)
- `src/ipc/unix_socket_channel.cpp` - Factory implementation (canonical)
- `src/ipc/ipc_server.cpp:614-630` - Accept loop
- `src/ipc/ipc_server.cpp` - Add sessionReadLoop, state machine
- `include/scratchbird/ipc/ipc_server.h` - Add SessionState, getChannel()
- `include/scratchbird/ipc/ipc_contract_v1_1.h:268-272` - STREAM_CONTROL payload

### Flow Control
- `src/ipc/external_agents/copy_flow_control.cpp:272-338` - Flow control logic
- `/docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md:430-435` - Spec (update to credits)

### Parser Agents
- `src/ipc/parser_agent.cpp:288-303` - IPC send/receive stubs
- `src/ipc/parser_agent.cpp:581-721` - Native SB stubs
- `src/ipc/external_agents/postgresql_parser_agent.cpp:460-489` - Simulated query
- `src/ipc/external_agents/postgresql_parser_agent.cpp:769-795` - COPY stubs
- `src/ipc/external_agents/mysql_parser_agent.cpp:574-596` - Simulated query
- `src/ipc/external_agents/firebird_parser_agent.cpp:814-829` - Translation stub

### Engine COPY
- `src/ipc/external_agents/engine_ipc_session_handler.cpp:914-934` - COPY buffering

### UDR
- `src/udr/postgresql_udr.cpp:328-340` - SCRAM-SHA-256 stub

### Documentation
- `PROJECT_CONTEXT.md:179-191` - Path mismatches (use UnixSocketIPCChannel)
