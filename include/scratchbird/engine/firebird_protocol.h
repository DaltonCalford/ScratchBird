#pragma once

#include "scratchbird/engine/protocol_handler.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    /// Firebird protocol constants (from Firebird 6.0 source)
    namespace FirebirdProtocol
    {
        // Protocol versions with FB_PROTOCOL_FLAG (0x8000)
        constexpr std::uint16_t FB_PROTOCOL_FLAG = 0x8000;
        constexpr std::uint16_t PROTOCOL_VERSION10 = 10;
        constexpr std::uint16_t PROTOCOL_VERSION11 = (FB_PROTOCOL_FLAG | 11);
        constexpr std::uint16_t PROTOCOL_VERSION12 = (FB_PROTOCOL_FLAG | 12);
        constexpr std::uint16_t PROTOCOL_VERSION13 = (FB_PROTOCOL_FLAG | 13);
        constexpr std::uint16_t PROTOCOL_VERSION14 = (FB_PROTOCOL_FLAG | 14);
        constexpr std::uint16_t PROTOCOL_VERSION15 = (FB_PROTOCOL_FLAG | 15);
        constexpr std::uint16_t PROTOCOL_VERSION16 = (FB_PROTOCOL_FLAG | 16);
        constexpr std::uint16_t PROTOCOL_VERSION17 = (FB_PROTOCOL_FLAG | 17);
        constexpr std::uint16_t PROTOCOL_VERSION18 = (FB_PROTOCOL_FLAG | 18);
        constexpr std::uint16_t PROTOCOL_VERSION19 = (FB_PROTOCOL_FLAG | 19);
        constexpr std::uint16_t PROTOCOL_VERSION20 = (FB_PROTOCOL_FLAG | 20);

        // Operation codes (P_OP enum from protocol.h)
        constexpr std::uint32_t op_void = 0;                // Packet has been voided
        constexpr std::uint32_t op_connect = 1;             // Connect to remote server
        constexpr std::uint32_t op_exit = 2;                // Remote end has exitted
        constexpr std::uint32_t op_accept = 3;              // Server accepts connection
        constexpr std::uint32_t op_reject = 4;              // Server rejects connection
        constexpr std::uint32_t op_disconnect = 6;          // Connect is going away
        constexpr std::uint32_t op_response = 9;            // Generic response block
        constexpr std::uint32_t op_attach = 19;             // Attach database
        constexpr std::uint32_t op_create = 20;             // Create database
        constexpr std::uint32_t op_detach = 21;             // Detach database
        constexpr std::uint32_t op_transaction = 29;        // Transaction operations
        constexpr std::uint32_t op_commit = 30;             // Commit transaction
        constexpr std::uint32_t op_rollback = 31;           // Rollback transaction
        constexpr std::uint32_t op_prepare = 32;            // Prepare transaction
        constexpr std::uint32_t op_reconnect = 33;          // Reconnect transaction
        constexpr std::uint32_t op_create_blob = 34;        // Create blob
        constexpr std::uint32_t op_open_blob = 35;          // Open blob
        constexpr std::uint32_t op_get_segment = 36;        // Get blob segment
        constexpr std::uint32_t op_put_segment = 37;        // Put blob segment
        constexpr std::uint32_t op_cancel_blob = 38;        // Cancel blob
        constexpr std::uint32_t op_close_blob = 39;         // Close blob
        constexpr std::uint32_t op_info_database = 40;      // Database information
        constexpr std::uint32_t op_info_request = 41;       // Request information
        constexpr std::uint32_t op_info_transaction = 42;   // Transaction information
        constexpr std::uint32_t op_info_blob = 43;          // Blob information
        constexpr std::uint32_t op_allocate_statement = 62; // Allocate statement handle
        constexpr std::uint32_t op_execute = 63;            // Execute prepared statement
        constexpr std::uint32_t op_exec_immediate = 64;     // Execute immediate
        constexpr std::uint32_t op_fetch = 65;              // Fetch records
        constexpr std::uint32_t op_fetch_response = 66;     // Fetch response
        constexpr std::uint32_t op_free_statement = 67;     // Free statement
        constexpr std::uint32_t op_prepare_statement = 68;  // Prepare statement
        constexpr std::uint32_t op_set_cursor = 69;         // Set cursor name
        constexpr std::uint32_t op_info_sql = 70;           // SQL information
        constexpr std::uint32_t op_dummy = 71;              // Dummy packet
        constexpr std::uint32_t op_service_attach = 82;     // Service attach
        constexpr std::uint32_t op_service_detach = 83;     // Service detach
        constexpr std::uint32_t op_service_info = 84;       // Service info
        constexpr std::uint32_t op_service_start = 85;      // Service start
        constexpr std::uint32_t op_ping = 93;               // Ping operation
        constexpr std::uint32_t op_accept_data = 94;        // Accept with data
        constexpr std::uint32_t op_cond_accept = 98;        // Conditional accept

        // Connection parameters
        constexpr std::uint32_t isc_dpb_version1 = 1;
        constexpr std::uint32_t isc_dpb_user_name = 28;
        constexpr std::uint32_t isc_dpb_password = 29;
        constexpr std::uint32_t isc_dpb_sql_role_name = 60;
        constexpr std::uint32_t isc_dpb_lc_ctype = 48;
        constexpr std::uint32_t isc_dpb_utf8_filename = 77;

        // Architecture types (P_ARCH enum from protocol.h)
        constexpr std::uint32_t arch_generic = 1;       // Generic -- always use canonical forms
        constexpr std::uint32_t arch_sun = 3;           // Sun workstation
        constexpr std::uint32_t arch_sun4 = 8;          // Sun 4 workstation
        constexpr std::uint32_t arch_sunx86 = 9;        // Sun x86 workstation
        constexpr std::uint32_t arch_hpux = 10;         // HP/UX workstation
        constexpr std::uint32_t arch_rt = 14;           // Real time
        constexpr std::uint32_t arch_intel_32 = 29;     // Generic Intel chip w/32-bit compilation
        constexpr std::uint32_t arch_linux = 36;        // Linux
        constexpr std::uint32_t arch_freebsd = 37;      // FreeBSD
        constexpr std::uint32_t arch_netbsd = 38;       // NetBSD
        constexpr std::uint32_t arch_darwin_ppc = 39;   // Darwin PowerPC
        constexpr std::uint32_t arch_winnt_64 = 40;     // Windows NT 64-bit
        constexpr std::uint32_t arch_darwin_x64 = 41;   // Darwin x64
        constexpr std::uint32_t arch_darwin_ppc64 = 42; // Darwin PowerPC 64-bit
        constexpr std::uint32_t arch_arm = 43;          // ARM architecture
        constexpr std::uint32_t arch_winnt_arm64 = 44;  // Windows NT ARM64
        constexpr std::uint32_t arch_max = 45;          // Keep this at the end

        // Protocol types (from protocol.h)
        constexpr std::uint32_t ptype_batch_send = 3;   // Batch sends, no asynchrony
        constexpr std::uint32_t ptype_out_of_band = 4;  // Batch sends w/ out of band notification
        constexpr std::uint32_t ptype_lazy_send = 5;    // Deferred packets delivery
        constexpr std::uint32_t ptype_mask = 0xFF;      // Mask - up to 255 types of protocol
        constexpr std::uint32_t pflag_compress = 0x100; // Turn on compression if possible
        constexpr std::uint32_t pflag_win_sspi_nego =
            0x200; // Win_SSPI supports Negotiate security package

        // Character sets
        constexpr std::uint32_t charset_none = 0;
        constexpr std::uint32_t charset_utf8 = 4;
        constexpr std::uint32_t charset_unicode_fss = 3;
    } // namespace FirebirdProtocol

    /// Firebird protocol version information
    struct FirebirdProtocolVersion {
        std::uint16_t protocol_version = 0;
        std::uint32_t architecture = 0;
        std::uint32_t min_type = 0;
        std::uint32_t max_type = 0;
        std::uint32_t priority = 0;

        bool is_compatible_with(const FirebirdProtocolVersion& other) const;
        std::string to_string() const;
    };

    /// Firebird protocol capabilities
    struct FirebirdCapabilities {
        bool supports_lazy_send = false;
        bool supports_batch_send = false;
        bool supports_out_of_band = false;
        bool supports_compression = false;
        bool supports_encryption = false;
        bool supports_multi_hop = false;
        bool supports_cancel_operation = false;
        bool supports_partial_batch = false;
        std::uint32_t max_packet_size = 8192;
        std::uint32_t buffer_length = 1024;

        static FirebirdCapabilities get_capabilities_for_version(std::uint16_t protocol_version);
    };

    /// Firebird connection parameters
    struct FirebirdConnectionParams {
        std::string database_path;
        std::string username;
        std::string password;
        std::string role;
        std::string charset = "UTF8";
        std::uint32_t page_size = 4096;
        bool create_database = false;
        std::map<std::uint32_t, std::string> additional_params;

        std::vector<std::uint8_t> encode_dpb() const;
        bool decode_dpb(const std::vector<std::uint8_t>& dpb);
    };

    /// Firebird protocol message types
    class FirebirdMessage : public ProtocolMessage
    {
      public:
        FirebirdMessage(std::uint32_t operation);

        std::unique_ptr<ProtocolMessage> clone() const override;

        std::uint32_t get_operation() const
        {
            return operation_;
        }
        void set_operation(std::uint32_t op)
        {
            operation_ = op;
        }

        // Parameter management
        void add_parameter(const std::string& value);
        void add_parameter(std::uint32_t value);
        void add_parameter(std::int32_t value);
        void add_parameter(const std::vector<std::uint8_t>& value);

        std::size_t get_parameter_count() const
        {
            return parameters_.size();
        }
        const std::vector<std::uint8_t>& get_parameter_data() const
        {
            return parameters_;
        }

        // Serialization
        std::vector<std::uint8_t> serialize() const;
        bool deserialize(const std::vector<std::uint8_t>& data);

      private:
        std::uint32_t operation_;
        std::vector<std::uint8_t> parameters_;

        void encode_string(const std::string& str);
        void encode_uint32(std::uint32_t value);
        void encode_int32(std::int32_t value);
    };

    /// Firebird protocol message framer
    class FirebirdMessageFramer : public MessageFramer
    {
      public:
        FirebirdMessageFramer();

        std::vector<std::vector<std::uint8_t>>
        frame_messages(const std::vector<std::uint8_t>& data) override;
        bool needs_more_data() const override;
        void reset() override;

        void set_endianness(bool big_endian)
        {
            big_endian_ = big_endian;
        }

      private:
        enum class FrameState { ReadingHeader, ReadingOperation, ReadingData };

        FrameState frame_state_;
        std::uint32_t expected_data_size_;
        std::uint32_t bytes_read_;
        bool big_endian_;

        std::uint32_t read_uint32(const std::uint8_t* data) const;
        void write_uint32(std::uint8_t* data, std::uint32_t value) const;
        bool parse_header(const std::vector<std::uint8_t>& data, std::size_t& offset);
    };

    /// Firebird protocol state machine
    class FirebirdStateMachine : public ProtocolStateMachine
    {
      public:
        FirebirdStateMachine();

        std::string get_current_state() const override;
        bool transition_to_state(const std::string& new_state) override;
        bool is_valid_transition(const std::string& from_state,
                                 const std::string& to_state) const override;
        std::vector<std::string> get_valid_next_states() const override;

        // Firebird-specific state queries
        bool is_connected() const;
        bool is_attached() const;
        bool has_active_transaction() const;

      private:
        void initialize_state_transitions();
        std::map<std::string, std::vector<std::string>> valid_transitions_;

        // Firebird connection state
        bool database_attached_;
        bool transaction_active_;
    };

    /// Firebird protocol version negotiator
    class FirebirdVersionNegotiator
    {
      public:
        FirebirdVersionNegotiator();

        // Version negotiation
        void add_supported_version(const FirebirdProtocolVersion& version);
        FirebirdProtocolVersion
        negotiate_version(const std::vector<FirebirdProtocolVersion>& client_versions);
        bool is_version_supported(std::uint16_t protocol_version) const;

        // Capability negotiation
        FirebirdCapabilities get_capabilities(std::uint16_t protocol_version) const;
        bool negotiate_capabilities(std::uint16_t protocol_version,
                                    FirebirdCapabilities& negotiated_caps) const;

        // Backward compatibility
        std::vector<std::uint16_t> get_compatible_versions(std::uint16_t target_version) const;
        bool is_backward_compatible(std::uint16_t newer_version, std::uint16_t older_version) const;

      private:
        std::vector<FirebirdProtocolVersion> supported_versions_;
        std::map<std::uint16_t, FirebirdCapabilities> version_capabilities_;

        void initialize_default_versions();
        void initialize_version_capabilities();
    };

    /// Firebird wire protocol handler
    class FirebirdProtocolHandler : public ProtocolHandler
    {
      public:
        FirebirdProtocolHandler();
        ~FirebirdProtocolHandler() override;

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

        /// Firebird-specific operations
        bool handle_connect_request(const FirebirdMessage& message);
        bool handle_attach_request(const FirebirdMessage& message);
        bool handle_detach_request(const FirebirdMessage& message);
        bool handle_transaction_request(const FirebirdMessage& message);
        bool handle_statement_request(const FirebirdMessage& message);

      private:
        TcpConnection* connection_;
        CatalogManager* catalog_;

        std::unique_ptr<FirebirdStateMachine> state_machine_;
        std::unique_ptr<FirebirdMessageFramer> message_framer_;
        std::unique_ptr<FirebirdVersionNegotiator> version_negotiator_;

        MessageQueue outgoing_queue_;
        CorrelationTracker correlation_tracker_;

        FirebirdProtocolVersion negotiated_version_;
        FirebirdCapabilities negotiated_capabilities_;
        FirebirdConnectionParams connection_params_;

        std::atomic<bool> initialized_;
        std::atomic<bool> authenticated_;
        std::string last_error_;
        mutable std::mutex handler_mutex_;

        // Database handles
        std::uint32_t next_handle_id_;
        std::map<std::uint32_t, std::string> database_handles_;
        std::map<std::uint32_t, std::uint32_t> transaction_handles_;
        std::map<std::uint32_t, std::string> statement_handles_;

        // Message handlers
        ProtocolResult handle_connect_message(const FirebirdMessage& message);
        ProtocolResult handle_attach_message(const FirebirdMessage& message);
        ProtocolResult handle_detach_message(const FirebirdMessage& message);
        ProtocolResult handle_transaction_message(const FirebirdMessage& message);
        ProtocolResult handle_statement_message(const FirebirdMessage& message);
        ProtocolResult handle_response_message(const FirebirdMessage& message);

        // Response generation
        void send_accept_response(const FirebirdProtocolVersion& version);
        void send_reject_response(const std::string& reason);
        void send_response(std::uint32_t response_data, std::uint64_t correlation_id = 0);
        void send_error_response(std::int32_t error_code, const std::string& error_message,
                                 std::uint64_t correlation_id = 0);

        // Helper methods
        std::uint32_t allocate_handle();
        bool validate_database_path(const std::string& path) const;
        bool authenticate_user(const std::string& username, const std::string& password) const;
        std::uint64_t generate_message_id();

        std::atomic<std::uint64_t> next_message_id_;
    };

} // namespace scratchbird::engine
