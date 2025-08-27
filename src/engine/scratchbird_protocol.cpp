#include "scratchbird/engine/scratchbird_protocol.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/network_server.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace scratchbird::engine
{

    //=============================================================================
    // ScratchBirdMessage Implementation
    //=============================================================================

    ScratchBirdMessage::ScratchBirdMessage(const std::string& msg_type)
    {
        message_type = msg_type;
    }

    std::unique_ptr<ProtocolMessage> ScratchBirdMessage::clone() const
    {
        auto clone = std::make_unique<ScratchBirdMessage>();
        clone->message_id = this->message_id;
        clone->correlation_id = this->correlation_id;
        clone->priority = this->priority;
        clone->payload = this->payload;
        clone->timestamp_ms = this->timestamp_ms;
        clone->message_type = this->message_type;
        return clone;
    }

    void ScratchBirdMessage::set_payload_string(const std::string& str)
    {
        payload.clear();
        payload.reserve(str.size());
        for (char c : str) {
            payload.push_back(static_cast<std::uint8_t>(c));
        }
    }

    std::string ScratchBirdMessage::get_payload_string() const
    {
        std::string result;
        result.reserve(payload.size());
        for (std::uint8_t byte : payload) {
            result.push_back(static_cast<char>(byte));
        }
        return result;
    }

    //=============================================================================
    // ScratchBirdStateMachine Implementation
    //=============================================================================

    ScratchBirdStateMachine::ScratchBirdStateMachine()
    {
        current_state_ = "Disconnected";
        initialize_state_transitions();
    }

    std::string ScratchBirdStateMachine::get_current_state() const
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return current_state_;
    }

    bool ScratchBirdStateMachine::transition_to_state(const std::string& new_state)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (!is_valid_transition(current_state_, new_state)) {
            on_invalid_transition(current_state_, new_state);
            return false;
        }

        std::string old_state = current_state_;
        on_state_exit(old_state);
        current_state_ = new_state;
        on_state_enter(new_state);

        return true;
    }

    bool ScratchBirdStateMachine::is_valid_transition(const std::string& from_state,
                                                      const std::string& to_state) const
    {
        auto it = valid_transitions_.find(from_state);
        if (it == valid_transitions_.end()) {
            return false;
        }

        const auto& valid_states = it->second;
        return std::find(valid_states.begin(), valid_states.end(), to_state) != valid_states.end();
    }

    std::vector<std::string> ScratchBirdStateMachine::get_valid_next_states() const
    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        auto it = valid_transitions_.find(current_state_);
        if (it != valid_transitions_.end()) {
            return it->second;
        }

        return {};
    }

    void ScratchBirdStateMachine::initialize_state_transitions()
    {
        // Define valid state transitions for ScratchBird protocol
        valid_transitions_["Disconnected"] = {"Connected"};
        valid_transitions_["Connected"] = {"Authenticating", "Disconnected"};
        valid_transitions_["Authenticating"] = {"Authenticated", "Disconnected"};
        valid_transitions_["Authenticated"] = {"Processing", "Disconnected"};
        valid_transitions_["Processing"] = {"Authenticated", "Disconnected"};
    }

    //=============================================================================
    // ScratchBirdFramer Implementation
    //=============================================================================

    ScratchBirdFramer::ScratchBirdFramer()
        : frame_state_(FrameState::ReadingHeader), expected_payload_size_(0), bytes_read_(0)
    {
    }

    std::vector<std::vector<std::uint8_t>>
    ScratchBirdFramer::frame_messages(const std::vector<std::uint8_t>& data)
    {
        std::vector<std::vector<std::uint8_t>> messages;

        // Add new data to buffer
        buffer_.insert(buffer_.end(), data.begin(), data.end());

        std::size_t offset = 0;
        while (offset < buffer_.size()) {
            if (frame_state_ == FrameState::ReadingHeader) {
                if (!parse_header(buffer_, offset)) {
                    needs_more_data_ = true;
                    break; // Need more data for complete header
                }
                frame_state_ = FrameState::ReadingPayload;
            }

            if (frame_state_ == FrameState::ReadingPayload) {
                std::size_t available_bytes = buffer_.size() - offset;
                std::size_t needed_bytes = expected_payload_size_ - bytes_read_;

                if (available_bytes >= needed_bytes) {
                    // We have enough data for complete message
                    std::size_t payload_start = offset;
                    std::size_t payload_end = offset + needed_bytes;

                    // Extract complete message (header + payload)
                    std::vector<std::uint8_t> complete_message;
                    complete_message.insert(
                        complete_message.end(),
                        buffer_.begin() + (payload_start - (bytes_read_ + sizeof(std::uint32_t))),
                        buffer_.begin() + payload_end);
                    messages.push_back(complete_message);

                    // Reset for next message
                    offset = payload_end;
                    frame_state_ = FrameState::ReadingHeader;
                    expected_payload_size_ = 0;
                    bytes_read_ = 0;
                } else {
                    // Need more data
                    bytes_read_ += available_bytes;
                    needs_more_data_ = true;
                    break;
                }
            }
        }

        // Remove processed data from buffer
        if (offset > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + offset);
        }

        return messages;
    }

    bool ScratchBirdFramer::needs_more_data() const
    {
        return needs_more_data_.load();
    }

    void ScratchBirdFramer::reset()
    {
        buffer_.clear();
        frame_state_ = FrameState::ReadingHeader;
        expected_payload_size_ = 0;
        bytes_read_ = 0;
        needs_more_data_ = false;
    }

    bool ScratchBirdFramer::parse_header(const std::vector<std::uint8_t>& data, std::size_t& offset)
    {
        // ScratchBird protocol header: 4 bytes for payload size (little-endian)
        const std::size_t header_size = sizeof(std::uint32_t);

        if (data.size() - offset < header_size) {
            return false; // Not enough data for header
        }

        // Extract payload size (little-endian)
        std::uint32_t payload_size = 0;
        std::memcpy(&payload_size, &data[offset], sizeof(std::uint32_t));

        expected_payload_size_ = payload_size;
        offset += header_size;

        return true;
    }

    std::vector<std::uint8_t> ScratchBirdFramer::extract_message()
    {
        // This method extracts a complete message from the buffer
        // Implementation depends on how messages are structured
        std::vector<std::uint8_t> message;

        if (buffer_.size() >= (sizeof(std::uint32_t) + expected_payload_size_)) {
            std::size_t total_message_size = sizeof(std::uint32_t) + expected_payload_size_;
            message.assign(buffer_.begin(), buffer_.begin() + total_message_size);
            buffer_.erase(buffer_.begin(), buffer_.begin() + total_message_size);
        }

        return message;
    }

    //=============================================================================
    // ScratchBirdProtocolHandler Implementation
    //=============================================================================

    ScratchBirdProtocolHandler::ScratchBirdProtocolHandler()
        : connection_(nullptr), catalog_(nullptr), initialized_(false), authenticated_(false),
          next_message_id_(1)
    {
        state_machine_ = std::make_unique<ScratchBirdStateMachine>();
        message_framer_ = std::make_unique<ScratchBirdFramer>();
    }

    ScratchBirdProtocolHandler::~ScratchBirdProtocolHandler()
    {
        shutdown();
    }

    ProtocolType ScratchBirdProtocolHandler::get_protocol_type() const
    {
        return ProtocolType::ScratchBirdNative;
    }

    ProtocolVersion ScratchBirdProtocolHandler::get_supported_version() const
    {
        ProtocolVersion version;
        version.major = 1;
        version.minor = 0;
        version.build = 0;
        version.type = ProtocolType::ScratchBirdNative;
        return version;
    }

    bool ScratchBirdProtocolHandler::supports_version(const ProtocolVersion& version) const
    {
        ProtocolVersion supported = get_supported_version();
        return supported.is_compatible_with(version);
    }

    bool ScratchBirdProtocolHandler::initialize(TcpConnection* connection, CatalogManager* catalog)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (initialized_.load()) {
            return true;
        }

        if (!connection || !catalog) {
            last_error_ = "Invalid connection or catalog";
            return false;
        }

        connection_ = connection;
        catalog_ = catalog;

        // Initialize state machine
        if (!state_machine_->transition_to_state("Connected")) {
            last_error_ = "Failed to transition to connected state";
            return false;
        }

        initialized_ = true;
        return true;
    }

    void ScratchBirdProtocolHandler::shutdown()
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (!initialized_.load()) {
            return;
        }

        // Transition to disconnected state
        if (state_machine_) {
            state_machine_->transition_to_state("Disconnected");
        }

        // Clear queues and reset state
        outgoing_queue_.clear();

        connection_ = nullptr;
        catalog_ = nullptr;
        authenticated_ = false;
        initialized_ = false;
    }

    bool ScratchBirdProtocolHandler::is_initialized() const
    {
        return initialized_.load();
    }

    ProtocolResult
    ScratchBirdProtocolHandler::process_incoming_data(const std::vector<std::uint8_t>& data)
    {
        if (!initialized_.load() || !message_framer_) {
            return ProtocolResult::ProtocolError;
        }

        try {
            auto messages = message_framer_->frame_messages(data);

            for (const auto& message_data : messages) {
                // Parse message data into ScratchBirdMessage
                auto message = std::make_unique<ScratchBirdMessage>();

                if (message_data.size() >= sizeof(std::uint32_t)) {
                    // Skip the size header and use the payload
                    std::size_t payload_start = sizeof(std::uint32_t);
                    message->payload.assign(message_data.begin() + payload_start,
                                            message_data.end());

                    // For simplicity, assume message type is embedded in payload
                    std::string payload_str = message->get_payload_string();
                    if (payload_str.find("CONNECT") == 0) {
                        message->message_type = ScratchBirdMessages::CONNECT;
                    } else if (payload_str.find("AUTH") == 0) {
                        message->message_type = ScratchBirdMessages::AUTHENTICATE;
                    } else if (payload_str.find("QUERY") == 0) {
                        message->message_type = ScratchBirdMessages::QUERY;
                    } else if (payload_str.find("HEARTBEAT") == 0) {
                        message->message_type = ScratchBirdMessages::HEARTBEAT;
                    } else {
                        message->message_type = "UNKNOWN";
                    }

                    message->message_id = generate_message_id();

                    // Process message immediately
                    ProtocolResult result = handle_message(std::move(message));
                    if (result != ProtocolResult::Success &&
                        result != ProtocolResult::ContinueProcessing) {
                        return result;
                    }
                }
            }

            return message_framer_->needs_more_data() ? ProtocolResult::NeedMoreData
                                                      : ProtocolResult::Success;

        } catch (const std::exception& e) {
            handle_protocol_error("Error processing incoming data: " + std::string(e.what()));
            return ProtocolResult::ProtocolError;
        }
    }

    ProtocolResult
    ScratchBirdProtocolHandler::handle_message(std::unique_ptr<ProtocolMessage> message)
    {
        if (!message) {
            return ProtocolResult::ProtocolError;
        }

        auto sb_message = dynamic_cast<ScratchBirdMessage*>(message.get());
        if (!sb_message) {
            handle_protocol_error("Invalid message type for ScratchBird protocol");
            return ProtocolResult::ProtocolError;
        }

        try {
            // Route message based on type
            if (sb_message->message_type == ScratchBirdMessages::CONNECT) {
                return handle_connect_message(*sb_message);
            } else if (sb_message->message_type == ScratchBirdMessages::AUTHENTICATE) {
                return handle_auth_message(*sb_message);
            } else if (sb_message->message_type == ScratchBirdMessages::QUERY) {
                return handle_query_message(*sb_message);
            } else if (sb_message->message_type == ScratchBirdMessages::HEARTBEAT) {
                return handle_heartbeat_message(*sb_message);
            } else {
                handle_protocol_error("Unknown message type: " + sb_message->message_type);
                return ProtocolResult::ProtocolError;
            }

        } catch (const std::exception& e) {
            handle_protocol_error("Error handling message: " + std::string(e.what()));
            return ProtocolResult::ProtocolError;
        }
    }

    bool ScratchBirdProtocolHandler::has_outgoing_messages() const
    {
        return !outgoing_queue_.empty();
    }

    std::vector<std::unique_ptr<ProtocolMessage>>
    ScratchBirdProtocolHandler::get_outgoing_messages()
    {
        std::vector<std::unique_ptr<ProtocolMessage>> messages;

        while (!outgoing_queue_.empty()) {
            auto message = outgoing_queue_.dequeue();
            if (message) {
                messages.push_back(std::move(message));
            }
        }

        return messages;
    }

    std::string ScratchBirdProtocolHandler::get_current_state() const
    {
        if (state_machine_) {
            return state_machine_->get_current_state();
        }
        return "Unknown";
    }

    bool ScratchBirdProtocolHandler::is_authenticated() const
    {
        return authenticated_.load();
    }

    bool ScratchBirdProtocolHandler::requires_authentication() const
    {
        return true; // ScratchBird protocol requires authentication
    }

    void ScratchBirdProtocolHandler::handle_protocol_error(const std::string& error_message)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        last_error_ = error_message;
        std::cerr << "ScratchBird Protocol Error: " << error_message << std::endl;
    }

    std::string ScratchBirdProtocolHandler::get_last_error() const
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        return last_error_;
    }

    ProtocolResult
    ScratchBirdProtocolHandler::handle_connect_message(const ScratchBirdMessage& message)
    {
        if (!state_machine_->transition_to_state("Authenticating")) {
            send_error("Invalid state for connection");
            return ProtocolResult::ProtocolError;
        }

        // Send connection acknowledgment
        send_response("CONNECTION_OK", message.correlation_id);
        return ProtocolResult::Success;
    }

    ProtocolResult
    ScratchBirdProtocolHandler::handle_auth_message(const ScratchBirdMessage& message)
    {
        if (state_machine_->get_current_state() != "Authenticating") {
            send_error("Authentication not expected in current state");
            return ProtocolResult::ProtocolError;
        }

        // Simple authentication for testing - accept any credentials
        authenticated_ = true;

        if (!state_machine_->transition_to_state("Authenticated")) {
            send_error("Failed to complete authentication");
            return ProtocolResult::ProtocolError;
        }

        send_response("AUTH_OK", message.correlation_id);
        return ProtocolResult::Success;
    }

    ProtocolResult
    ScratchBirdProtocolHandler::handle_query_message(const ScratchBirdMessage& message)
    {
        if (!authenticated_.load()) {
            send_error("Authentication required");
            return ProtocolResult::AuthenticationRequired;
        }

        if (!state_machine_->transition_to_state("Processing")) {
            send_error("Invalid state for query processing");
            return ProtocolResult::ProtocolError;
        }

        // Process query (simplified for testing)
        std::string query = message.get_payload_string();
        std::string response = "QUERY_RESULT: Processed query '" + query + "'";

        send_response(response, message.correlation_id);

        // Return to authenticated state
        state_machine_->transition_to_state("Authenticated");

        return ProtocolResult::Success;
    }

    ProtocolResult
    ScratchBirdProtocolHandler::handle_heartbeat_message(const ScratchBirdMessage& message)
    {
        send_response("HEARTBEAT_OK", message.correlation_id);
        return ProtocolResult::Success;
    }

    void ScratchBirdProtocolHandler::send_response(const std::string& response_data,
                                                   std::uint64_t correlation_id)
    {
        auto response = std::make_unique<ScratchBirdMessage>(ScratchBirdMessages::RESPONSE);
        response->message_id = generate_message_id();
        response->correlation_id = correlation_id;
        response->set_payload_string(response_data);

        outgoing_queue_.enqueue(std::move(response));
    }

    void ScratchBirdProtocolHandler::send_error(const std::string& error_message,
                                                std::uint64_t correlation_id)
    {
        auto error_msg = std::make_unique<ScratchBirdMessage>(ScratchBirdMessages::ERROR);
        error_msg->message_id = generate_message_id();
        error_msg->correlation_id = correlation_id;
        error_msg->set_payload_string(error_message);

        outgoing_queue_.enqueue(std::move(error_msg));
    }

    std::uint64_t ScratchBirdProtocolHandler::generate_message_id()
    {
        return next_message_id_++;
    }

} // namespace scratchbird::engine
