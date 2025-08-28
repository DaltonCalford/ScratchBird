#pragma once

#include "scratchbird/engine/authentication.h"
#include "scratchbird/engine/network_server.h"
#include "scratchbird/engine/protocol_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    /**
     * Firebird wire protocol operations - from Firebird 6.0 protocol.h
     * These are the actual operation codes used by Firebird
     */
    enum FirebirdProtocolOp : std::uint32_t {
        // Authentication operations
        op_authenticate_user = 88, // User/password authentication
        op_trusted_auth = 90,      // Trusted authentication (protocol < 13)
        op_cont_auth = 92,         // Continue authentication (protocol >= 13)

        // Connection operations
        op_connect = 1,    // Connect to database
        op_attach = 19,    // Attach to database
        op_detach = 21,    // Detach from database
        op_disconnect = 4, // Disconnect from server

        // Query operations
        op_prepare_statement = 61, // Prepare SQL statement
        op_execute = 62,           // Execute prepared statement
        op_fetch = 63,             // Fetch result rows
        op_free_statement = 64,    // Free prepared statement

        // Transaction operations
        op_transaction = 29, // Start transaction
        op_commit = 30,      // Commit transaction
        op_rollback = 31,    // Rollback transaction

        // Response operations
        op_response = 9, // Generic response
        op_dummy = 10,   // Keep-alive/dummy packet

        // Error responses
        op_reject = 3, // Connection rejected
    };

    /**
     * Firebird protocol versions
     */
    enum FirebirdProtocolVersion : std::uint16_t {
        PROTOCOL_VERSION10 = 10,
        PROTOCOL_VERSION11 = 11, // First with authentication support
        PROTOCOL_VERSION12 = 12,
        PROTOCOL_VERSION13 = 13, // Uses op_cont_auth instead of op_trusted_auth
        PROTOCOL_VERSION15 = 15, // Current Firebird 6.0 protocol
    };

    /**
     * Firebird protocol packet structure
     */
    struct FirebirdPacket {
        std::uint32_t operation;
        std::vector<std::uint8_t> data;

        // Convenience methods for common data types
        void write_string(const std::string& str);
        void write_uint32(std::uint32_t value);
        void write_uint16(std::uint16_t value);

        std::string read_string(std::size_t& offset) const;
        std::uint32_t read_uint32(std::size_t& offset) const;
        std::uint16_t read_uint16(std::size_t& offset) const;

        std::vector<std::uint8_t> serialize() const;
        bool deserialize(const std::vector<std::uint8_t>& data);
    };

    /**
     * Firebird protocol message types
     */
    class FirebirdAuthMessage : public ProtocolMessage
    {
      public:
        FirebirdAuthMessage(FirebirdProtocolOp op, const std::vector<std::uint8_t>& auth_data);
        std::unique_ptr<ProtocolMessage> clone() const override;

        FirebirdProtocolOp get_operation() const
        {
            return operation_;
        }
        const std::vector<std::uint8_t>& get_auth_data() const
        {
            return auth_data_;
        }

      private:
        FirebirdProtocolOp operation_;
        std::vector<std::uint8_t> auth_data_;
    };

    class FirebirdResponseMessage : public ProtocolMessage
    {
      public:
        FirebirdResponseMessage(std::uint32_t status_code, const std::string& message = "");
        std::unique_ptr<ProtocolMessage> clone() const override;

        std::uint32_t get_status_code() const
        {
            return status_code_;
        }
        const std::string& get_response_message() const
        {
            return response_message_;
        }

      private:
        std::uint32_t status_code_;
        std::string response_message_;
    };

    /**
     * Firebird wire protocol handler implementing Firebird-compatible authentication
     * Based on Firebird 6.0 source code analysis
     */
    class FirebirdAuthenticationHandler : public ProtocolHandler
    {
      public:
        explicit FirebirdAuthenticationHandler(
            ScratchBird::AuthenticationManager* auth_manager = nullptr);
        ~FirebirdAuthenticationHandler() override;

        // ProtocolHandler interface
        ProtocolType get_protocol_type() const override;
        ProtocolVersion get_supported_version() const override;
        bool supports_version(const ProtocolVersion& version) const override;

        bool initialize(TcpConnection* connection, CatalogManager* catalog) override;
        void shutdown() override;
        bool is_initialized() const override;

        ProtocolResult process_incoming_data(const std::vector<std::uint8_t>& data) override;
        ProtocolResult handle_message(std::unique_ptr<ProtocolMessage> message) override;
        bool has_outgoing_messages() const override;
        std::vector<std::unique_ptr<ProtocolMessage>> get_outgoing_messages() override;

        std::string get_current_state() const override;
        bool is_authenticated() const override;
        bool requires_authentication() const override;

        void handle_protocol_error(const std::string& error_message) override;
        std::string get_last_error() const override;

      private:
        // Protocol state machine states
        enum class FirebirdState {
            Connecting,
            WaitingForAuth,
            Authenticating,
            TrustedAuthInProgress,
            ContinueAuthInProgress,
            Authenticated,
            Connected,
            Error,
            Disconnected
        };

        // Core components
        TcpConnection* connection_;
        CatalogManager* catalog_;
        ScratchBird::AuthenticationManager* auth_manager_;

        // Protocol state
        FirebirdState current_state_;
        std::uint16_t protocol_version_;
        bool initialized_;
        std::string last_error_;

        // Authentication state
        std::unique_ptr<ScratchBird::AuthenticationContext> auth_context_;
        std::unique_ptr<ScratchBird::AuthenticationChallenge> active_challenge_;
        std::string client_username_;
        std::string client_database_;

        // Message queues
        std::queue<std::unique_ptr<ProtocolMessage>> outgoing_messages_;
        std::vector<std::uint8_t> incoming_buffer_;

        // Firebird protocol handlers
        ProtocolResult handle_firebird_packet(const FirebirdPacket& packet);
        ProtocolResult handle_connect_request(const FirebirdPacket& packet);
        ProtocolResult handle_authenticate_user(const FirebirdPacket& packet);
        ProtocolResult handle_trusted_auth(const FirebirdPacket& packet);
        ProtocolResult handle_cont_auth(const FirebirdPacket& packet);
        ProtocolResult handle_attach_database(const FirebirdPacket& packet);

        // Authentication methods
        bool start_password_authentication(const std::string& username,
                                           const std::string& password);
        bool start_trusted_authentication();
        bool continue_authentication_challenge(const std::vector<std::uint8_t>& challenge_data);

        // Protocol utilities
        void send_response(std::uint32_t status_code, const std::string& message = "");
        void send_authentication_challenge(const std::vector<std::uint8_t>& challenge_data);
        void send_authentication_success();
        void send_error(std::uint32_t error_code, const std::string& error_message);

        bool parse_firebird_packet(const std::vector<std::uint8_t>& data, FirebirdPacket& packet);
        std::vector<std::uint8_t> serialize_packet(const FirebirdPacket& packet) const;

        // State management
        void transition_to_state(FirebirdState new_state);
        std::string state_to_string(FirebirdState state) const;
        bool is_valid_state_transition(FirebirdState from, FirebirdState to) const;

        // Protocol version handling
        bool is_legacy_protocol() const
        {
            return protocol_version_ < PROTOCOL_VERSION13;
        }
        bool supports_trusted_auth() const
        {
            return protocol_version_ >= PROTOCOL_VERSION11;
        }
        bool supports_continue_auth() const
        {
            return protocol_version_ >= PROTOCOL_VERSION13;
        }

        mutable std::mutex handler_mutex_;
    };

    /**
     * Firebird message framer for handling Firebird wire protocol packets
     */
    class FirebirdAuthMessageFramer : public MessageFramer
    {
      public:
        FirebirdAuthMessageFramer();
        ~FirebirdAuthMessageFramer() override = default;

        std::vector<std::vector<std::uint8_t>>
        frame_messages(const std::vector<std::uint8_t>& data) override;
        bool needs_more_data() const override;
        void reset() override;

      private:
        std::uint32_t expected_packet_length_;
        bool has_header_;

        static constexpr std::size_t FIREBIRD_PACKET_HEADER_SIZE = 4; // 4-byte length header
    };

} // namespace scratchbird::engine
