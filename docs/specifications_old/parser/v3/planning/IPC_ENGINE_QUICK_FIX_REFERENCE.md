# IPC & Engine Issues - Quick Fix Reference

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**For:** Immediate action by developers  
**Companion:** See `IPC_ENGINE_REMEDIATION_PLAN_2026-02-06.md` for full details  
**Status:** Updated with team decisions (2026-02-06)

---

## Team Decisions Summary

| Decision | Choice |
|----------|--------|
| **IPCChannelFactory** | Keep in `unix_socket_channel.cpp`, remove stub from `ipc_contract_v1_1.cpp` |
| **Class name** | `UnixSocketIPCChannel` |
| **Thread safety** | `IPCChannel` is NOT thread-safe; use `io_mutex_` per session |
| **STREAM_CONTROL** | Keep credits model, update spec to match |
| **Session lifecycle** | ACTIVE → CLOSING → CLOSED state machine |
| **ParserAgent API** | Use `IPCChannel::send/receive(IPCMessage&)` directly |

---

## Quick Fixes (Can Apply Today)

### Fix 1: CMake IPC Sources (5 minutes)

**File:** `src/CMakeLists.txt:559`

**Change FROM:**
```cmake
file(GLOB SCRATCHBIRD_IPC_SOURCES
    ipc/*.cpp
)
```

**Change TO:**
```cmake
file(GLOB SCRATCHBIRD_IPC_SOURCES CONFIGURE_DEPENDS
    ipc/*.cpp
    ipc/external_agents/*.cpp
)
```

**Verify:**
```bash
cd build && cmake .. && make scratchbird_ipc 2>&1 | grep -E "(error|Error)" || echo "Build OK"
```

---

### Fix 2: Remove IPCChannelFactory Stub (5 minutes)

**File:** `src/ipc/ipc_contract_v1_1.cpp:258-263`

**Current (Stub):**
```cpp
std::unique_ptr<IPCChannel> IPCChannelFactory::create(IPCChannelType type) {
    // Channel implementations will be created in separate files
    // This is a stub that returns nullptr for now
    (void)type;
    return nullptr;
}
```

**Replace WITH:**
```cpp
std::unique_ptr<IPCChannel> IPCChannelFactory::create(IPCChannelType type) {
    // Implementation lives in unix_socket_channel.cpp
    return IPCChannelFactory::createDefault();
}
```

**Rationale:** The real implementation is already in `unix_socket_channel.cpp`. This avoids duplicate symbols.

---

### Fix 3: IPC Server Accept Loop with Session State Machine (1 hour)

**Files:** 
- `src/ipc/ipc_server.cpp`
- `include/scratchbird/ipc/ipc_server.h`

#### 3a. Add Session State to Header

**File:** `include/scratchbird/ipc/ipc_server.h`

```cpp
// Add before IPCSession class
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
    std::mutex& getIOMutex() { return io_mutex_; }
    
private:
    SessionState state_ = SessionState::ACTIVE;
    std::mutex io_mutex_;  // Protects channel I/O
};
```

#### 3b. Add Private Methods to IPCServer

```cpp
class IPCServer {
    // ... existing methods ...
    
private:
    void sessionReadLoop(uint32_t session_id);
    void enqueueMessage(uint32_t session_id, IPCMessage&& msg);
    uint32_t createSession(std::unique_ptr<IPCChannel> channel);
    void destroySession(uint32_t session_id);
    
    uint32_t next_session_id_ = 1;
    std::shared_mutex sessions_mutex_;  // Use shared_mutex for read-heavy ops
};
```

#### 3c. Update Accept Loop

**File:** `src/ipc/ipc_server.cpp`

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

#### 3d. Add Session Management Methods

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
            std::lock_guard<std::mutex> io_lock(session->getIOMutex());
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

#### 3e. Add IPCSession::shutdown()

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

### Fix 4: Thread-Safe sendMessage (15 minutes)

**File:** `src/ipc/ipc_server.cpp`

Update `IPCSession::sendMessage()` to use `io_mutex_`:

```cpp
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

---

### Fix 5: Add STREAM_CONTROL Handler (30 minutes)

**File:** `src/ipc/ipc_server.cpp:72-119`

**Add to switch statement:**
```cpp
case IPCMessageType::STREAM_CONTROL:
    return handleStreamControl(msg, ctx);
