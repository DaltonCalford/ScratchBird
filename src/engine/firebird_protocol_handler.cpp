#include "scratchbird/engine/firebird_protocol_handler.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/network_server.h"

#include <chrono>
#include <cstring>
#include <iostream>

namespace scratchbird::engine
{

    // ============================================================================
    // FirebirdPacket Implementation
    // ============================================================================

    void FirebirdPacket::write_string(const std::string& str)
    {
        write_uint32(static_cast<std::uint32_t>(str.length()));
        data.insert(data.end(), str.begin(), str.end());
        // Pad to 4-byte boundary
        while (data.size() % 4 != 0) {
            data.push_back(0);
        }
    }

    void FirebirdPacket::write_uint32(std::uint32_t value)
    {
        data.push_back(static_cast<std::uint8_t>(value & 0xFF));
        data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        data.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        data.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    }

    void FirebirdPacket::write_uint16(std::uint16_t value)
    {
        data.push_back(static_cast<std::uint8_t>(value & 0xFF));
        data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    std::string FirebirdPacket::read_string(std::size_t& offset) const
    {
        if (offset + 4 > data.size())
            return "";

        std::uint32_t length = read_uint32(offset);
        if (offset + length > data.size())
            return "";

        std::string result(reinterpret_cast<const char*>(&data[offset]), length);
        offset += length;

        // Skip padding
        while (offset % 4 != 0 && offset < data.size()) {
            offset++;
        }

        return result;
    }

    std::uint32_t FirebirdPacket::read_uint32(std::size_t& offset) const
    {
        if (offset + 4 > data.size())
            return 0;

        std::uint32_t result = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) |
                               (data[offset + 3] << 24);
        offset += 4;
        return result;
    }

    std::uint16_t FirebirdPacket::read_uint16(std::size_t& offset) const
    {
        if (offset + 2 > data.size())
            return 0;

        std::uint16_t result = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        return result;
    }

