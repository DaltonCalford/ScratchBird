#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// Forward declarations
    class TcpConnection;
    class CatalogManager;

    /// Protocol types enumeration
    enum class ProtocolType {
        Unknown = 0,
        FirebirdWire = 1,
        ScratchBirdNative = 2,
        PostgreSQL = 3,
        MySQL = 4
    };

    /// Protocol version information
    struct ProtocolVersion {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint16_t build = 0;
        ProtocolType type = ProtocolType::Unknown;

        bool is_compatible_with(const ProtocolVersion& other) const;
        std::string to_string() const;
    };

    /// Message priority levels
    enum class MessagePriority { Low = 0, Normal = 1, High = 2, Critical = 3 };

    /// Protocol message base class
    struct ProtocolMessage {
        std::uint64_t message_id = 0;
        std::uint64_t correlation_id = 0;
        MessagePriority priority = MessagePriority::Normal;
        std::vector<std::uint8_t> payload;
        std::int64_t timestamp_ms = 0;
        std::string message_type;

        virtual ~ProtocolMessage() = default;
        virtual std::unique_ptr<ProtocolMessage> clone() const = 0;
    };

    /// Protocol handler result codes
    enum class ProtocolResult {
        Success = 0,
        ContinueProcessing = 1,
        NeedMoreData = 2,
        ProtocolError = 3,
        AuthenticationRequired = 4,
        ConnectionClosed = 5,
        InternalError = 6
    };

    /// Protocol handler interface
    class ProtocolHandler
    {
      public:
        virtual ~ProtocolHandler() = default;

        /// Protocol identification
        virtual ProtocolType get_protocol_type() const = 0;
        virtual ProtocolVersion get_supported_version() const = 0;
        virtual bool supports_version(const ProtocolVersion& version) const = 0;

        /// Connection lifecycle
        virtual bool initialize(TcpConnection* connection, CatalogManager* catalog) = 0;
        virtual void shutdown() = 0;
        virtual bool is_initialized() const = 0;

        /// Message processing
        virtual ProtocolResult process_incoming_data(const std::vector<std::uint8_t>& data) = 0;
        virtual ProtocolResult handle_message(std::unique_ptr<ProtocolMessage> message) = 0;
        virtual bool has_outgoing_messages() const = 0;
        virtual std::vector<std::unique_ptr<ProtocolMessage>> get_outgoing_messages() = 0;

        /// Protocol state management
        virtual std::string get_current_state() const = 0;
        virtual bool is_authenticated() const = 0;
        virtual bool requires_authentication() const = 0;

        /// Error handling
        virtual void handle_protocol_error(const std::string& error_message) = 0;
        virtual std::string get_last_error() const = 0;
    };

    /// Message queue for prioritized message processing
    class MessageQueue
    {
      public:
        MessageQueue();
        ~MessageQueue();

        /// Queue management
        void enqueue(std::unique_ptr<ProtocolMessage> message);
        std::unique_ptr<ProtocolMessage> dequeue();
        std::unique_ptr<ProtocolMessage> dequeue_by_correlation_id(std::uint64_t correlation_id);

        /// Queue status
        std::size_t size() const;
        std::size_t size_by_priority(MessagePriority priority) const;
        bool empty() const;
        void clear();

        /// Statistics
        std::uint64_t get_total_messages_processed() const
        {
            return total_processed_;
        }
        std::uint64_t get_messages_by_priority(MessagePriority priority) const;

      private:
        struct MessageComparator {
            bool operator()(const std::unique_ptr<ProtocolMessage>& lhs,
                            const std::unique_ptr<ProtocolMessage>& rhs) const
            {
                // Higher priority has lower value for priority queue
                if (lhs->priority != rhs->priority) {
                    return static_cast<int>(lhs->priority) < static_cast<int>(rhs->priority);
                }
                return lhs->timestamp_ms > rhs->timestamp_ms; // FIFO for same priority
            }
        };

        mutable std::mutex queue_mutex_;
        std::priority_queue<std::unique_ptr<ProtocolMessage>,
                            std::vector<std::unique_ptr<ProtocolMessage>>, MessageComparator>
            priority_queue_;

        std::unordered_map<std::uint64_t, std::unique_ptr<ProtocolMessage>> correlation_map_;
        std::atomic<std::uint64_t> total_processed_;
        std::array<std::atomic<std::uint64_t>, 4> priority_counters_;
    };

    /// Protocol state machine base class
    class ProtocolStateMachine
    {
      public:
        virtual ~ProtocolStateMachine() = default;

        /// State management
        virtual std::string get_current_state() const = 0;
        virtual bool transition_to_state(const std::string& new_state) = 0;
        virtual bool is_valid_transition(const std::string& from_state,
                                         const std::string& to_state) const = 0;
        virtual std::vector<std::string> get_valid_next_states() const = 0;

        /// State callbacks
        virtual void on_state_enter(const std::string& /*state*/) {}
        virtual void on_state_exit(const std::string& /*state*/) {}
        virtual void on_invalid_transition(const std::string& /*from_state*/,
                                           const std::string& /*to_state*/)
        {
        }

      protected:
        std::string current_state_;
        mutable std::mutex state_mutex_;
    };

    /// Request/Response correlation tracker
    class CorrelationTracker
    {
      public:
        CorrelationTracker();
        ~CorrelationTracker();

        /// Correlation management
        std::uint64_t create_correlation_id();
        void register_request(std::uint64_t correlation_id,
                              std::unique_ptr<ProtocolMessage> request);
        std::unique_ptr<ProtocolMessage> get_request(std::uint64_t correlation_id);
        bool has_pending_request(std::uint64_t correlation_id) const;
        void complete_correlation(std::uint64_t correlation_id);

        /// Cleanup and statistics
        void cleanup_expired_correlations(std::int64_t expiry_time_ms);
        std::size_t get_pending_count() const;
        std::uint64_t get_total_correlations() const
        {
            return total_correlations_;
        }

      private:
        mutable std::mutex tracker_mutex_;
        std::atomic<std::uint64_t> next_correlation_id_;
        std::atomic<std::uint64_t> total_correlations_;

        std::unordered_map<std::uint64_t, std::unique_ptr<ProtocolMessage>> pending_requests_;
        std::unordered_map<std::uint64_t, std::int64_t> correlation_timestamps_;
    };

    /// Protocol message framer for handling partial messages
    class MessageFramer
    {
      public:
        MessageFramer();
        virtual ~MessageFramer() = default;

        /// Frame processing
        virtual std::vector<std::vector<std::uint8_t>>
        frame_messages(const std::vector<std::uint8_t>& data) = 0;
        virtual bool needs_more_data() const = 0;
        virtual void reset() = 0;

        /// Buffer management
        std::size_t get_buffer_size() const;
        void clear_buffer();

      protected:
        std::vector<std::uint8_t> buffer_;
        std::atomic<bool> needs_more_data_;
    };

    /// Protocol handler factory
    class ProtocolHandlerFactory
    {
      public:
        using HandlerCreator = std::function<std::unique_ptr<ProtocolHandler>()>;

        /// Factory management
        static ProtocolHandlerFactory& get_instance();
        void register_handler(ProtocolType type, HandlerCreator creator);
        std::unique_ptr<ProtocolHandler> create_handler(ProtocolType type);

        /// Protocol detection
        ProtocolType detect_protocol(const std::vector<std::uint8_t>& initial_data);
        std::vector<ProtocolType> get_supported_protocols() const;

        /// Version support
        bool is_version_supported(ProtocolType type, const ProtocolVersion& version);
        std::vector<ProtocolVersion> get_supported_versions(ProtocolType type) const;

      private:
        ProtocolHandlerFactory() = default;

        mutable std::mutex factory_mutex_;
        std::unordered_map<ProtocolType, HandlerCreator> handlers_;
        std::unordered_map<ProtocolType, std::vector<ProtocolVersion>> supported_versions_;
    };

    /// Protocol handler manager - orchestrates all protocol handling
    class ProtocolHandlerManager
    {
      public:
        ProtocolHandlerManager(TcpConnection* connection, CatalogManager* catalog);
        ~ProtocolHandlerManager();

        /// Lifecycle
        bool initialize();
        void shutdown();
        bool is_initialized() const
        {
            return initialized_;
        }

        /// Protocol handling
        ProtocolResult process_incoming_data(const std::vector<std::uint8_t>& data);
        ProtocolResult process_next_message();
        bool has_pending_messages() const;

        /// Protocol management
        ProtocolType get_current_protocol() const;
        std::unique_ptr<ProtocolHandler> get_current_handler();
        bool switch_protocol(ProtocolType new_protocol);

        /// Message handling
        void enqueue_message(std::unique_ptr<ProtocolMessage> message);
        std::vector<std::unique_ptr<ProtocolMessage>> get_outgoing_messages();

        /// Statistics and monitoring
        std::uint64_t get_messages_processed() const
        {
            return messages_processed_;
        }
        std::uint64_t get_protocol_errors() const
        {
            return protocol_errors_;
        }
        std::string get_current_state() const;

      private:
        TcpConnection* connection_;
        CatalogManager* catalog_;

        std::unique_ptr<ProtocolHandler> current_handler_;
        std::unique_ptr<MessageFramer> message_framer_;
        MessageQueue incoming_queue_;
        MessageQueue outgoing_queue_;
        CorrelationTracker correlation_tracker_;

        ProtocolType detected_protocol_;
        std::atomic<bool> initialized_;
        std::atomic<std::uint64_t> messages_processed_;
        std::atomic<std::uint64_t> protocol_errors_;

        mutable std::mutex manager_mutex_;

        bool detect_and_initialize_protocol(const std::vector<std::uint8_t>& initial_data);
        void handle_protocol_error(const std::string& error);
    };

} // namespace scratchbird::engine
