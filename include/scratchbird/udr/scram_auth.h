/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * SCRAM helper for outbound UDR connectors
 *
 * Implements RFC 5802 (SCRAM) and RFC 7677 (SCRAM-SHA-256) support used when
 * ScratchBird acts as a client to remote PostgreSQL/MySQL-compatible servers.
 *
 * This header is not the inbound ScratchBird user-auth contract; that lives in
 * the engine security/plugin layer.
 */

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace scratchbird {
namespace udr {

/**
 * SCRAM mechanism selection
 */
enum class SCRAMMechanism {
    SCRAM_SHA_256,
    SCRAM_SHA_256_PLUS,  // With channel binding
    SCRAM_SHA_512
};

/**
 * SCRAM server-role state machine
 *
 * Use this when a connector test harness, compatibility shim, or simulated
 * remote peer needs the server side of a SCRAM exchange. It is not the
 * authoritative ScratchBird login path.
 */
class SCRAMServer {
public:
    explicit SCRAMServer(SCRAMMechanism mechanism = SCRAMMechanism::SCRAM_SHA_256);
    ~SCRAMServer();
    
    // Non-copyable
    SCRAMServer(const SCRAMServer&) = delete;
    SCRAMServer& operator=(const SCRAMServer&) = delete;

    /**
     * Generate server-first-message after receiving client-first-message
     * 
     * @param client_first_bare The client-first-message-bare (without gs2-header)
     * @param username The username for authentication
     * @param password The password for authentication
     * @return server-first-message
     */
    std::string generateServerFirstMessage(const std::string& client_first_bare,
                                           const std::string& username,
                                           const std::string& password);
    
    /**
     * Verify client-final-message and generate server-final-message
     * 
     * @param client_final The client-final-message
     * @param server_final_out Output for server-final-message
     * @return true if authentication successful
     */
    bool verifyClientFinalMessage(const std::string& client_final,
                                  std::string& server_final_out);
    
    /**
     * Check if authentication is complete and successful
     */
    bool isAuthenticated() const { return state_ == State::AUTHENTICATED; }
    
    /**
     * Get error message if authentication failed
     */
    const std::string& getError() const { return error_; }

private:
    enum class State {
        INITIAL,
        SERVER_FIRST_SENT,
        AUTHENTICATED,
        FAILED
    };
    
    SCRAMMechanism mechanism_;
    State state_;
    std::string error_;
    
    // Authentication data
    std::string username_;
    std::string client_first_bare_;
    std::string server_first_message_;
    std::string client_final_;
    std::string client_nonce_;
    std::string nonce_;
    
    // Cryptographic data
    std::vector<uint8_t> server_nonce_;
    std::vector<uint8_t> salt_;
    uint32_t iterations_;
    std::vector<uint8_t> salted_password_;
    std::vector<uint8_t> stored_key_;
    std::vector<uint8_t> server_key_;
    
    // Helpers
    std::map<char, std::string> parseAttributes(const std::string& message);
    void clearMemory(void* ptr, size_t len);
};

/**
 * SCRAM client-role state machine
 *
 * Use this for outbound connector sessions that must authenticate against
 * remote servers requiring SCRAM.
 */
class SCRAMClient {
public:
    explicit SCRAMClient(SCRAMMechanism mechanism = SCRAMMechanism::SCRAM_SHA_256);
    ~SCRAMClient();
    
    // Non-copyable
    SCRAMClient(const SCRAMClient&) = delete;
    SCRAMClient& operator=(const SCRAMClient&) = delete;

    /**
     * Generate client-first-message
     * 
     * @param username The username for authentication
     * @return client-first-message (gs2-header + client-first-bare)
     */
    std::string generateClientFirstMessage(const std::string& username);
    
    /**
     * Process server-first-message and generate client-final-message
     * 
     * @param server_first The server-first-message
     * @param password The password for authentication
     * @param client_final_out Output for client-final-message
     * @return true if server message is valid
     */
    bool processServerFirstMessage(const std::string& server_first,
                                   const std::string& password,
                                   std::string& client_final_out);
    
    /**
     * Verify server-final-message
     * 
     * @param server_final The server-final-message
     * @return true if server signature is valid
     */
    bool verifyServerFinalMessage(const std::string& server_final);
    
    /**
     * Check if authentication is complete and successful
     */
    bool isAuthenticated() const { return state_ == State::AUTHENTICATED; }
    
    /**
     * Get error message if authentication failed
     */
    const std::string& getError() const { return error_; }

private:
    enum class State {
        INITIAL,
        CLIENT_FIRST_SENT,
        CLIENT_FINAL_SENT,
        AUTHENTICATED,
        FAILED
    };
    
    SCRAMMechanism mechanism_;
    State state_;
    std::string error_;
    
    // Authentication data
    std::string username_;
    std::string client_first_bare_;
    std::string server_first_;
    std::vector<uint8_t> client_nonce_;
    
    // Cryptographic data
    std::vector<uint8_t> salted_password_;
    
    // Helpers
    std::string escapeUsername(const std::string& username);
    std::map<char, std::string> parseAttributes(const std::string& message);
    void clearMemory(void* ptr, size_t len);
};

} // namespace udr
} // namespace scratchbird
