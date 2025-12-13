#pragma once

/**
 * ScratchBird Multi-Factor Authentication (MFA)
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 *
 * Implements MFA framework with:
 * - TOTP (RFC 6238)
 * - HOTP (RFC 4226)
 * - Backup codes
 * - WebAuthn/FIDO2 (placeholder)
 * - Push notifications (placeholder)
 * - SMS/Email OTP (placeholder)
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <optional>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"

namespace scratchbird {
namespace security {

// ============================================================================
// MFA Types
// ============================================================================

/**
 * MFA method type
 */
enum class MfaMethodType : uint8_t {
    TOTP = 0,           // Time-based OTP (Google Authenticator, etc.)
    HOTP = 1,           // HMAC-based OTP (YubiKey HOTP)
    BACKUP_CODES = 2,   // One-time recovery codes
    WEBAUTHN = 3,       // FIDO2/WebAuthn (hardware keys)
    PUSH = 4,           // Push notification
    SMS = 5,            // SMS OTP
    EMAIL = 6,          // Email OTP
    HARDWARE_TOKEN = 7  // Hardware token (RSA SecurID, etc.)
};

/**
 * MFA hash algorithm
 */
enum class MfaHashAlgorithm : uint8_t {
    SHA1 = 0,           // RFC 4226 default
    SHA256 = 1,
    SHA512 = 2
};

/**
 * TOTP/HOTP configuration
 */
struct OtpConfig {
    MfaHashAlgorithm algorithm = MfaHashAlgorithm::SHA1;
    uint8_t digits = 6;             // OTP length (6 or 8)
    uint32_t period = 30;           // TOTP time step in seconds
    uint32_t look_ahead = 1;        // Number of periods to look ahead
    uint32_t look_behind = 1;       // Number of periods to look behind
};

/**
 * MFA method configuration
 */
struct MfaMethodConfig {
    MfaMethodType type;
    std::string name;               // Display name
    bool enabled = true;
    bool is_primary = false;        // Primary method

    // TOTP/HOTP specific
    std::vector<uint8_t> secret;    // Base32-decoded secret
    OtpConfig otp_config;
    uint64_t hotp_counter = 0;      // HOTP counter

    // Backup codes specific
    std::vector<std::string> codes; // Hashed backup codes
    uint32_t codes_remaining = 0;

    // WebAuthn specific
    std::string credential_id;
    std::vector<uint8_t> public_key;
    std::string attestation_type;

    // Timestamps
    std::chrono::system_clock::time_point registered;
    std::chrono::system_clock::time_point last_used;
};

/**
 * User MFA configuration
 */
struct UserMfaConfig {
    std::string username;
    bool mfa_enabled = false;
    bool mfa_enforced = false;      // Require MFA even if not enrolled

    // Registered methods
    std::vector<MfaMethodConfig> methods;

    // Grace period (allow login without MFA temporarily)
    std::chrono::system_clock::time_point grace_period_until;

    // Recovery
    std::string recovery_email;
    std::string recovery_phone;

    /**
     * Get primary MFA method
     */
    const MfaMethodConfig* getPrimaryMethod() const;

    /**
     * Check if user has any enabled MFA methods
     */
    bool hasEnabledMethod() const;

    /**
     * Get enabled methods of specific type
     */
    std::vector<const MfaMethodConfig*> getMethodsByType(MfaMethodType type) const;
};

/**
 * MFA challenge
 */
struct MfaChallenge {
    std::string challenge_id;
    std::string username;
    MfaMethodType method_type;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point expires;

    // Method-specific challenge data
    std::string challenge_data;     // Nonce for WebAuthn, etc.

    // Attempt tracking
    uint32_t attempts = 0;
    uint32_t max_attempts = 3;
};

/**
 * MFA verification result
 */
struct MfaVerifyResult {
    bool success = false;
    std::string error_message;
    MfaMethodType method_used;
    bool backup_code_used = false;

    // For HOTP - new counter value
    std::optional<uint64_t> new_counter;

    // For backup codes - code was consumed
    bool code_consumed = false;
};

