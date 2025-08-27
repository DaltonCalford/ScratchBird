#pragma once

#include "scratchbird/engine/protocol_handler.h"

namespace scratchbird::engine
{

    /// ScratchBird native protocol message types
    namespace ScratchBirdMessages
    {
        constexpr const char* CONNECT = "CONNECT";
        constexpr const char* AUTHENTICATE = "AUTH";
        constexpr const char* QUERY = "QUERY";
        constexpr const char* RESPONSE = "RESPONSE";
        constexpr const char* ERROR = "ERROR";
        constexpr const char* DISCONNECT = "DISCONNECT";
        constexpr const char* HEARTBEAT = "HEARTBEAT";
    } // namespace ScratchBirdMessages

    /// ScratchBird protocol message implementation
    class ScratchBirdMessage : public ProtocolMessage
    {
      public:
        ScratchBirdMessage() = default;
        explicit ScratchBirdMessage(const std::string& msg_type);

        std::unique_ptr<ProtocolMessage> clone() const override;

        // Message-specific data
        std::string get_message_type_string() const
        {
            return message_type;
        }
        void set_payload_string(const std::string& str);
        std::string get_payload_string() const;
    };

    /// ScratchBird protocol state machine
    class ScratchBirdStateMachine : public ProtocolStateMachine
    {
      public:
        ScratchBirdStateMachine();

        std::string get_current_state() const override;
        bool transition_to_state(const std::string& new_state) override;
        bool is_valid_transition(const std::string& from_state,
                                 const std::string& to_state) const override;
        std::vector<std::string> get_valid_next_states() const override;

      private:
        void initialize_state_transitions();
        std::unordered_map<std::string, std::vector<std::string>> valid_transitions_;
    };

    /// ScratchBird protocol message framer
    class ScratchBirdFramer : public MessageFramer
    {
      public:
        ScratchBirdFramer();

        std::vector<std::vector<std::uint8_t>>
        frame_messages(const std::vector<std::uint8_t>& data) override;
        bool needs_more_data() const override;
        void reset() override;

      private:
        enum class FrameState { ReadingHeader, ReadingPayload };

        FrameState frame_state_;
        std::uint32_t expected_payload_size_;
        std::uint32_t bytes_read_;

        bool parse_header(const std::vector<std::uint8_t>& data, std::size_t& offset);
        std::vector<std::uint8_t> extract_message();
    };

    /// ScratchBird native protocol handler
    class ScratchBirdProtocolHandler : public ProtocolHandler
    {
      public:
        ScratchBirdProtocolHandler();
        ~ScratchBirdProtocolHandler() override;

        /// Protocol identification
        ProtocolType get_protocol_type() const override;
        ProtocolVersion get_supported_version() const override;
        bool supports_version(const ProtocolVersion& version) const override;

        /// Connection lifecycle
        bool initialize(TcpConnection* connection, CatalogManager* catalog) override;
        void shutdown() override;
        bool is_initialized() const override;

        /// Message processing
        ProtocolResult process_incoming_data(const std::vector<std::uint8_t>& data) override;
        ProtocolResult handle_message(std::unique_ptr<ProtocolMessage> message) override;
        bool has_outgoing_messages() const override;
        std::vector<std::unique_ptr<ProtocolMessage>> get_outgoing_messages() override;

        /// Protocol state management
        std::string get_current_state() const override;
        bool is_authenticated() const override;
        bool requires_authentication() const override;

        /// Error handling
        void handle_protocol_error(const std::string& error_message) override;
        std::string get_last_error() const override;

      private:
        TcpConnection* connection_;
        CatalogManager* catalog_;

        std::unique_ptr<ScratchBirdStateMachine> state_machine_;
        std::unique_ptr<ScratchBirdFramer> message_framer_;

        MessageQueue outgoing_queue_;
        CorrelationTracker correlation_tracker_;

        std::atomic<bool> initialized_;
        std::atomic<bool> authenticated_;
        std::string last_error_;
        mutable std::mutex handler_mutex_;

        // Message handlers
        ProtocolResult handle_connect_message(const ScratchBirdMessage& message);
        ProtocolResult handle_auth_message(const ScratchBirdMessage& message);
        ProtocolResult handle_query_message(const ScratchBirdMessage& message);
        ProtocolResult handle_heartbeat_message(const ScratchBirdMessage& message);

        // Helper methods
        void send_response(const std::string& response_data, std::uint64_t correlation_id = 0);
        void send_error(const std::string& error_message, std::uint64_t correlation_id = 0);
        std::uint64_t generate_message_id();

        std::atomic<std::uint64_t> next_message_id_;
    };

} // namespace scratchbird::engine