    std::vector<std::uint8_t> FirebirdPacket::serialize() const
    {
        std::vector<std::uint8_t> result;

        // Write packet length (operation + data)
        std::uint32_t total_length = 4 + static_cast<std::uint32_t>(data.size());
        result.push_back(static_cast<std::uint8_t>(total_length & 0xFF));
        result.push_back(static_cast<std::uint8_t>((total_length >> 8) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((total_length >> 16) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((total_length >> 24) & 0xFF));

        // Write operation
        result.push_back(static_cast<std::uint8_t>(operation & 0xFF));
        result.push_back(static_cast<std::uint8_t>((operation >> 8) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((operation >> 16) & 0xFF));
        result.push_back(static_cast<std::uint8_t>((operation >> 24) & 0xFF));

        // Write data
        result.insert(result.end(), data.begin(), data.end());

        return result;
    }

    bool FirebirdPacket::deserialize(const std::vector<std::uint8_t>& packet_data)
    {
        if (packet_data.size() < 8)
            return false; // Minimum: 4 bytes length + 4 bytes operation

        std::size_t offset = 0;

        // Read packet length
        std::uint32_t packet_length = packet_data[offset] | (packet_data[offset + 1] << 8) |
                                      (packet_data[offset + 2] << 16) |
                                      (packet_data[offset + 3] << 24);
        offset += 4;

        if (packet_length != packet_data.size() - 4)
            return false; // Length mismatch

        // Read operation
        operation = packet_data[offset] | (packet_data[offset + 1] << 8) |
                    (packet_data[offset + 2] << 16) | (packet_data[offset + 3] << 24);
        offset += 4;

        // Read data
        data.assign(packet_data.begin() + offset, packet_data.end());

        return true;
    }

    // ============================================================================
    // FirebirdAuthMessage Implementation
    // ============================================================================

    FirebirdAuthMessage::FirebirdAuthMessage(FirebirdProtocolOp op,
                                             const std::vector<std::uint8_t>& auth_data)
        : operation_(op), auth_data_(auth_data)
    {
        message_type = "firebird_auth";
        payload = auth_data_;
        timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    }

    std::unique_ptr<ProtocolMessage> FirebirdAuthMessage::clone() const
    {
        return std::make_unique<FirebirdAuthMessage>(operation_, auth_data_);
    }

    // ============================================================================
    // FirebirdResponseMessage Implementation
    // ============================================================================

    FirebirdResponseMessage::FirebirdResponseMessage(std::uint32_t status_code,
                                                     const std::string& message)
        : status_code_(status_code), response_message_(message)
    {
        message_type = "firebird_response";
        timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

        // Serialize status and message into payload
        FirebirdPacket packet;
        packet.operation = op_response;
        packet.write_uint32(status_code);
        packet.write_string(message);
        payload = packet.data;
    }

    std::unique_ptr<ProtocolMessage> FirebirdResponseMessage::clone() const
    {
        return std::make_unique<FirebirdResponseMessage>(status_code_, response_message_);
    }

    // ============================================================================
    // FirebirdAuthenticationHandler Implementation
    // ============================================================================

    FirebirdAuthenticationHandler::FirebirdAuthenticationHandler(
        ScratchBird::AuthenticationManager* auth_manager)
        : connection_(nullptr), catalog_(nullptr), auth_manager_(auth_manager),
          current_state_(FirebirdState::Connecting), protocol_version_(PROTOCOL_VERSION15),
          initialized_(false)
    {
        if (auth_manager_) {
            auth_context_ = std::make_unique<ScratchBird::AuthenticationContext>();
        }
    }

    FirebirdAuthenticationHandler::~FirebirdAuthenticationHandler()
    {
        shutdown();
    }

    ProtocolType FirebirdAuthenticationHandler::get_protocol_type() const
    {
        return ProtocolType::FirebirdWire;
    }

    ProtocolVersion FirebirdAuthenticationHandler::get_supported_version() const
    {
        ProtocolVersion version;
        version.major = PROTOCOL_VERSION15;
        version.minor = 0;
        version.build = 0;
        version.type = ProtocolType::FirebirdWire;
        return version;
    }

    bool FirebirdAuthenticationHandler::supports_version(const ProtocolVersion& version) const
    {
        if (version.type != ProtocolType::FirebirdWire)
            return false;

        // Support protocol versions 10-15 (Firebird compatibility range)
        return version.major >= PROTOCOL_VERSION10 && version.major <= PROTOCOL_VERSION15;
    }

    bool FirebirdAuthenticationHandler::initialize(TcpConnection* connection,
                                                   CatalogManager* catalog)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (initialized_)
            return true;

        connection_ = connection;
        catalog_ = catalog;

        if (!connection_) {
            last_error_ = "Invalid connection provided";
            return false;
        }

        transition_to_state(FirebirdState::WaitingForAuth);
        initialized_ = true;

        return true;
    }

    void FirebirdAuthenticationHandler::shutdown()
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (!initialized_)
            return;

        // Clean up authentication state
        auth_context_.reset();
        active_challenge_.reset();

        // Clear message queues
        while (!outgoing_messages_.empty()) {
            outgoing_messages_.pop();
        }
        incoming_buffer_.clear();

        transition_to_state(FirebirdState::Disconnected);
        initialized_ = false;
    }

    bool FirebirdAuthenticationHandler::is_initialized() const
    {
        return initialized_;
    }

    ProtocolResult
    FirebirdAuthenticationHandler::process_incoming_data(const std::vector<std::uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (!initialized_) {
            return ProtocolResult::InternalError;
        }

        // Add to incoming buffer
        incoming_buffer_.insert(incoming_buffer_.end(), data.begin(), data.end());

        // Try to parse complete packets
        while (incoming_buffer_.size() >= 8) { // Minimum packet size
            // Read packet length
            std::uint32_t packet_length = incoming_buffer_[0] | (incoming_buffer_[1] << 8) |
                                          (incoming_buffer_[2] << 16) | (incoming_buffer_[3] << 24);

            // Check if we have complete packet
            if (incoming_buffer_.size() < packet_length + 4) {
                return ProtocolResult::NeedMoreData;
            }

            // Extract packet data
            std::vector<std::uint8_t> packet_data(incoming_buffer_.begin(),
                                                  incoming_buffer_.begin() + packet_length + 4);

            // Remove processed data from buffer
            incoming_buffer_.erase(incoming_buffer_.begin(),
                                   incoming_buffer_.begin() + packet_length + 4);

            // Parse and handle packet
            FirebirdPacket packet;
            if (packet.deserialize(packet_data)) {
                ProtocolResult result = handle_firebird_packet(packet);
                if (result != ProtocolResult::Success &&
                    result != ProtocolResult::ContinueProcessing) {
                    return result;
                }
            } else {
                handle_protocol_error("Failed to parse Firebird packet");
                return ProtocolResult::ProtocolError;
            }
        }

        return ProtocolResult::Success;
    }

    ProtocolResult
    FirebirdAuthenticationHandler::handle_firebird_packet(const FirebirdPacket& packet)
    {
        switch (static_cast<FirebirdProtocolOp>(packet.operation)) {
        case op_connect:
            return handle_connect_request(packet);

        case op_authenticate_user:
            return handle_authenticate_user(packet);

        case op_trusted_auth:
            if (!is_legacy_protocol()) {
                send_error(1, "op_trusted_auth not supported in protocol >= 13");
                return ProtocolResult::ProtocolError;
            }
            return handle_trusted_auth(packet);

        case op_cont_auth:
            if (!supports_continue_auth()) {
                send_error(1, "op_cont_auth not supported in protocol < 13");
                return ProtocolResult::ProtocolError;
            }
            return handle_cont_auth(packet);

        case op_attach:
            return handle_attach_database(packet);

        case op_detach:
            transition_to_state(FirebirdState::WaitingForAuth);
            send_response(0, "Database detached");
            return ProtocolResult::Success;

        case op_disconnect:
            transition_to_state(FirebirdState::Disconnected);
            return ProtocolResult::ConnectionClosed;

        default:
            handle_protocol_error("Unsupported Firebird operation: " +
                                  std::to_string(packet.operation));
            return ProtocolResult::ProtocolError;
        }
    }

    ProtocolResult
    FirebirdAuthenticationHandler::handle_connect_request(const FirebirdPacket& packet)
    {
        std::size_t offset = 0;

        // Read protocol version
        if (packet.data.size() >= 4) {
            std::uint32_t requested_version = packet.read_uint32(offset);

            // Validate protocol version
            if (requested_version >= PROTOCOL_VERSION10 &&
                requested_version <= PROTOCOL_VERSION15) {
                protocol_version_ = static_cast<std::uint16_t>(requested_version);
            } else {
                send_error(1, "Unsupported protocol version");
                return ProtocolResult::ProtocolError;
            }
        }

        // Send connection acceptance
        send_response(0, "Connection established");
        transition_to_state(FirebirdState::WaitingForAuth);

        return ProtocolResult::Success;
    }

    ProtocolResult
    FirebirdAuthenticationHandler::handle_authenticate_user(const FirebirdPacket& packet)
    {
        if (!auth_manager_) {
            send_error(1, "Authentication not configured");
            return ProtocolResult::AuthenticationRequired;
        }

        std::size_t offset = 0;
        client_username_ = packet.read_string(offset);
        std::string password = packet.read_string(offset);

        if (client_username_.empty()) {
            send_error(1, "Username required");
            return ProtocolResult::AuthenticationRequired;
        }

        return start_password_authentication(client_username_, password)
                   ? ProtocolResult::Success
                   : ProtocolResult::AuthenticationRequired;
    }

    ProtocolResult FirebirdAuthenticationHandler::handle_trusted_auth(const FirebirdPacket& packet)
    {
        if (!supports_trusted_auth()) {
            send_error(1, "Trusted authentication not supported");
            return ProtocolResult::ProtocolError;
        }

        transition_to_state(FirebirdState::TrustedAuthInProgress);

        // For now, implement basic trusted authentication
        // In full implementation, this would use SSPI on Windows or PAM on Linux
        if (start_trusted_authentication()) {
            return ProtocolResult::Success;
        } else {
            return ProtocolResult::AuthenticationRequired;
        }
    }

    ProtocolResult FirebirdAuthenticationHandler::handle_cont_auth(const FirebirdPacket& packet)
    {
        if (current_state_ != FirebirdState::ContinueAuthInProgress) {
            send_error(1, "Unexpected continue authentication");
            return ProtocolResult::ProtocolError;
        }

        return continue_authentication_challenge(packet.data)
                   ? ProtocolResult::Success
                   : ProtocolResult::AuthenticationRequired;
    }

    ProtocolResult
    FirebirdAuthenticationHandler::handle_attach_database(const FirebirdPacket& packet)
    {
        if (current_state_ != FirebirdState::Authenticated) {
            send_error(1, "Authentication required before database attachment");
            return ProtocolResult::AuthenticationRequired;
        }

        std::size_t offset = 0;
        client_database_ = packet.read_string(offset);

        if (client_database_.empty()) {
            send_error(1, "Database path required");
            return ProtocolResult::ProtocolError;
        }

        // TODO: Implement actual database attachment using catalog_
        transition_to_state(FirebirdState::Connected);
        send_response(0, "Database attached successfully");

        return ProtocolResult::Success;
    }

    bool FirebirdAuthenticationHandler::start_password_authentication(const std::string& username,
                                                                      const std::string& password)
    {
        if (!auth_context_)
            return false;

        transition_to_state(FirebirdState::Authenticating);

        auth_context_->set_username(username);
        auth_context_->set_credential("password", password);
        auth_context_->set_remote_address(connection_ ? connection_->get_peer_address() : "");

        ScratchBird::AuthenticationResult result = auth_manager_->authenticate_user(*auth_context_);

        switch (result) {
        case ScratchBird::AuthenticationResult::Success:
            transition_to_state(FirebirdState::Authenticated);
            send_authentication_success();
            return true;

        case ScratchBird::AuthenticationResult::RequiresTwoFactor:
            // Initiate 2FA challenge
            active_challenge_ = auth_manager_->initiate_challenge(
                username, ScratchBird::AuthenticationMethod::TwoFactor);
            if (active_challenge_) {
                transition_to_state(FirebirdState::ContinueAuthInProgress);
                send_authentication_challenge(
                    std::vector<std::uint8_t>(active_challenge_->get_challenge_data().begin(),
                                              active_challenge_->get_challenge_data().end()));
                return true; // Authentication continues
            }
            break;

        default:
            send_error(1, "Authentication failed: " + ScratchBird::to_string(result));
            break;
        }

        transition_to_state(FirebirdState::WaitingForAuth);
        return false;
    }

    bool FirebirdAuthenticationHandler::start_trusted_authentication()
    {
        if (!auth_context_)
            return false;

        // Simplified trusted authentication - in real implementation this would
        // use platform-specific APIs (SSPI on Windows, PAM on Linux)
        auth_context_->set_username("TRUSTED_USER");
        auth_context_->set_remote_address(connection_ ? connection_->get_peer_address() : "");
        auth_context_->set_authenticated(true);

        transition_to_state(FirebirdState::Authenticated);
        send_authentication_success();

        return true;
    }

    bool FirebirdAuthenticationHandler::continue_authentication_challenge(
        const std::vector<std::uint8_t>& challenge_data)
    {
        if (!auth_manager_ || !active_challenge_ || !auth_context_) {
            return false;
        }

        std::string response(challenge_data.begin(), challenge_data.end());
        active_challenge_->set_response(response);

        ScratchBird::AuthenticationResult result =
            auth_manager_->complete_challenge(*active_challenge_, *auth_context_);

        if (result == ScratchBird::AuthenticationResult::Success) {
            active_challenge_.reset();
            transition_to_state(FirebirdState::Authenticated);
            send_authentication_success();
            return true;
        }

        active_challenge_.reset();
        transition_to_state(FirebirdState::WaitingForAuth);
        send_error(1, "Challenge authentication failed");
        return false;
    }

    void FirebirdAuthenticationHandler::send_response(std::uint32_t status_code,
                                                      const std::string& message)
    {
        auto response = std::make_unique<FirebirdResponseMessage>(status_code, message);
        outgoing_messages_.push(std::move(response));
    }

    void FirebirdAuthenticationHandler::send_authentication_challenge(
        const std::vector<std::uint8_t>& challenge_data)
    {
        FirebirdProtocolOp op = supports_continue_auth() ? op_cont_auth : op_trusted_auth;
        auto challenge = std::make_unique<FirebirdAuthMessage>(op, challenge_data);
        outgoing_messages_.push(std::move(challenge));
    }

    void FirebirdAuthenticationHandler::send_authentication_success()
    {
        send_response(0, "Authentication successful");
    }

    void FirebirdAuthenticationHandler::send_error(std::uint32_t error_code,
                                                   const std::string& error_message)
    {
        send_response(error_code, error_message);
        last_error_ = error_message;
    }

    ProtocolResult
    FirebirdAuthenticationHandler::handle_message(std::unique_ptr<ProtocolMessage> message)
    {
        // This method handles messages that have already been parsed
        // For Firebird protocol, most work is done in process_incoming_data
        return ProtocolResult::Success;
    }

    bool FirebirdAuthenticationHandler::has_outgoing_messages() const
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        return !outgoing_messages_.empty();
    }