// ============================================================================
// TOTP Implementation (RFC 6238)
// ============================================================================

/**
 * Generate TOTP secret key
 */
std::vector<uint8_t> generateTotpSecret(size_t bytes = 20);

/**
 * Encode secret as base32 for QR codes
 */
std::string encodeBase32(const std::vector<uint8_t>& data);

/**
 * Decode base32 secret
 */
std::vector<uint8_t> decodeBase32(const std::string& encoded);

/**
 * Generate TOTP code
 */
std::string generateTotp(const std::vector<uint8_t>& secret,
                         const OtpConfig& config = OtpConfig{},
                         std::chrono::system_clock::time_point time =
                             std::chrono::system_clock::now());

/**
 * Verify TOTP code
 */
bool verifyTotp(const std::vector<uint8_t>& secret,
                const std::string& code,
                const OtpConfig& config = OtpConfig{},
                std::chrono::system_clock::time_point time =
                    std::chrono::system_clock::now());

/**
 * Generate TOTP provisioning URI for QR code
 */
std::string generateTotpUri(const std::string& issuer,
                            const std::string& account,
                            const std::vector<uint8_t>& secret,
                            const OtpConfig& config = OtpConfig{});

// ============================================================================
// HOTP Implementation (RFC 4226)
// ============================================================================

/**
 * Generate HOTP code
 */
std::string generateHotp(const std::vector<uint8_t>& secret,
                         uint64_t counter,
                         const OtpConfig& config = OtpConfig{});

/**
 * Verify HOTP code (with look-ahead window)
 */
bool verifyHotp(const std::vector<uint8_t>& secret,
                const std::string& code,
                uint64_t& counter,
                uint32_t look_ahead = 10,
                const OtpConfig& config = OtpConfig{});

// ============================================================================
// Backup Codes
// ============================================================================

/**
 * Generate backup codes
 */
std::vector<std::string> generateBackupCodes(size_t count = 10,
                                              size_t length = 8);

/**
 * Hash backup code for storage
 */
std::string hashBackupCode(const std::string& code);

/**
 * Verify backup code
 */
bool verifyBackupCode(const std::string& code,
                      const std::string& hashed_code);

// ============================================================================
// MFA Manager
// ============================================================================

/**
 * MFA Manager
 *
 * Coordinates MFA enrollment, verification, and management.
 */
class MfaManager {
public:
    MfaManager();
    ~MfaManager();

