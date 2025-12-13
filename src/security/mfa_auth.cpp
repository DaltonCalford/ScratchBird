/**
 * ScratchBird Multi-Factor Authentication (MFA) Implementation
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 */

#include "scratchbird/security/mfa_auth.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <random>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace scratchbird {
namespace security {

// ============================================================================
// TOTP Implementation (RFC 6238)
// ============================================================================

std::vector<uint8_t> generateTotpSecret(size_t bytes) {
    std::vector<uint8_t> secret(bytes);
    RAND_bytes(secret.data(), secret.size());
    return secret;
}

std::string encodeBase32(const std::vector<uint8_t>& data) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    std::string encoded;
    encoded.reserve((data.size() * 8 + 4) / 5);

    uint64_t buffer = 0;
    int bits = 0;

    for (uint8_t byte : data) {
        buffer = (buffer << 8) | byte;
        bits += 8;

        while (bits >= 5) {
            bits -= 5;
            encoded += alphabet[(buffer >> bits) & 0x1f];
        }
    }

    if (bits > 0) {
        encoded += alphabet[(buffer << (5 - bits)) & 0x1f];
    }

    // Add padding
    while (encoded.size() % 8 != 0) {
        encoded += '=';
    }

    return encoded;
}

std::vector<uint8_t> decodeBase32(const std::string& encoded) {
    static const int8_t decode_table[128] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1
    };

    std::vector<uint8_t> decoded;
    decoded.reserve(encoded.size() * 5 / 8);

    uint64_t buffer = 0;
    int bits = 0;

    for (char c : encoded) {
        if (c == '=' || c == ' ') continue;
        if (c < 0 || c >= 128) continue;

        int8_t value = decode_table[static_cast<unsigned char>(c)];
        if (value < 0) continue;

        buffer = (buffer << 5) | value;
        bits += 5;

        if (bits >= 8) {
            bits -= 8;
            decoded.push_back((buffer >> bits) & 0xff);
        }
    }

    return decoded;
}

static std::vector<uint8_t> hmacSha(const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& message,
                                    MfaHashAlgorithm algorithm) {
    const EVP_MD* md = nullptr;
    switch (algorithm) {
        case MfaHashAlgorithm::SHA1:   md = EVP_sha1(); break;
        case MfaHashAlgorithm::SHA256: md = EVP_sha256(); break;
        case MfaHashAlgorithm::SHA512: md = EVP_sha512(); break;
    }

    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    HMAC(md, key.data(), key.size(),
         message.data(), message.size(),
         result, &result_len);

    return std::vector<uint8_t>(result, result + result_len);
}

static uint32_t truncateOtp(const std::vector<uint8_t>& hmac_result, uint8_t digits) {
    // Dynamic truncation per RFC 4226
    int offset = hmac_result[hmac_result.size() - 1] & 0x0f;

    uint32_t binary_code =
        ((hmac_result[offset] & 0x7f) << 24) |
        ((hmac_result[offset + 1] & 0xff) << 16) |
        ((hmac_result[offset + 2] & 0xff) << 8) |
        (hmac_result[offset + 3] & 0xff);

    // Get last 'digits' digits
    uint32_t modulo = 1;
    for (uint8_t i = 0; i < digits; ++i) {
        modulo *= 10;
    }

    return binary_code % modulo;
}

std::string generateTotp(const std::vector<uint8_t>& secret,
                         const OtpConfig& config,
                         std::chrono::system_clock::time_point time) {
    // Calculate time counter
    auto epoch = time.time_since_epoch();
    uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    uint64_t counter = seconds / config.period;

    // Generate HOTP with time counter
    return generateHotp(secret, counter, config);
}

bool verifyTotp(const std::vector<uint8_t>& secret,
                const std::string& code,
                const OtpConfig& config,
                std::chrono::system_clock::time_point time) {
    // Check current time step and adjacent ones
    auto epoch = time.time_since_epoch();
    uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    uint64_t counter = seconds / config.period;

    for (int32_t i = -static_cast<int32_t>(config.look_behind);
         i <= static_cast<int32_t>(config.look_ahead); ++i) {
        std::string expected = generateHotp(secret, counter + i, config);
        if (secureCompare(code, expected)) {
            return true;
        }
    }

    return false;
}

