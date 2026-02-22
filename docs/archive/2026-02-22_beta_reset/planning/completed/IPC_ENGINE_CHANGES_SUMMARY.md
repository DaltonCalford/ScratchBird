# IPC & Engine Planning Documents - Changes Summary

**Date:** 2026-02-06  
**Status:** Planning documents updated with team decisions

---

## Overview

The planning documents have been updated to reflect architectural decisions made by the development team. This summary describes what was changed and why.

---

## Documents Updated

| Document | Changes |
|----------|---------|
| `IPC_ENGINE_REMEDIATION_PLAN_2026-02-06.md` | Fully revised with team decisions, removed addendum (integrated), added Section 8 (Architectural Decisions Summary) |
| `IPC_ENGINE_QUICK_FIX_REFERENCE.md` | Updated all code examples to use correct class names and patterns, added team decisions summary at top |
| `IPC_ENGINE_VERIFICATION_SUMMARY_2026-02-06.md` | Added note about team decisions at end |

---

## Key Changes Made

### 1. IPCChannelFactory Location

**Before (Original Plan):**
- Assumed stub in `ipc_contract_v1_1.cpp` needed implementation
- Suggested implementing factory in either file

**After (Team Decision):**
- Real implementation already exists in `unix_socket_channel.cpp`
- Remove/redirect stub in `ipc_contract_v1_1.cpp` only
- **Rationale:** Avoids duplicate symbols; keeps channel logic together

**Code Change:**
```cpp
// In ipc_contract_v1_1.cpp - replace stub with:
std::unique_ptr<IPCChannel> IPCChannelFactory::create(IPCChannelType type) {
    // Implementation lives in unix_socket_channel.cpp
    return IPCChannelFactory::createDefault();
}
```

---

### 2. Class Name Correction

**Before:**
- Used `UnixSocketChannel` in examples

**After:**
- Corrected to `UnixSocketIPCChannel` everywhere
- **Rationale:** Matches actual class name in header/implementation

**Files Updated:**
- All code examples in both planning documents
- References in fix descriptions

---

### 3. Thread Safety Model

**Before:**
- Did not explicitly address thread safety
- Implied channels might be thread-safe

**After:**
- `IPCChannel` is explicitly **NOT thread-safe**
- Added `io_mutex_` to `IPCSession` for all channel I/O
- **Rationale:** Safest assumption; avoids undefined concurrency

**Implementation:**
```cpp
// In IPCSession:
std::mutex io_mutex_;  // Protects channel I/O

// Usage in sessionReadLoop:
{
    std::lock_guard<std::mutex> io_lock(session->getIOMutex());
    auto status = session->getChannel()->receive(msg, &ctx);
}
```

---

### 4. Missing Reader Loop

**Before:**
- Identified that nothing populates `message_queue_`
- Solution not fully specified

**After:**
- Added `sessionReadLoop()` implementation
- Added `enqueueMessage()` method
- Added `IPCSession::getChannel()` accessor
- **Rationale:** Worker threads assume centralized queue; preserves design

**Implementation:**
```cpp
void IPCServer::sessionReadLoop(uint32_t session_id) {
    // ... get session ...
    while (running_ && session->isActive()) {
        IPCMessage msg;
        {
            std::lock_guard<std::mutex> io_lock(session->getIOMutex());
            auto status = session->getChannel()->receive(msg, &ctx);
            if (status != core::Status::OK) break;
        }
        enqueueMessage(session_id, std::move(msg));
    }
    destroySession(session_id);
}
```

---

### 5. STREAM_CONTROL Payload Model

**Before:**
- Presented two options (credits vs windowed)
- Recommended following spec (windowed)

**After:**
- **Decision:** Keep credits model, update spec to match
- **Short-term:** Update spec document to credits model
- **Long-term:** Optional migration to windowed if needed
- **Rationale:** Fastest path to working system; credits already implemented

**Implementation:**
```cpp
struct FlowControlState {
    int32_t credits = 0;          // Credits model (kept)
    uint32_t buffer_avail = 0;
    // ...
};
```

---

### 6. Session Lifecycle Management

**Before:**
- Did not specify session state management
- Risk of races during teardown

**After:**
- Added `SessionState` enum: **ACTIVE → CLOSING → CLOSED**
- Added `shutdown()` method
- `destroySession()` sets CLOSING, stops I/O, erases after reader exits
- **Rationale:** Avoids races between message processing and teardown

**Implementation:**
```cpp
enum class SessionState {
    ACTIVE,
    CLOSING,
    CLOSED
};

void IPCServer::destroySession(uint32_t session_id) {
    // ... get session ...
    session->setState(SessionState::CLOSING);
    session->shutdown();
    // ... erase session ...
}
```

---

### 7. ParserAgent API

**Before:**
- Suggested manual serialization of headers
- Implied low-level buffer management

**After:**
- Use `IPCChannel::send/receive(IPCMessage&)` directly
- **Rationale:** Matches interface; avoids divergence with transport

**Implementation:**
```cpp
core::Status ParserAgent::sendToEngine(uint32_t client_id,
                                      const IPCMessage& msg,
                                      core::ErrorContext* ctx) {
    auto channel = getChannelForClient(client_id);
    if (!channel) return core::Status::NOT_FOUND;
    return channel->send(msg, ctx);  // Direct API usage
}
```

---

## New Section Added: Architectural Decisions Summary

Added Section 8 to the main remediation plan documenting all team decisions:

```markdown
## 8. Architectural Decisions Summary

### 8.1 IPCChannelFactory Location
- Decision: Keep implementation in unix_socket_channel.cpp
- Rationale: Avoids duplicate symbols

### 8.2 Class Naming
- Decision: Use UnixSocketIPCChannel everywhere
- Rationale: Matches current implementation

### 8.3 Thread Safety
- Decision: Treat IPCChannel as not thread-safe
- Implementation: Add io_mutex_ to IPCSession
- Rationale: Safest assumption

... (etc)
```

---

## Checklist Updates

The implementation checklists in both documents were updated to include:
- [ ] Remove IPCChannelFactory stub (not implement)
- [ ] Add session state machine (ACTIVE, CLOSING, CLOSED)
- [ ] Add io_mutex_ for thread-safe I/O
- [ ] Update spec to credits model (not code to windowed)
- [ ] Use IPCChannel::send/receive(IPCMessage&) directly

---

## Timeline Impact

The team decisions **do not change the overall timeline** (~4 weeks) but clarify the implementation approach:

| Phase | Impact |
|-------|--------|
| Phase 1 | Adds io_mutex_ and state machine work (minimal time increase) |
| Phase 2 | Simplified - keep credits model, just add handler |
| Phase 3-7 | No significant impact |

**Net result:** Timeline remains ~4 weeks with 1-2 developers.

---

## Recommended Reading Order

1. `IPC_ENGINE_VERIFICATION_SUMMARY_2026-02-06.md` - Understand what issues exist
2. `IPC_ENGINE_REMEDIATION_PLAN_2026-02-06.md` - Full plan with architectural decisions
3. `IPC_ENGINE_QUICK_FIX_REFERENCE.md` - Quick implementation guide

---

## Questions or Issues?

If any part of the updated plan is unclear:
1. Review Section 8 in the main remediation plan
2. Check the "Team Decisions Summary" in the quick fix reference
3. Compare code examples against existing codebase