```

**Add handler declaration** in `include/scratchbird/ipc/ipc_server.h`:
```cpp
private:
    core::Status handleStreamControl(const IPCMessage& msg, core::ErrorContext* ctx);
    
    struct FlowControlState {
        int32_t credits = 0;          // Credits model (not windowed)
        uint32_t buffer_avail = 0;
        std::chrono::steady_clock::time_point last_update;
        std::mutex mutex;
        std::condition_variable cv;
    } flow_control_;
```

**Add implementation** in `src/ipc/ipc_server.cpp`:
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

**Note:** Uses **credits model** (not windowed). Update spec document separately.

---

### Fix 6: ParserAgent send/receive (30 minutes)

**File:** `src/ipc/parser_agent.cpp:288-303`

**Replace stubs WITH:**
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

## Dependency Chain

```
Fix 1 (CMake)
    ↓
Fix 2 (Remove Factory Stub)
    ↓
Fix 3 (Accept Loop + State Machine)
    ↓
Fix 4 (Thread-Safe I/O)
    ↓
Fix 5 (STREAM_CONTROL)
    ↓
Fix 6 (Parser Agents)
```

**Do NOT start on parser agents until IPC foundation is fixed.**

---

## Testing After Each Fix

### After Fix 1 (CMake):
```bash
cd build
rm -rf *
cmake ..
make -j$(nproc)
# Should compile without "No rule to make target" errors
```

### After Fix 2-4 (IPC Foundation):
```cpp
// Test: Basic session lifecycle
1. Start IPC server
2. Connect client via Unix socket
3. Verify session state is ACTIVE
4. Send STARTUP message
5. Verify response received
6. Disconnect
7. Verify session cleanup (state CLOSED, erased)
```

### After Fix 5 (STREAM_CONTROL):
```cpp
// Test: Flow control message handling
1. Send STREAM_CONTROL with credits=10
2. Verify credits stored in session
3. Send COPY_DATA
4. Verify credits decremented
```

---

## Estimated Time to Functional IPC

| Fix | Time | Cumulative |
|-----|------|------------|
| 1. CMake | 5 min | 5 min |
| 2. Remove Stub | 5 min | 10 min |
| 3. Accept Loop + State Machine | 1 hour | 1h 10m |
| 4. Thread-Safe I/O | 15 min | 1h 25m |
| 5. STREAM_CONTROL | 30 min | 1h 55m |
| Testing | 30 min | 2h 25m |

**Total: ~2.5 hours for basic functional IPC**

---

## Red Flags to Watch For

1. **Deadlocks:** Ensure `io_mutex_` is released before calling external callbacks
2. **Session leaks:** Verify `destroySession()` is always called (even on error paths)
3. **Use-after-free:** Check session state before accessing in reader loop
4. **Double-close:** Ensure only `shutdown()` closes the channel

---

## Common Mistakes

### ❌ Wrong: Using UnixSocketChannel
```cpp
auto channel = std::make_unique<UnixSocketChannel>();  // WRONG CLASS NAME
```

### ✅ Correct: Using UnixSocketIPCChannel
```cpp
auto channel = std::make_unique<UnixSocketIPCChannel>();  // CORRECT
```

### ❌ Wrong: Unprotected channel I/O
```cpp
auto status = session->getChannel()->receive(msg, &ctx);  // NOT THREAD-SAFE
```

### ✅ Correct: Protected channel I/O
```cpp
{
    std::lock_guard<std::mutex> lock(session->getIOMutex());
    auto status = session->getChannel()->receive(msg, &ctx);
}
```

### ❌ Wrong: Manual serialization in ParserAgent
```cpp
std::vector<uint8_t> buffer;
msg.serialize(buffer);  // DON'T DO THIS
channel->send(buffer.data(), buffer.size(), ctx);
```

### ✅ Correct: Use IPCChannel API directly
```cpp
channel->send(msg, ctx);  // Use high-level API
```

---

## Contact

For questions about these fixes, refer to:
- Full plan: `IPC_ENGINE_REMEDIATION_PLAN_2026-02-06.md`
- Verification: `IPC_ENGINE_VERIFICATION_SUMMARY_2026-02-06.md`
- Original audit: `~/CliWork/ScratchBird_Engine_Agent_Report_2026-02-06.md`