std::string generateTotpUri(const std::string& issuer,
                            const std::string& account,
                            const std::vector<uint8_t>& secret,
                            const OtpConfig& config) {
    std::ostringstream uri;
    uri << "otpauth://totp/";

    // URL encode issuer and account
    if (!issuer.empty()) {
        uri << issuer << ":";
    }
    uri << account;

    uri << "?secret=" << encodeBase32(secret);

    if (!issuer.empty()) {
        uri << "&issuer=" << issuer;
    }

    if (config.algorithm != MfaHashAlgorithm::SHA1) {
        uri << "&algorithm=";
        switch (config.algorithm) {
            case MfaHashAlgorithm::SHA256: uri << "SHA256"; break;
            case MfaHashAlgorithm::SHA512: uri << "SHA512"; break;
            default: uri << "SHA1"; break;
        }
    }

    if (config.digits != 6) {
        uri << "&digits=" << static_cast<int>(config.digits);
    }

    if (config.period != 30) {
        uri << "&period=" << config.period;
    }

    return uri.str();
}

// ============================================================================
// HOTP Implementation (RFC 4226)
// ============================================================================

std::string generateHotp(const std::vector<uint8_t>& secret,
                         uint64_t counter,
                         const OtpConfig& config) {
    // Convert counter to big-endian 8 bytes
    std::vector<uint8_t> counter_bytes(8);
    for (int i = 7; i >= 0; --i) {
        counter_bytes[i] = counter & 0xff;
        counter >>= 8;
    }

    // HMAC-SHA
    auto hmac_result = hmacSha(secret, counter_bytes, config.algorithm);

    // Truncate to OTP
    uint32_t otp = truncateOtp(hmac_result, config.digits);

    // Format with leading zeros
    std::ostringstream oss;
    oss << std::setw(config.digits) << std::setfill('0') << otp;

    return oss.str();
}

bool verifyHotp(const std::vector<uint8_t>& secret,
                const std::string& code,
                uint64_t& counter,
                uint32_t look_ahead,
                const OtpConfig& config) {
    for (uint32_t i = 0; i <= look_ahead; ++i) {
        std::string expected = generateHotp(secret, counter + i, config);
        if (secureCompare(code, expected)) {
            counter = counter + i + 1;  // Advance counter
            return true;
        }
    }

    return false;
}

// ============================================================================
// Backup Codes
// ============================================================================

std::vector<std::string> generateBackupCodes(size_t count, size_t length) {
    std::vector<std::string> codes;
    codes.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        codes.push_back(generateReadableCode(length, true, true));
    }

    return codes;
}

std::string hashBackupCode(const std::string& code) {
    // Use SHA256 for backup code hashing
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(code.data()),
           code.size(), hash);

    std::ostringstream oss;
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }

    return oss.str();
}

bool verifyBackupCode(const std::string& code,
                      const std::string& hashed_code) {
    std::string computed_hash = hashBackupCode(code);
    return secureCompare(computed_hash, hashed_code);
}

// ============================================================================
// UserMfaConfig Implementation
// ============================================================================

const MfaMethodConfig* UserMfaConfig::getPrimaryMethod() const {
    for (const auto& method : methods) {
        if (method.is_primary && method.enabled) {
            return &method;
        }
    }

    // Return first enabled method if no primary
    for (const auto& method : methods) {
        if (method.enabled) {
            return &method;
        }
    }

    return nullptr;
}

bool UserMfaConfig::hasEnabledMethod() const {
    for (const auto& method : methods) {
        if (method.enabled) {
            return true;
        }
    }
    return false;
}

std::vector<const MfaMethodConfig*> UserMfaConfig::getMethodsByType(MfaMethodType type) const {
    std::vector<const MfaMethodConfig*> result;
    for (const auto& method : methods) {
        if (method.type == type && method.enabled) {
            result.push_back(&method);
        }
    }
    return result;
}

// ============================================================================
// MfaManager Implementation
// ============================================================================

