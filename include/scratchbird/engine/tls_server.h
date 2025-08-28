#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace ScratchBird
{

    /**
     * TLS version enum
     */
    enum class TLSVersion {
        TLS_1_2,
        TLS_1_3,
        Auto // Negotiate highest available
    };

    /**
     * TLS certificate verification mode
     */
    enum class TLSVerificationMode {
        None,     // No verification
        Optional, // Verify if certificate provided
        Required  // Require and verify certificate
    };

    /**
     * TLS certificate chain information
     */
    struct TLSCertificateInfo {
        std::string subject;
        std::string issuer;
        std::string serial_number;
        std::chrono::system_clock::time_point not_before;
        std::chrono::system_clock::time_point not_after;
        std::vector<std::string> subject_alt_names;
        std::string fingerprint_sha256;

        bool is_valid() const;
        bool is_expired() const;
        bool is_self_signed() const;
    };

    /**
     * TLS configuration
     */
    struct TLSConfiguration {
        // Protocol versions
        TLSVersion min_version = TLSVersion::TLS_1_2;
        TLSVersion max_version = TLSVersion::TLS_1_3;

        // Certificate and key files
        std::string certificate_file;
        std::string private_key_file;
        std::string ca_certificate_file;
        std::string crl_file; // Certificate Revocation List

        // Client certificate verification
        TLSVerificationMode client_verification = TLSVerificationMode::None;
        bool require_client_certificate = false;

        // Cipher suites and security
        std::vector<std::string> allowed_cipher_suites;
        std::vector<std::string> allowed_curves;
        bool disable_compression = true; // Prevent CRIME/BREACH attacks
        bool enable_session_tickets = false;

        // Perfect Forward Secrecy
        bool require_perfect_forward_secrecy = true;

        // OCSP (Online Certificate Status Protocol)
        bool enable_ocsp_stapling = false;
        std::string ocsp_responder_url;

        // Session management
        std::chrono::minutes session_timeout{60};
        bool enable_session_resumption = true;

        // Security policies
        bool enforce_server_cipher_order = true;
        bool disable_renegotiation = true;

        // Validation
        bool validate() const;
        std::vector<std::string> get_validation_errors() const;
    };

    /**
     * TLS session information
     */
    class TLSSession
    {
      public:
        TLSSession(SSL* ssl, const std::string& remote_address);
        ~TLSSession();

        // Session information
        const std::string& get_remote_address() const
        {
            return remote_address_;
        }
        std::string get_cipher_suite() const;
        std::string get_protocol_version() const;

        // Certificate information
        bool has_client_certificate() const;
        TLSCertificateInfo get_client_certificate_info() const;
        bool verify_client_certificate() const;

        // Security properties
        bool has_perfect_forward_secrecy() const;
        std::string get_key_exchange_algorithm() const;
        int get_key_bits() const;

        // Session management
        bool is_resumed_session() const;
        std::chrono::system_clock::time_point get_established_time() const;
        std::chrono::system_clock::time_point get_last_activity() const;

        void update_activity();
        bool is_expired(std::chrono::minutes timeout) const;

        // Allow TLSServer access to internal SSL pointer
        friend class TLSServer;

      private:
        SSL* ssl_;
        std::string remote_address_;
        std::chrono::system_clock::time_point established_time_;
        std::chrono::system_clock::time_point last_activity_;
    };

    /**
     * TLS context manager for OpenSSL integration
     */
    class TLSContext
    {
      public:
        TLSContext();
        ~TLSContext();

        // Initialization
        bool initialize(const TLSConfiguration& config);
        void shutdown();
        bool is_initialized() const
        {
            return ssl_ctx_ != nullptr;
        }

        // SSL context access
        SSL_CTX* get_ssl_context() const
        {
            return ssl_ctx_;
        }

        // Certificate management
        bool load_certificate_chain(const std::string& cert_file, const std::string& key_file);
        bool load_ca_certificates(const std::string& ca_file);
        bool load_crl(const std::string& crl_file);
        bool verify_certificate_chain() const;

        // Configuration
        void set_cipher_suites(const std::vector<std::string>& cipher_suites);
        void set_curves(const std::vector<std::string>& curves);
        void set_verification_mode(TLSVerificationMode mode);
        void enable_session_cache(bool enable);

        // Security features
        void configure_security_options();
        void setup_dh_parameters();
        void setup_ecdh_curves();

        // Certificate validation callbacks
        static int verify_certificate_callback(int preverify_ok, X509_STORE_CTX* ctx);
        static int select_certificate_callback(SSL* ssl, void* arg);

        // Session management
        static int new_session_callback(SSL* ssl, SSL_SESSION* session);
        static void remove_session_callback(SSL_CTX* ctx, SSL_SESSION* session);

      private:
        SSL_CTX* ssl_ctx_;
        TLSConfiguration config_;
        mutable std::mutex context_mutex_;

        // Internal setup methods
        bool setup_protocol_versions();
        bool setup_cipher_configuration();
        bool setup_certificate_verification();
        bool setup_session_management();
        void setup_security_callbacks();

        // Certificate utilities
        std::string extract_certificate_info(X509* cert) const;
        bool check_certificate_revocation(X509* cert) const;
    };

    /**
     * TLS server implementation
     */
    class TLSServer
    {
      public:
        TLSServer();
        ~TLSServer();

        // Server lifecycle
        bool initialize(const TLSConfiguration& config);
        void shutdown();
        bool is_running() const
        {
            return is_running_;
        }

        // Connection handling
        using ConnectionHandler = std::function<void(std::unique_ptr<TLSSession>)>;
        void set_connection_handler(ConnectionHandler handler)
        {
            connection_handler_ = handler;
        }

        bool start_listening(const std::string& bind_address, std::uint16_t port);
        void stop_listening();

        // TLS operations
        std::unique_ptr<TLSSession> accept_tls_connection(int socket_fd,
                                                          const std::string& remote_address);
        bool perform_tls_handshake(TLSSession& session);

        // Session management
        void register_session(std::shared_ptr<TLSSession> session);
        void unregister_session(const std::string& session_id);
        void cleanup_expired_sessions();

        std::vector<std::shared_ptr<TLSSession>> get_active_sessions() const;
        std::size_t get_active_session_count() const;

        // Statistics and monitoring
        struct TLSStatistics {
            std::uint64_t total_connections = 0;
            std::uint64_t successful_handshakes = 0;
            std::uint64_t failed_handshakes = 0;
            std::uint64_t certificate_errors = 0;
            std::uint64_t protocol_errors = 0;
            std::uint64_t active_sessions = 0;
            std::chrono::system_clock::time_point last_reset;
        };

        TLSStatistics get_statistics() const;
        void reset_statistics();

        // Configuration management
        const TLSConfiguration& get_configuration() const
        {
            return config_;
        }
        bool update_configuration(const TLSConfiguration& new_config);
        bool reload_certificates();

        // Security audit
        struct SecurityAuditEvent {
            enum Type {
                HandshakeStart,
                HandshakeSuccess,
                HandshakeFailure,
                CertificateError,
                ProtocolError,
                SessionEstablished,
                SessionClosed,
                ConfigurationChanged
            };

            Type type;
            std::string remote_address;
            std::string details;
            std::chrono::system_clock::time_point timestamp;
        };

        using AuditCallback = std::function<void(const SecurityAuditEvent&)>;
        void set_audit_callback(AuditCallback callback)
        {
            audit_callback_ = callback;
        }

      private:
        // Core components
        std::unique_ptr<TLSContext> tls_context_;
        TLSConfiguration config_;

        // Server state
        bool is_running_;
        std::atomic<bool> shutdown_requested_;

        // Connection management
        ConnectionHandler connection_handler_;
        std::map<std::string, std::weak_ptr<TLSSession>> active_sessions_;
        mutable std::shared_mutex sessions_mutex_;

        // Statistics
        mutable TLSStatistics statistics_;
        mutable std::mutex stats_mutex_;

        // Audit logging
        AuditCallback audit_callback_;

        // Internal methods
        void log_audit_event(SecurityAuditEvent::Type type, const std::string& remote_address,
                             const std::string& details = "");

        std::string generate_session_id() const;
        void handle_ssl_error(const std::string& operation, int error_code);

        // Session cleanup thread
        std::thread cleanup_thread_;
        void session_cleanup_worker();

        mutable std::mutex server_mutex_;
    };

    /**
     * TLS certificate manager for certificate lifecycle management
     */
    class TLSCertificateManager
    {
      public:
        TLSCertificateManager();
        ~TLSCertificateManager();

        // Certificate loading and validation
        bool load_certificate_from_file(const std::string& cert_file);
        bool load_private_key_from_file(const std::string& key_file);
        bool load_ca_bundle(const std::string& ca_bundle_file);

        // Certificate generation (for testing/development)
        bool generate_self_signed_certificate(const std::string& common_name,
                                              const std::vector<std::string>& alt_names,
                                              int validity_days = 365);

        // Certificate validation
        bool validate_certificate_chain() const;
        bool check_certificate_expiry(std::chrono::hours warning_threshold = std::chrono::hours{
                                          720}) const;
        bool verify_private_key_match() const;

        // Certificate information
        TLSCertificateInfo get_certificate_info() const;
        std::vector<TLSCertificateInfo> get_certificate_chain_info() const;

        // Certificate storage
        bool save_certificate_to_file(const std::string& cert_file) const;
        bool save_private_key_to_file(const std::string& key_file) const;

        // OCSP support
        bool check_ocsp_status(const std::string& ocsp_url = "") const;
        bool staple_ocsp_response();

      private:
        X509* certificate_;
        EVP_PKEY* private_key_;
        STACK_OF(X509) * certificate_chain_;
        X509_STORE* ca_store_;

        mutable std::mutex cert_mutex_;

        // Internal utilities
        std::string get_certificate_fingerprint(X509* cert) const;
        bool verify_certificate_signature(X509* cert, X509* issuer) const;
        std::vector<std::string> extract_alt_names(X509* cert) const;
    };

    // Utility functions
    std::string tls_version_to_string(TLSVersion version);
    TLSVersion parse_tls_version(const std::string& version_str);
    std::string verification_mode_to_string(TLSVerificationMode mode);
    TLSVerificationMode parse_verification_mode(const std::string& mode_str);

    // OpenSSL initialization and cleanup
    void initialize_openssl();
    void cleanup_openssl();

} // namespace ScratchBird