    /**
     * Initialize MFA manager
     */
    core::Status initialize(const std::map<std::string, std::string>& config,
                            core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Enrollment
    // ========================================================================

    /**
     * Begin TOTP enrollment
     *
     * Returns: Secret (base32), URI for QR code
     */
    core::Status beginTotpEnrollment(const std::string& username,
                                     std::string& secret_base32,
                                     std::string& qr_uri,
                                     core::ErrorContext* ctx = nullptr);

    /**
     * Complete TOTP enrollment (verify first code)
     */
    core::Status completeTotpEnrollment(const std::string& username,
                                        const std::string& secret_base32,
                                        const std::string& verification_code,
                                        core::ErrorContext* ctx = nullptr);

    /**
     * Generate backup codes for user
     */
    core::Status generateBackupCodes(const std::string& username,
                                     std::vector<std::string>& codes,
                                     core::ErrorContext* ctx = nullptr);

    /**
     * Remove MFA method
     */
    core::Status removeMfaMethod(const std::string& username,
                                 MfaMethodType method_type,
                                 core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Verification
    // ========================================================================

    /**
     * Create MFA challenge
     */
    core::Status createChallenge(const std::string& username,
                                 MfaChallenge& challenge,
                                 core::ErrorContext* ctx = nullptr);

    /**
     * Verify MFA code
     */
    MfaVerifyResult verify(const std::string& username,
                           const std::string& code,
                           const std::string& challenge_id = "");

    /**
     * Check if user requires MFA
     */
    bool requiresMfa(const std::string& username);

    /**
     * Check if user has MFA configured
     */
    bool hasMfaConfigured(const std::string& username);

    // ========================================================================
    // User Management
    // ========================================================================

    /**
     * Get user MFA configuration
     */
    core::Status getUserMfaConfig(const std::string& username,
                                  UserMfaConfig& config,
                                  core::ErrorContext* ctx = nullptr);

    /**
     * Enable MFA requirement for user
     */
    core::Status enableMfaRequirement(const std::string& username,
                                      core::ErrorContext* ctx = nullptr);

    /**
     * Disable MFA requirement for user
     */
    core::Status disableMfaRequirement(const std::string& username,
                                       core::ErrorContext* ctx = nullptr);

    /**
     * Set grace period (temporary MFA bypass)
     */
    core::Status setGracePeriod(const std::string& username,
                                std::chrono::hours duration,
                                core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * MFA global configuration
     */
    struct Config {
        // TOTP settings
        OtpConfig totp_config;
        std::string issuer_name;        // App name for QR codes

        // Backup codes
        size_t backup_code_count = 10;
        size_t backup_code_length = 8;

        // Policy
        bool enforce_mfa_for_all = false;
        std::vector<std::string> mfa_exempt_roles;
        uint32_t max_failed_attempts = 3;
        std::chrono::seconds challenge_validity{300};

        // Rate limiting
        std::chrono::seconds rate_limit_window{60};
        uint32_t rate_limit_attempts = 5;
    };

    void setConfig(const Config& config);
    const Config& config() const { return config_; }

private:
    /**
     * Store user MFA configuration
     */
    core::Status storeUserMfaConfig(const UserMfaConfig& config);

    /**
     * Load user MFA configuration
     */
    core::Status loadUserMfaConfig(const std::string& username,
                                   UserMfaConfig& config);

    Config config_;

    // User configurations (should be backed by database in production)
    std::map<std::string, UserMfaConfig> user_configs_;
    std::mutex users_mutex_;

    // Active challenges
    std::map<std::string, MfaChallenge> challenges_;
    std::mutex challenges_mutex_;
};

// ============================================================================
// MFA Authentication Wrapper
// ============================================================================

/**
 * MFA Authentication Method Wrapper
 *
 * Wraps primary authentication with MFA verification.
 */
class MfaAuthMethod : public AuthMethod {
public:
    MfaAuthMethod(std::shared_ptr<AuthMethod> primary_method,
                  std::shared_ptr<MfaManager> mfa_manager);
    ~MfaAuthMethod();

    AuthType type() const override { return primary_method_->type(); }
    const char* name() const override { return "mfa"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                            core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                            const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    bool supportsPasswordVerification() const override {
        return primary_method_->supportsPasswordVerification();
    }

    bool verifyPassword(const std::string& username,
                        const std::string& password) override {
        return primary_method_->verifyPassword(username, password);
    }

private:
    enum class MfaAuthState : uint8_t {
        PRIMARY_AUTH = 0,
        MFA_REQUIRED = 1,
        MFA_VERIFY = 2,
        COMPLETE = 3
    };

    struct SessionState {
        MfaAuthState state = MfaAuthState::PRIMARY_AUTH;
        std::string username;
        MfaChallenge challenge;
    };

    AuthResult processMfaVerification(AuthContext& ctx,
                                      const std::vector<uint8_t>& data);

    std::shared_ptr<AuthMethod> primary_method_;
    std::shared_ptr<MfaManager> mfa_manager_;

    std::map<AuthContext*, SessionState> sessions_;
    std::mutex sessions_mutex_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get MFA method type name
 */
const char* mfaMethodTypeToString(MfaMethodType type);

/**
 * Parse MFA method type from string
 */
bool parseMfaMethodType(const std::string& str, MfaMethodType& type);

/**
 * Secure compare strings (constant time)
 */
bool secureCompare(const std::string& a, const std::string& b);

/**
 * Generate cryptographically secure random bytes
 */
std::vector<uint8_t> generateSecureRandom(size_t bytes);

/**
 * Generate human-readable random string
 */
std::string generateReadableCode(size_t length,
                                 bool uppercase = true,
                                 bool digits = true);

}  // namespace security
}  // namespace scratchbird