MfaManager::MfaManager() = default;
MfaManager::~MfaManager() = default;

core::Status MfaManager::initialize(const std::map<std::string, std::string>& config,
                                    core::ErrorContext* ctx) {
    auto it = config.find("issuer");
    if (it != config.end()) {
        config_.issuer_name = it->second;
    } else {
        config_.issuer_name = "ScratchBird";
    }

    it = config.find("totp_digits");
    if (it != config.end()) {
        config_.totp_config.digits = static_cast<uint8_t>(std::stoi(it->second));
    }

    it = config.find("totp_period");
    if (it != config.end()) {
        config_.totp_config.period = static_cast<uint32_t>(std::stoi(it->second));
    }

    it = config.find("enforce_mfa");
    if (it != config.end()) {
        config_.enforce_mfa_for_all = (it->second == "true" || it->second == "1");
    }

    it = config.find("backup_code_count");
    if (it != config.end()) {
        config_.backup_code_count = static_cast<size_t>(std::stoi(it->second));
    }

    return core::Status::OK;
}

core::Status MfaManager::beginTotpEnrollment(const std::string& username,
                                             std::string& secret_base32,
                                             std::string& qr_uri,
                                             core::ErrorContext* ctx) {
    // Generate new secret
    auto secret = generateTotpSecret(20);  // 160 bits
    secret_base32 = encodeBase32(secret);

    // Generate URI for QR code
    qr_uri = generateTotpUri(config_.issuer_name, username, secret, config_.totp_config);

    return core::Status::OK;
}

core::Status MfaManager::completeTotpEnrollment(const std::string& username,
                                                const std::string& secret_base32,
                                                const std::string& verification_code,
                                                core::ErrorContext* ctx) {
    // Decode secret
    auto secret = decodeBase32(secret_base32);
    if (secret.empty()) {
        if (ctx) ctx->message = "Invalid secret format";
        return core::Status::INVALID_ARGUMENT;
    }

    // Verify the code
    if (!verifyTotp(secret, verification_code, config_.totp_config)) {
        if (ctx) ctx->message = "Invalid verification code";
        return core::Status::PERMISSION_DENIED;
    }

    // Store MFA configuration
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto& user_config = user_configs_[username];
    user_config.username = username;
    user_config.mfa_enabled = true;

    MfaMethodConfig method;
    method.type = MfaMethodType::TOTP;
    method.name = "Authenticator App";
    method.enabled = true;
    method.is_primary = true;
    method.secret = secret;
    method.otp_config = config_.totp_config;
    method.registered = std::chrono::system_clock::now();

    // Remove any existing TOTP method
    user_config.methods.erase(
        std::remove_if(user_config.methods.begin(), user_config.methods.end(),
                       [](const MfaMethodConfig& m) { return m.type == MfaMethodType::TOTP; }),
        user_config.methods.end());

    user_config.methods.push_back(method);

    return storeUserMfaConfig(user_config);
}

core::Status MfaManager::generateBackupCodes(const std::string& username,
                                             std::vector<std::string>& codes,
                                             core::ErrorContext* ctx) {
    // Generate new backup codes
    codes = security::generateBackupCodes(config_.backup_code_count,
                                          config_.backup_code_length);

    // Hash codes for storage
    std::vector<std::string> hashed_codes;
    for (const auto& code : codes) {
        hashed_codes.push_back(hashBackupCode(code));
    }

    // Store configuration
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto& user_config = user_configs_[username];
    user_config.username = username;

    MfaMethodConfig method;
    method.type = MfaMethodType::BACKUP_CODES;
    method.name = "Backup Codes";
    method.enabled = true;
    method.is_primary = false;
    method.codes = hashed_codes;
    method.codes_remaining = hashed_codes.size();
    method.registered = std::chrono::system_clock::now();

    // Remove any existing backup codes
    user_config.methods.erase(
        std::remove_if(user_config.methods.begin(), user_config.methods.end(),
                       [](const MfaMethodConfig& m) { return m.type == MfaMethodType::BACKUP_CODES; }),
        user_config.methods.end());

    user_config.methods.push_back(method);

    return storeUserMfaConfig(user_config);
}

