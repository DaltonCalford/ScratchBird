#include "scratchbird/engine/protocol_handler.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/network_server.h"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace scratchbird::engine
{

    //=============================================================================
    // ProtocolVersion Implementation
    //=============================================================================

    bool ProtocolVersion::is_compatible_with(const ProtocolVersion& other) const
    {
        if (type != other.type) {
            return false;
        }

        // Major version must match for compatibility
        if (major != other.major) {
            return false;
        }

        // Minor version backwards compatibility: newer server can handle older clients
        return minor >= other.minor;
    }

    std::string ProtocolVersion::to_string() const
    {
        std::string type_str;
        switch (type) {
        case ProtocolType::FirebirdWire:
            type_str = "Firebird";
            break;
        case ProtocolType::ScratchBirdNative:
            type_str = "ScratchBird";
            break;
        case ProtocolType::PostgreSQL:
            type_str = "PostgreSQL";
            break;
        case ProtocolType::MySQL:
            type_str = "MySQL";
            break;
        default:
            type_str = "Unknown";
            break;
        }

        return type_str + " v" + std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(build);
    }

    //=============================================================================
    // MessageQueue Implementation
    //=============================================================================

    MessageQueue::MessageQueue() : total_processed_(0)
    {
        // Initialize priority counters
        for (auto& counter : priority_counters_) {
            counter = 0;
        }
    }

    MessageQueue::~MessageQueue()
    {
        clear();
    }

    void MessageQueue::enqueue(std::unique_ptr<ProtocolMessage> message)
    {
        if (!message) {
            return;
        }

        std::lock_guard<std::mutex> lock(queue_mutex_);

        // Update message timestamp if not set
        if (message->timestamp_ms == 0) {
            auto now = std::chrono::system_clock::now();
            message->timestamp_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                    .count();
        }

        // Track correlation if present
        if (message->correlation_id != 0) {
            correlation_map_[message->correlation_id] = message->clone();
        }

        // Update priority counter
        int priority_index = static_cast<int>(message->priority);
        if (priority_index >= 0 && priority_index < 4) {
            priority_counters_[priority_index]++;
        }

        priority_queue_.push(std::move(message));
    }

    std::unique_ptr<ProtocolMessage> MessageQueue::dequeue()
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        if (priority_queue_.empty()) {
            return nullptr;
        }

        auto message =
            std::move(const_cast<std::unique_ptr<ProtocolMessage>&>(priority_queue_.top()));
        priority_queue_.pop();

        // Remove from correlation map if present
        if (message->correlation_id != 0) {
            correlation_map_.erase(message->correlation_id);
        }

        // Update counters
        int priority_index = static_cast<int>(message->priority);
        if (priority_index >= 0 && priority_index < 4) {
            priority_counters_[priority_index]--;
        }
        total_processed_++;

        return message;
    }

    std::unique_ptr<ProtocolMessage>
    MessageQueue::dequeue_by_correlation_id(std::uint64_t correlation_id)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        auto it = correlation_map_.find(correlation_id);
        if (it == correlation_map_.end()) {
            return nullptr;
        }

        auto message = std::move(it->second);
        correlation_map_.erase(it);
        total_processed_++;

        return message;
    }

    std::size_t MessageQueue::size() const
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return priority_queue_.size();
    }

    std::size_t MessageQueue::size_by_priority(MessagePriority priority) const
    {
        int priority_index = static_cast<int>(priority);
        if (priority_index >= 0 && priority_index < 4) {
            return priority_counters_[priority_index].load();
        }
        return 0;
    }

    bool MessageQueue::empty() const
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return priority_queue_.empty();
    }

    void MessageQueue::clear()
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        // Clear priority queue
        while (!priority_queue_.empty()) {
            priority_queue_.pop();
        }

        // Clear correlation map
        correlation_map_.clear();

        // Reset counters
        for (auto& counter : priority_counters_) {
            counter = 0;
        }
    }

    std::uint64_t MessageQueue::get_messages_by_priority(MessagePriority priority) const
    {
        int priority_index = static_cast<int>(priority);
        if (priority_index >= 0 && priority_index < 4) {
            return priority_counters_[priority_index].load();
        }
        return 0;
    }

    //=============================================================================
    // CorrelationTracker Implementation
    //=============================================================================

    CorrelationTracker::CorrelationTracker() : next_correlation_id_(1), total_correlations_(0) {}

    CorrelationTracker::~CorrelationTracker()
    {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        pending_requests_.clear();
        correlation_timestamps_.clear();
    }

    std::uint64_t CorrelationTracker::create_correlation_id()
    {
        total_correlations_++;
        return next_correlation_id_++;
    }

    void CorrelationTracker::register_request(std::uint64_t correlation_id,
                                              std::unique_ptr<ProtocolMessage> request)
    {
        if (!request || correlation_id == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(tracker_mutex_);

        auto now = std::chrono::system_clock::now();
        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        pending_requests_[correlation_id] = std::move(request);
        correlation_timestamps_[correlation_id] = timestamp;
    }

    std::unique_ptr<ProtocolMessage> CorrelationTracker::get_request(std::uint64_t correlation_id)
    {
        std::lock_guard<std::mutex> lock(tracker_mutex_);

        auto it = pending_requests_.find(correlation_id);
        if (it == pending_requests_.end()) {
            return nullptr;
        }

        auto request = std::move(it->second);
        pending_requests_.erase(it);
        correlation_timestamps_.erase(correlation_id);

        return request;
    }

    bool CorrelationTracker::has_pending_request(std::uint64_t correlation_id) const
    {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        return pending_requests_.find(correlation_id) != pending_requests_.end();
    }

    void CorrelationTracker::complete_correlation(std::uint64_t correlation_id)
    {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        pending_requests_.erase(correlation_id);
        correlation_timestamps_.erase(correlation_id);
    }

    void CorrelationTracker::cleanup_expired_correlations(std::int64_t expiry_time_ms)
    {
        std::lock_guard<std::mutex> lock(tracker_mutex_);

        auto now = std::chrono::system_clock::now();
        auto current_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::vector<std::uint64_t> expired_correlations;

        for (const auto& [correlation_id, timestamp] : correlation_timestamps_) {
            if ((current_time - timestamp) > expiry_time_ms) {
                expired_correlations.push_back(correlation_id);
            }
        }

        for (auto correlation_id : expired_correlations) {
            pending_requests_.erase(correlation_id);
            correlation_timestamps_.erase(correlation_id);
        }
    }

    std::size_t CorrelationTracker::get_pending_count() const
    {
        std::lock_guard<std::mutex> lock(tracker_mutex_);
        return pending_requests_.size();
    }

    //=============================================================================
    // MessageFramer Implementation
    //=============================================================================

    MessageFramer::MessageFramer() : needs_more_data_(false) {}

    std::size_t MessageFramer::get_buffer_size() const
    {
        return buffer_.size();
    }

    void MessageFramer::clear_buffer()
    {
        buffer_.clear();
        needs_more_data_ = false;
    }

    //=============================================================================
    // ProtocolHandlerFactory Implementation
    //=============================================================================

    ProtocolHandlerFactory& ProtocolHandlerFactory::get_instance()
    {
        static ProtocolHandlerFactory instance;
        return instance;
    }

    void ProtocolHandlerFactory::register_handler(ProtocolType type, HandlerCreator creator)
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        handlers_[type] = std::move(creator);
    }

    std::unique_ptr<ProtocolHandler> ProtocolHandlerFactory::create_handler(ProtocolType type)
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        auto it = handlers_.find(type);
        if (it == handlers_.end()) {
            return nullptr;
        }

        return it->second();
    }

    ProtocolType
    ProtocolHandlerFactory::detect_protocol(const std::vector<std::uint8_t>& initial_data)
    {
        if (initial_data.empty()) {
            return ProtocolType::Unknown;
        }

        // Simple protocol detection based on first few bytes
        // This is a placeholder - real implementation would have more sophisticated detection

        // Firebird wire protocol typically starts with specific byte patterns
        if (initial_data.size() >= 4) {
            // Check for Firebird protocol signature
            if (initial_data[0] == 0x00 && initial_data[1] == 0x00 && initial_data[2] == 0x00 &&
                initial_data[3] == 0x01) {
                return ProtocolType::FirebirdWire;
            }
        }

        // Default to ScratchBird native protocol for now
        return ProtocolType::ScratchBirdNative;
    }

    std::vector<ProtocolType> ProtocolHandlerFactory::get_supported_protocols() const
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        std::vector<ProtocolType> protocols;
        for (const auto& [type, creator] : handlers_) {
            protocols.push_back(type);
        }

        return protocols;
    }

    bool ProtocolHandlerFactory::is_version_supported(ProtocolType type,
                                                      const ProtocolVersion& version)
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        auto it = supported_versions_.find(type);
        if (it == supported_versions_.end()) {
            return false;
        }

        for (const auto& supported : it->second) {
            if (supported.is_compatible_with(version)) {
                return true;
            }
        }

        return false;
    }

    std::vector<ProtocolVersion>
    ProtocolHandlerFactory::get_supported_versions(ProtocolType type) const
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        auto it = supported_versions_.find(type);
        if (it != supported_versions_.end()) {
            return it->second;
        }

        return {};
    }

    //=============================================================================
    // ProtocolHandlerManager Implementation
    //=============================================================================

    ProtocolHandlerManager::ProtocolHandlerManager(TcpConnection* connection,
                                                   CatalogManager* catalog)
        : connection_(connection), catalog_(catalog), detected_protocol_(ProtocolType::Unknown),
          initialized_(false), messages_processed_(0), protocol_errors_(0)
    {
    }

    ProtocolHandlerManager::~ProtocolHandlerManager()
    {
        shutdown();
    }

    bool ProtocolHandlerManager::initialize()
    {
        std::lock_guard<std::mutex> lock(manager_mutex_);

        if (initialized_.load()) {
            return true;
        }

        // For testing purposes, we allow initialization without connection
        // but catalog is required for database operations
        if (!catalog_) {
            return false;
        }

        // Protocol will be detected on first data reception
        initialized_ = true;
        return true;
    }

    void ProtocolHandlerManager::shutdown()
    {
        std::lock_guard<std::mutex> lock(manager_mutex_);

        if (!initialized_.load()) {
            return;
        }

        if (current_handler_) {
            current_handler_->shutdown();
            current_handler_.reset();
        }

        incoming_queue_.clear();
        outgoing_queue_.clear();

        initialized_ = false;
    }

    ProtocolResult
    ProtocolHandlerManager::process_incoming_data(const std::vector<std::uint8_t>& data)
    {
        if (!initialized_.load() || data.empty()) {
            return ProtocolResult::ProtocolError;
        }

        std::lock_guard<std::mutex> lock(manager_mutex_);

        // Detect protocol on first data if not already detected
        if (detected_protocol_ == ProtocolType::Unknown) {
            if (!detect_and_initialize_protocol(data)) {
                protocol_errors_++;
                return ProtocolResult::ProtocolError;
            }
        }

        // Process data through current handler
        if (current_handler_) {
            ProtocolResult result = current_handler_->process_incoming_data(data);
            if (result == ProtocolResult::Success) {
                messages_processed_++;
            } else if (result == ProtocolResult::ProtocolError) {
                protocol_errors_++;
            }
            return result;
        }

        return ProtocolResult::ProtocolError;
    }

    ProtocolResult ProtocolHandlerManager::process_next_message()
    {
        if (!initialized_.load()) {
            return ProtocolResult::ProtocolError;
        }

        auto message = incoming_queue_.dequeue();
        if (!message) {
            return ProtocolResult::NeedMoreData;
        }

        std::lock_guard<std::mutex> lock(manager_mutex_);

        if (current_handler_) {
            ProtocolResult result = current_handler_->handle_message(std::move(message));
            if (result == ProtocolResult::Success) {
                messages_processed_++;
            } else if (result == ProtocolResult::ProtocolError) {
                protocol_errors_++;
            }
            return result;
        }

        return ProtocolResult::ProtocolError;
    }

    bool ProtocolHandlerManager::has_pending_messages() const
    {
        return !incoming_queue_.empty() || !outgoing_queue_.empty();
    }

    ProtocolType ProtocolHandlerManager::get_current_protocol() const
    {
        return detected_protocol_;
    }

    std::unique_ptr<ProtocolHandler> ProtocolHandlerManager::get_current_handler()
    {
        std::lock_guard<std::mutex> lock(manager_mutex_);

        // Return a copy/clone of current handler if needed
        // For now, return nullptr as we don't want to transfer ownership
        return nullptr;
    }

    bool ProtocolHandlerManager::switch_protocol(ProtocolType new_protocol)
    {
        std::lock_guard<std::mutex> lock(manager_mutex_);

        if (detected_protocol_ == new_protocol && current_handler_) {
            return true;
        }

        // Shutdown current handler
        if (current_handler_) {
            current_handler_->shutdown();
            current_handler_.reset();
        }

        // Create new handler
        auto& factory = ProtocolHandlerFactory::get_instance();
        current_handler_ = factory.create_handler(new_protocol);

        if (!current_handler_) {
            return false;
        }

        // Initialize new handler
        if (!current_handler_->initialize(connection_, catalog_)) {
            current_handler_.reset();
            return false;
        }

        detected_protocol_ = new_protocol;
        return true;
    }

    void ProtocolHandlerManager::enqueue_message(std::unique_ptr<ProtocolMessage> message)
    {
        if (message) {
            incoming_queue_.enqueue(std::move(message));
        }
    }

    std::vector<std::unique_ptr<ProtocolMessage>> ProtocolHandlerManager::get_outgoing_messages()
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

    std::string ProtocolHandlerManager::get_current_state() const
    {
        std::lock_guard<std::mutex> lock(manager_mutex_);

        if (current_handler_) {
            return current_handler_->get_current_state();
        }

        return "Uninitialized";
    }

    bool ProtocolHandlerManager::detect_and_initialize_protocol(
        const std::vector<std::uint8_t>& initial_data)
    {
        auto& factory = ProtocolHandlerFactory::get_instance();
        detected_protocol_ = factory.detect_protocol(initial_data);

        if (detected_protocol_ == ProtocolType::Unknown) {
            handle_protocol_error("Unable to detect protocol from initial data");
            return false;
        }

        // Create and initialize handler for detected protocol
        current_handler_ = factory.create_handler(detected_protocol_);
        if (!current_handler_) {
            handle_protocol_error("No handler available for detected protocol");
            return false;
        }

        if (!current_handler_->initialize(connection_, catalog_)) {
            handle_protocol_error("Failed to initialize protocol handler");
            current_handler_.reset();
            return false;
        }

        return true;
    }

    void ProtocolHandlerManager::handle_protocol_error(const std::string& error)
    {
        std::cerr << "Protocol error: " << error << std::endl;
        protocol_errors_++;
    }

} // namespace scratchbird::engine
