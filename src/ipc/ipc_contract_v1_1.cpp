/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/ipc/ipc_contract_v1_1.h"

#include <cstring>
#include <chrono>

namespace scratchbird {
namespace ipc {

// ============================================================================
// IPCMessage Implementation
// ============================================================================

IPCMessage::IPCMessage() {
    std::memset(&header, 0, sizeof(header));
    header.magic = IPCHeader::MAGIC;
    header.version = IPC_CURRENT_VERSION;
}

IPCMessage::IPCMessage(IPCMessageType type, uint32_t session_id) : IPCMessage() {
    header.type = static_cast<uint16_t>(type);
    header.session_id = session_id;
}

std::vector<uint8_t> IPCMessage::serialize() const {
    std::vector<uint8_t> data(sizeof(header) + payload.size());
    std::memcpy(data.data(), &header, sizeof(header));
    if (!payload.empty()) {
        std::memcpy(data.data() + sizeof(header), payload.data(), payload.size());
    }
    return data;
}

bool IPCMessage::deserialize(const uint8_t* data, size_t len) {
    if (len < sizeof(header)) {
        return false;
    }
    
    std::memcpy(&header, data, sizeof(header));
    
    if (!header.isValid()) {
        return false;
    }
    
    size_t payload_len = len - sizeof(header);
    if (payload_len > 0) {
        payload.assign(data + sizeof(header), data + len);
    } else {
        payload.clear();
    }
    
    return true;
}

bool IPCMessage::isValid() const {
    return header.isValid();
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* ipcMessageTypeToString(IPCMessageType type) {
    switch (type) {
        // Connection Management
        case IPCMessageType::STARTUP: return "STARTUP";
        case IPCMessageType::READY: return "READY";
        case IPCMessageType::FEATURE_NEGOTIATE: return "FEATURE_NEGOTIATE";
        case IPCMessageType::TERMINATE: return "TERMINATE";
        case IPCMessageType::PING: return "PING";
        case IPCMessageType::PONG: return "PONG";
        
        // Session Management
        case IPCMessageType::ATTACH: return "ATTACH";
        case IPCMessageType::DETACH: return "DETACH";
        case IPCMessageType::ATTACHED: return "ATTACHED";
        case IPCMessageType::DETACHED: return "DETACHED";
        
        // Query Execution
        case IPCMessageType::SIMPLE_QUERY: return "SIMPLE_QUERY";
        case IPCMessageType::PARSE: return "PARSE";
        case IPCMessageType::BIND: return "BIND";
        case IPCMessageType::DESCRIBE: return "DESCRIBE";
        case IPCMessageType::EXECUTE: return "EXECUTE";
        case IPCMessageType::CLOSE: return "CLOSE";
        case IPCMessageType::SYNC: return "SYNC";
        case IPCMessageType::COMPILED_QUERY: return "COMPILED_QUERY";
        case IPCMessageType::COMPILED_PARSE: return "COMPILED_PARSE";
        
        // Results
        case IPCMessageType::ROW_DESCRIPTION: return "ROW_DESCRIPTION";
        case IPCMessageType::DATA_ROW: return "DATA_ROW";
        case IPCMessageType::DATA_BATCH: return "DATA_BATCH";
        case IPCMessageType::COMMAND_COMPLETE: return "COMMAND_COMPLETE";
        case IPCMessageType::EMPTY_RESPONSE: return "EMPTY_RESPONSE";
        case IPCMessageType::PARSE_COMPLETE: return "PARSE_COMPLETE";
        case IPCMessageType::BIND_COMPLETE: return "BIND_COMPLETE";
        case IPCMessageType::CLOSE_COMPLETE: return "CLOSE_COMPLETE";
        
        // COPY Operations
        case IPCMessageType::COPY_IN_REQUEST: return "COPY_IN_REQUEST";
        case IPCMessageType::COPY_OUT_RESPONSE: return "COPY_OUT_RESPONSE";
        case IPCMessageType::COPY_DATA: return "COPY_DATA";
        case IPCMessageType::COPY_DONE: return "COPY_DONE";
        case IPCMessageType::COPY_FAIL: return "COPY_FAIL";
        case IPCMessageType::COPY_COMPLETE: return "COPY_COMPLETE";
        case IPCMessageType::STREAM_CONTROL: return "STREAM_CONTROL";
        
        // Transactions
        case IPCMessageType::TXN_BEGIN: return "TXN_BEGIN";
        case IPCMessageType::TXN_COMMIT: return "TXN_COMMIT";
        case IPCMessageType::TXN_ROLLBACK: return "TXN_ROLLBACK";
        case IPCMessageType::SAVEPOINT: return "SAVEPOINT";
        case IPCMessageType::RELEASE: return "RELEASE";
        case IPCMessageType::ROLLBACK_TO: return "ROLLBACK_TO";
        case IPCMessageType::TXN_COMPLETE: return "TXN_COMPLETE";
        
        // Asynchronous
        case IPCMessageType::NOTIFY_SUBSCRIBE: return "NOTIFY_SUBSCRIBE";
        case IPCMessageType::NOTIFY_UNSUBSCRIBE: return "NOTIFY_UNSUBSCRIBE";
        case IPCMessageType::NOTIFY_DELIVER: return "NOTIFY_DELIVER";
        case IPCMessageType::CANCEL_REQUEST: return "CANCEL_REQUEST";
        case IPCMessageType::CANCEL_ACK: return "CANCEL_ACK";
        
        // Errors
        case IPCMessageType::ERROR_RESPONSE: return "ERROR_RESPONSE";
        case IPCMessageType::NOTICE: return "NOTICE";
        
        // Internal
        case IPCMessageType::HEARTBEAT: return "HEARTBEAT";
        case IPCMessageType::SHUTDOWN: return "SHUTDOWN";
        
        default: return "UNKNOWN";
    }
}

const char* ipcFeatureFlagToString(uint32_t flag) {
    switch (flag) {
        case IPC_FEATURE_PREPARED_STATEMENTS: return "PREPARED_STATEMENTS";
        case IPC_FEATURE_COPY_STREAMING: return "COPY_STREAMING";
        case IPC_FEATURE_NOTIFICATIONS: return "NOTIFICATIONS";
        case IPC_FEATURE_CANCEL: return "CANCEL";
        case IPC_FEATURE_BINARY_RESULTS: return "BINARY_RESULTS";
        case IPC_FEATURE_COMPRESSION: return "COMPRESSION";
        case IPC_FEATURE_ENCRYPTION: return "ENCRYPTION";
        case IPC_FEATURE_BATCH_EXECUTION: return "BATCH_EXECUTION";
        default: return "UNKNOWN";
    }
}

bool validateIPCMessage(const IPCMessage& msg, std::string& error) {
    if (!msg.isValid()) {
        error = "Invalid IPC header (magic or version mismatch)";
        return false;
    }
    
    if (msg.payload.size() != msg.header.length) {
        error = "Payload length mismatch";
        return false;
    }
    
    if (msg.getTotalSize() > IPC_MAX_MESSAGE_SIZE) {
        error = "Message exceeds maximum size";
        return false;
    }
    
    // Validate specific payload sizes
    switch (msg.getType()) {
        case IPCMessageType::SIMPLE_QUERY:
            if (msg.payload.size() < sizeof(IPCSimpleQueryPayload)) {
                error = "SIMPLE_QUERY payload too small";
                return false;
            }
            break;
            
        case IPCMessageType::PARSE:
            if (msg.payload.size() < sizeof(IPCParsePayload)) {
                error = "PARSE payload too small";
                return false;
            }
            break;

        case IPCMessageType::COMPILED_QUERY:
            if (msg.payload.size() < sizeof(IPCCompiledQueryPayload)) {
                error = "COMPILED_QUERY payload too small";
                return false;
            }
            break;

        case IPCMessageType::COMPILED_PARSE:
            if (msg.payload.size() < sizeof(IPCCompiledParsePayload)) {
                error = "COMPILED_PARSE payload too small";
                return false;
            }
            break;
            
        case IPCMessageType::BIND:
            if (msg.payload.size() < sizeof(IPCBindPayload)) {
                error = "BIND payload too small";
                return false;
            }
            break;
            
        case IPCMessageType::EXECUTE:
            if (msg.payload.size() < sizeof(IPCExecutePayload)) {
                error = "EXECUTE payload too small";
                return false;
            }
            break;
            
        case IPCMessageType::ROW_DESCRIPTION:
            if (msg.payload.size() < sizeof(IPCRowDescPayload)) {
                error = "ROW_DESCRIPTION payload too small";
                return false;
            }
            break;
            
        default:
            // Other message types have variable payloads
            break;
    }
    
    return true;
}

} // namespace ipc
} // namespace scratchbird