core::Status MfaManager::removeMfaMethod(const std::string& username,
                                         MfaMethodType method_type,
                                         core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it == user_configs_.end()) {
        if (ctx) ctx->message = "User not found";
        return core::Status::NOT_FOUND;
    }

    it->second.methods.erase(
        std::remove_if(it->second.methods.begin(), it->second.methods.end(),
                       [method_type](const MfaMethodConfig& m) { return m.type == method_type; }),
        it->second.methods.end());

    // Disable MFA if no methods left
    if (it->second.methods.empty()) {
        it->second.mfa_enabled = false;
    }

    return storeUserMfaConfig(it->second);
}

core::Status MfaManager::createChallenge(const std::string& username,
                                         MfaChallenge& challenge,
                                         core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it == user_configs_.end() || !it->second.hasEnabledMethod()) {
        if (ctx) ctx->message = "No MFA methods configured";
        return core::Status::NOT_FOUND;
    }

    const MfaMethodConfig* method = it->second.getPrimaryMethod();
    if (!method) {
        if (ctx) ctx->message = "No enabled MFA method";
        return core::Status::NOT_FOUND;
    }

    // Generate challenge ID
    std::vector<uint8_t> random(16);
    RAND_bytes(random.data(), random.size());
    std::ostringstream oss;
    for (uint8_t byte : random) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    challenge.challenge_id = oss.str();
    challenge.username = username;
    challenge.method_type = method->type;
    challenge.created = std::chrono::system_clock::now();
    challenge.expires = challenge.created + config_.challenge_validity;
    challenge.max_attempts = config_.max_failed_attempts;

    // Store challenge
    std::lock_guard<std::mutex> challenge_lock(challenges_mutex_);
    challenges_[challenge.challenge_id] = challenge;

    return core::Status::OK;
}

MfaVerifyResult MfaManager::verify(const std::string& username,
                                   const std::string& code,
                                   const std::string& challenge_id) {
    MfaVerifyResult result;

    std::lock_guard<std::mutex> lock(users_mutex_);

    auto user_it = user_configs_.find(username);
    if (user_it == user_configs_.end()) {
        result.error_message = "User not found";
        return result;
    }

    const UserMfaConfig& user_config = user_it->second;

    // Try TOTP first
    auto totp_methods = user_config.getMethodsByType(MfaMethodType::TOTP);
    for (const MfaMethodConfig* method : totp_methods) {
        if (verifyTotp(method->secret, code, method->otp_config)) {
            result.success = true;
            result.method_used = MfaMethodType::TOTP;
            return result;
        }
    }

    // Try backup codes
    auto backup_methods = user_config.getMethodsByType(MfaMethodType::BACKUP_CODES);
    for (auto* method_ptr : backup_methods) {
        // Need non-const access for modifying codes
        for (auto& stored_method : user_it->second.methods) {
            if (stored_method.type != MfaMethodType::BACKUP_CODES) continue;

            for (size_t i = 0; i < stored_method.codes.size(); ++i) {
                if (!stored_method.codes[i].empty() &&
                    verifyBackupCode(code, stored_method.codes[i])) {
                    // Mark code as used
                    stored_method.codes[i] = "";
                    stored_method.codes_remaining--;
                    stored_method.last_used = std::chrono::system_clock::now();

                    result.success = true;
                    result.method_used = MfaMethodType::BACKUP_CODES;
                    result.backup_code_used = true;
                    result.code_consumed = true;
                    return result;
                }
            }
        }
    }

    result.error_message = "Invalid code";
    return result;
}

bool MfaManager::requiresMfa(const std::string& username) {
    if (config_.enforce_mfa_for_all) {
        return true;
    }

    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it == user_configs_.end()) {
        return false;
    }

    const UserMfaConfig& config = it->second;

    // Check grace period
    if (config.grace_period_until > std::chrono::system_clock::now()) {
        return false;
    }

    return config.mfa_enabled || config.mfa_enforced;
}