    std::vector<std::unique_ptr<ProtocolMessage>>
    FirebirdAuthenticationHandler::get_outgoing_messages()
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        std::vector<std::unique_ptr<ProtocolMessage>> messages;

        while (!outgoing_messages_.empty()) {
            messages.push_back(std::move(outgoing_messages_.front()));
            outgoing_messages_.pop();
        }

        return messages;
    }

    std::string FirebirdAuthenticationHandler::get_current_state() const
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        return state_to_string(current_state_);
    }

    bool FirebirdAuthenticationHandler::is_authenticated() const
    {
        return current_state_ == FirebirdState::Authenticated ||
               current_state_ == FirebirdState::Connected;
    }

    bool FirebirdAuthenticationHandler::requires_authentication() const
    {
        return current_state_ == FirebirdState::WaitingForAuth ||
               current_state_ == FirebirdState::Authenticating ||
               current_state_ == FirebirdState::TrustedAuthInProgress ||
               current_state_ == FirebirdState::ContinueAuthInProgress;
    }

    void FirebirdAuthenticationHandler::handle_protocol_error(const std::string& error_message)
    {
        last_error_ = error_message;
        transition_to_state(FirebirdState::Error);
        std::cerr << "Firebird protocol error: " << error_message << std::endl;
    }

    std::string FirebirdAuthenticationHandler::get_last_error() const
    {
        return last_error_;
    }

    void FirebirdAuthenticationHandler::transition_to_state(FirebirdState new_state)
    {
        if (is_valid_state_transition(current_state_, new_state)) {
            current_state_ = new_state;
        }
    }

    std::string FirebirdAuthenticationHandler::state_to_string(FirebirdState state) const
    {
        switch (state) {
        case FirebirdState::Connecting:
            return "Connecting";
        case FirebirdState::WaitingForAuth:
            return "WaitingForAuth";
        case FirebirdState::Authenticating:
            return "Authenticating";
        case FirebirdState::TrustedAuthInProgress:
            return "TrustedAuthInProgress";
        case FirebirdState::ContinueAuthInProgress:
            return "ContinueAuthInProgress";
        case FirebirdState::Authenticated:
            return "Authenticated";
        case FirebirdState::Connected:
            return "Connected";
        case FirebirdState::Error:
            return "Error";
        case FirebirdState::Disconnected:
            return "Disconnected";
        default:
            return "Unknown";
        }
    }

    bool FirebirdAuthenticationHandler::is_valid_state_transition(FirebirdState from,
                                                                  FirebirdState to) const
    {
        // Implement state transition validation
        // For now, allow all transitions (simplified)
        return true;
    }

    // ============================================================================
    // FirebirdAuthMessageFramer Implementation
    // ============================================================================

    FirebirdAuthMessageFramer::FirebirdAuthMessageFramer()
        : expected_packet_length_(0), has_header_(false)
    {
    }

    std::vector<std::vector<std::uint8_t>>
    FirebirdAuthMessageFramer::frame_messages(const std::vector<std::uint8_t>& data)
    {
        std::vector<std::vector<std::uint8_t>> messages;

        // Add data to buffer
        buffer_.insert(buffer_.end(), data.begin(), data.end());

        while (buffer_.size() >= FIREBIRD_PACKET_HEADER_SIZE) {
            if (!has_header_) {
                // Read packet length
                expected_packet_length_ =
                    buffer_[0] | (buffer_[1] << 8) | (buffer_[2] << 16) | (buffer_[3] << 24);
                has_header_ = true;
            }

            if (buffer_.size() >= expected_packet_length_ + FIREBIRD_PACKET_HEADER_SIZE) {
                // Extract complete packet
                std::vector<std::uint8_t> packet(buffer_.begin(), buffer_.begin() +
                                                                      expected_packet_length_ +
                                                                      FIREBIRD_PACKET_HEADER_SIZE);
                messages.push_back(std::move(packet));

                // Remove processed data
                buffer_.erase(buffer_.begin(), buffer_.begin() + expected_packet_length_ +
                                                   FIREBIRD_PACKET_HEADER_SIZE);

                has_header_ = false;
                expected_packet_length_ = 0;
            } else {
                // Need more data
                needs_more_data_ = true;
                break;
            }
        }

        needs_more_data_ = (has_header_ && buffer_.size() < expected_packet_length_ +
                                                                FIREBIRD_PACKET_HEADER_SIZE) ||
                           (!has_header_ && buffer_.size() < FIREBIRD_PACKET_HEADER_SIZE);
        return messages;
    }

    bool FirebirdAuthMessageFramer::needs_more_data() const
    {
        return needs_more_data_.load();
    }

    void FirebirdAuthMessageFramer::reset()
    {
        buffer_.clear();
        expected_packet_length_ = 0;
        has_header_ = false;
        needs_more_data_ = false;
    }

} // namespace scratchbird::engine