bool MfaManager::hasMfaConfigured(const std::string& username) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it == user_configs_.end()) {
        return false;
    }

    return it->second.hasEnabledMethod();
}

core::Status MfaManager::getUserMfaConfig(const std::string& username,
                                          UserMfaConfig& config,
                                          core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it == user_configs_.end()) {
        config = UserMfaConfig{};
        config.username = username;
        return core::Status::OK;
    }

    config = it->second;
    return core::Status::OK;
}

core::Status MfaManager::enableMfaRequirement(const std::string& username,
                                              core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto& config = user_configs_[username];
    config.username = username;
    config.mfa_enforced = true;

    return storeUserMfaConfig(config);
}

core::Status MfaManager::disableMfaRequirement(const std::string& username,
                                               core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it == user_configs_.end()) {
        return core::Status::OK;
    }

    it->second.mfa_enforced = false;
    return storeUserMfaConfig(it->second);
}

core::Status MfaManager::setGracePeriod(const std::string& username,
                                        std::chrono::hours duration,
                                        core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto& config = user_configs_[username];
    config.username = username;
    config.grace_period_until = std::chrono::system_clock::now() + duration;

    return storeUserMfaConfig(config);
}

void MfaManager::setConfig(const Config& config) {
    config_ = config;
}

core::Status MfaManager::storeUserMfaConfig(const UserMfaConfig& config) {
    // In production, would persist to database
    // For now, just kept in memory
    return core::Status::OK;
}

core::Status MfaManager::loadUserMfaConfig(const std::string& username,
                                           UserMfaConfig& config) {
    // In production, would load from database
    std::lock_guard<std::mutex> lock(users_mutex_);

    auto it = user_configs_.find(username);
    if (it != user_configs_.end()) {
        config = it->second;
        return core::Status::OK;
    }

    return core::Status::NOT_FOUND;
}

// ============================================================================
// MfaAuthMethod Implementation
// ============================================================================

MfaAuthMethod::MfaAuthMethod(std::shared_ptr<AuthMethod> primary_method,
                             std::shared_ptr<MfaManager> mfa_manager)
    : primary_method_(primary_method)
    , mfa_manager_(mfa_manager)
{}

MfaAuthMethod::~MfaAuthMethod() = default;

core::Status MfaAuthMethod::initialize(const std::map<std::string, std::string>& config,
                                       core::ErrorContext* ctx) {
    return primary_method_->initialize(config, ctx);
}

AuthResult MfaAuthMethod::start(AuthContext& ctx) {
    // Start primary authentication
    auto result = primary_method_->start(ctx);

    // Track state
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[&ctx] = SessionState{MfaAuthState::PRIMARY_AUTH, ctx.username()};

    return result;
}

AuthResult MfaAuthMethod::continueAuth(AuthContext& ctx,
                                       const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = sessions_.find(&ctx);
    if (it == sessions_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No session state");
    }

    SessionState& state = it->second;

    switch (state.state) {
        case MfaAuthState::PRIMARY_AUTH: {
            // Continue primary auth
            auto result = primary_method_->continueAuth(ctx, data);

            if (result.state == AuthState::SUCCESS) {
                state.username = result.authenticated_user;

                // Check if MFA required
                if (mfa_manager_->requiresMfa(state.username)) {
                    if (!mfa_manager_->hasMfaConfigured(state.username)) {
                        // MFA required but not configured - fail or allow setup
                        result.warnings.push_back("MFA required but not configured");
                        // Could redirect to MFA setup here
                        return result;
                    }

                    // Create MFA challenge
                    core::ErrorContext err_ctx;
                    auto status = mfa_manager_->createChallenge(state.username,
                                                                state.challenge, &err_ctx);
                    if (status != core::Status::OK) {
                        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR,
                                                   "Failed to create MFA challenge");
                    }

                    state.state = MfaAuthState::MFA_REQUIRED;

                    // Return partial success - need MFA
                    AuthResult mfa_result;
                    mfa_result.state = AuthState::IN_PROGRESS;
                    mfa_result.requires_response = true;
                    // Signal MFA required
                    mfa_result.response_data = {'M', 'F', 'A'};
                    return mfa_result;
                }
            }

            return result;
        }

        case MfaAuthState::MFA_REQUIRED:
        case MfaAuthState::MFA_VERIFY:
            return processMfaVerification(ctx, data);

        default:
            return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "Invalid state");
    }
}

void MfaAuthMethod::abort(AuthContext& ctx) {
    primary_method_->abort(ctx);

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(&ctx);
}

AuthResult MfaAuthMethod::processMfaVerification(AuthContext& ctx,
                                                 const std::vector<uint8_t>& data) {
    auto it = sessions_.find(&ctx);
    if (it == sessions_.end()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No session");
    }

    SessionState& state = it->second;
    std::string code(data.begin(), data.end());

    auto verify_result = mfa_manager_->verify(state.username, code, state.challenge.challenge_id);

    if (verify_result.success) {
        state.state = MfaAuthState::COMPLETE;

        ctx.setState(AuthState::SUCCESS);
        ctx.setAuthenticatedUser(state.username);

        AuthResult result = AuthResult::success(state.username);

        if (verify_result.backup_code_used) {
            result.warnings.push_back("Backup code used - consider generating new codes");
        }

        return result;
    }

    // Increment attempts
    state.challenge.attempts++;

    if (state.challenge.attempts >= state.challenge.max_attempts) {
        ctx.setFailure(AuthFailReason::RATE_LIMITED, "Too many MFA attempts");
        return AuthResult::failure(AuthFailReason::RATE_LIMITED, "Too many MFA attempts");
    }

    // Allow retry
    AuthResult retry;
    retry.state = AuthState::IN_PROGRESS;
    retry.requires_response = true;
    retry.response_data = {'M', 'F', 'A', '_', 'R', 'E', 'T', 'R', 'Y'};

    return retry;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* mfaMethodTypeToString(MfaMethodType type) {
    switch (type) {
        case MfaMethodType::TOTP: return "totp";
        case MfaMethodType::HOTP: return "hotp";
        case MfaMethodType::BACKUP_CODES: return "backup_codes";
        case MfaMethodType::WEBAUTHN: return "webauthn";
        case MfaMethodType::PUSH: return "push";
        case MfaMethodType::SMS: return "sms";
        case MfaMethodType::EMAIL: return "email";
        case MfaMethodType::HARDWARE_TOKEN: return "hardware_token";
        default: return "unknown";
    }
}

bool parseMfaMethodType(const std::string& str, MfaMethodType& type) {
    if (str == "totp") { type = MfaMethodType::TOTP; return true; }
    if (str == "hotp") { type = MfaMethodType::HOTP; return true; }
    if (str == "backup_codes") { type = MfaMethodType::BACKUP_CODES; return true; }
    if (str == "webauthn") { type = MfaMethodType::WEBAUTHN; return true; }
    if (str == "push") { type = MfaMethodType::PUSH; return true; }
    if (str == "sms") { type = MfaMethodType::SMS; return true; }
    if (str == "email") { type = MfaMethodType::EMAIL; return true; }
    if (str == "hardware_token") { type = MfaMethodType::HARDWARE_TOKEN; return true; }
    return false;
}

bool secureCompare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }

    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::vector<uint8_t> generateSecureRandom(size_t bytes) {
    std::vector<uint8_t> result(bytes);
    RAND_bytes(result.data(), result.size());
    return result;
}

std::string generateReadableCode(size_t length, bool uppercase, bool digits) {
    std::string alphabet;

    if (uppercase) {
        alphabet += "ABCDEFGHJKLMNPQRSTUVWXYZ";  // No I, O (confusing)
    } else {
        alphabet += "abcdefghjkmnpqrstuvwxyz";  // No i, l, o
    }

    if (digits) {
        alphabet += "23456789";  // No 0, 1 (confusing)
    }

    std::string code;
    code.reserve(length);

    std::vector<uint8_t> random(length);
    RAND_bytes(random.data(), random.size());

    for (size_t i = 0; i < length; ++i) {
        code += alphabet[random[i] % alphabet.size()];
    }

    return code;
}

}  // namespace security
}  // namespace scratchbird
