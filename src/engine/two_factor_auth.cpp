#include "scratchbird/engine/two_factor_auth.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <unordered_map>

// For cryptographic operations
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace ScratchBird
{

    // Utility function implementations
    std::string to_string(TwoFactorMethod method)
    {
        switch (method) {
        case TwoFactorMethod::TOTP:
            return "TOTP";
        case TwoFactorMethod::SMS:
            return "SMS";
        case TwoFactorMethod::Email:
            return "Email";
        case TwoFactorMethod::Hardware:
            return "Hardware";
        case TwoFactorMethod::Backup:
            return "Backup";
        case TwoFactorMethod::Push:
            return "Push";
        case TwoFactorMethod::Voice:
            return "Voice";
        case TwoFactorMethod::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    std::string to_string(TwoFactorStatus status)
    {
        switch (status) {
        case TwoFactorStatus::NotEnrolled:
            return "NotEnrolled";
        case TwoFactorStatus::Enrolled:
            return "Enrolled";
        case TwoFactorStatus::Pending:
            return "Pending";
        case TwoFactorStatus::Disabled:
            return "Disabled";
        case TwoFactorStatus::Locked:
            return "Locked";
        default:
            return "Unknown";
        }
    }

    TwoFactorMethod parse_two_factor_method(const std::string& method_str)
    {
        if (method_str == "TOTP")
            return TwoFactorMethod::TOTP;
        if (method_str == "SMS")
            return TwoFactorMethod::SMS;
        if (method_str == "Email")
            return TwoFactorMethod::Email;
        if (method_str == "Hardware")
            return TwoFactorMethod::Hardware;
        if (method_str == "Backup")
            return TwoFactorMethod::Backup;
        if (method_str == "Push")
            return TwoFactorMethod::Push;
        if (method_str == "Voice")
            return TwoFactorMethod::Voice;
        if (method_str == "Custom")
            return TwoFactorMethod::Custom;
        return TwoFactorMethod::TOTP; // Default
    }

    TwoFactorStatus parse_two_factor_status(const std::string& status_str)
    {
        if (status_str == "NotEnrolled")
            return TwoFactorStatus::NotEnrolled;
        if (status_str == "Enrolled")
            return TwoFactorStatus::Enrolled;
        if (status_str == "Pending")
            return TwoFactorStatus::Pending;
        if (status_str == "Disabled")
            return TwoFactorStatus::Disabled;
        if (status_str == "Locked")
            return TwoFactorStatus::Locked;
        return TwoFactorStatus::NotEnrolled; // Default
    }

    // Base32 encoding/decoding for TOTP secrets
    namespace
    {
        const std::string base32_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

        std::string base32_encode(const std::vector<uint8_t>& data)
        {
            std::string result;
            int bits = 0;
            int value = 0;

            for (uint8_t byte : data) {
                value = (value << 8) | byte;
                bits += 8;

                while (bits >= 5) {
                    bits -= 5;
                    result += base32_chars[(value >> bits) & 0x1F];
                }
            }

            if (bits > 0) {
                result += base32_chars[(value << (5 - bits)) & 0x1F];
            }

            // Add padding
            while (result.length() % 8 != 0) {
                result += '=';
            }

            return result;
        }

        std::vector<uint8_t> base32_decode(const std::string& encoded)
        {
            std::vector<uint8_t> result;
            int bits = 0;
            int value = 0;

            for (char c : encoded) {
                if (c == '=')
                    break;

                auto pos = base32_chars.find(std::toupper(c));
                if (pos == std::string::npos)
                    continue;

                value = (value << 5) | pos;
                bits += 5;

                if (bits >= 8) {
                    bits -= 8;
                    result.push_back((value >> bits) & 0xFF);
                }
            }

            return result;
        }

        std::uint32_t hmac_sha1(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data)
        {
            unsigned char digest[EVP_MAX_MD_SIZE];
            unsigned int digest_len = 0;

            HMAC(EVP_sha1(), key.data(), key.size(), data.data(), data.size(), digest, &digest_len);

            // Extract dynamic binary code
            int offset = digest[digest_len - 1] & 0x0F;
            std::uint32_t code = ((digest[offset] & 0x7F) << 24) |
                                 ((digest[offset + 1] & 0xFF) << 16) |
                                 ((digest[offset + 2] & 0xFF) << 8) | (digest[offset + 3] & 0xFF);

            return code;
        }
    } // namespace

    // TOTP utility functions
    std::string generate_totp_secret_key()
    {
        std::vector<uint8_t> secret(20); // 160-bit secret
        if (RAND_bytes(secret.data(), secret.size()) != 1) {
            // Fallback to system random
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);
            for (auto& byte : secret) {
                byte = static_cast<uint8_t>(dis(gen));
            }
        }

        return base32_encode(secret);
    }

    std::string generate_totp_code(const std::string& secret_key, std::uint64_t timestamp,
                                   std::uint32_t digits, const std::string& algorithm)
    {
        auto secret_bytes = base32_decode(secret_key);

        // Convert timestamp to 8-byte array (big-endian)
        std::vector<uint8_t> time_bytes(8);
        for (int i = 7; i >= 0; --i) {
            time_bytes[i] = timestamp & 0xFF;
            timestamp >>= 8;
        }

        // Generate HMAC
        std::uint32_t code = hmac_sha1(secret_bytes, time_bytes);

        // Generate digits
        code = code % static_cast<std::uint32_t>(std::pow(10, digits));

        // Format with leading zeros
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(digits) << code;
        return ss.str();
    }

    bool verify_totp_code(const std::string& secret_key, const std::string& code,
                          std::uint64_t timestamp, std::uint32_t window, std::uint32_t digits,
                          const std::string& algorithm)
    {
        // Check current time and adjacent windows
        for (std::uint32_t i = 0; i <= window * 2; ++i) {
            std::int64_t test_time = static_cast<std::int64_t>(timestamp) -
                                     static_cast<std::int64_t>(window) +
                                     static_cast<std::int64_t>(i);
            if (test_time < 0)
                continue;

            std::string expected_code = generate_totp_code(
                secret_key, static_cast<std::uint64_t>(test_time), digits, algorithm);
            if (code == expected_code) {
                return true;
            }
        }

        return false;
    }

    std::string generate_totp_qr_url(const std::string& secret_key, const std::string& account_name,
                                     const std::string& issuer)
    {
        std::stringstream ss;
        ss << "otpauth://totp/";
        ss << issuer << ":" << account_name;
        ss << "?secret=" << secret_key;
        ss << "&issuer=" << issuer;
        ss << "&algorithm=SHA1";
        ss << "&digits=6";
        ss << "&period=30";
        return ss.str();
    }

    // TOTPConfig implementation
    std::string TOTPConfig::generate_qr_code_url() const
    {
        return generate_totp_qr_url(secret_key, account_name, issuer);
    }

    std::string TOTPConfig::generate_code() const
    {
        std::uint64_t current_time = static_cast<std::uint64_t>(std::time(nullptr)) / time_step;
        return generate_totp_code(secret_key, current_time, digits, algorithm);
    }

    std::string TOTPConfig::generate_code(std::uint64_t timestamp) const
    {
        std::uint64_t time_counter = timestamp / time_step;
        return generate_totp_code(secret_key, time_counter, digits, algorithm);
    }

    bool TOTPConfig::verify_code(const std::string& code, std::uint32_t window) const
    {
        std::uint64_t current_time = static_cast<std::uint64_t>(std::time(nullptr)) / time_step;
        return verify_totp_code(secret_key, code, current_time, window, digits, algorithm);
    }

    // TOTPProvider implementation
    class TOTPProvider::Impl
    {
      public:
        struct UserTOTPData {
            TOTPConfig config;
            TwoFactorStatus status = TwoFactorStatus::NotEnrolled;
            std::vector<std::string> backup_codes;
            std::vector<std::string> used_backup_codes;
            std::chrono::system_clock::time_point enrollment_time;
            std::uint32_t failed_attempts = 0;
            std::chrono::system_clock::time_point last_failure;
        };

        std::unordered_map<std::string, UserTOTPData> user_data_;
        mutable std::mutex data_mutex_;

        std::unordered_map<std::string, TOTPConfig> pending_enrollments_;
        mutable std::mutex enrollment_mutex_;

        std::string generate_backup_codes(std::uint32_t count = 10)
        {
            std::vector<std::string> codes;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(10000000, 99999999); // 8-digit codes

            for (std::uint32_t i = 0; i < count; ++i) {
                codes.push_back(std::to_string(dis(gen)));
            }

            std::stringstream ss;
            for (size_t i = 0; i < codes.size(); ++i) {
                if (i > 0)
                    ss << ",";
                ss << codes[i];
            }

            return ss.str();
        }

        std::vector<std::string> parse_backup_codes(const std::string& codes_str)
        {
            std::vector<std::string> codes;
            std::stringstream ss(codes_str);
            std::string code;

            while (std::getline(ss, code, ',')) {
                codes.push_back(code);
            }

            return codes;
        }
    };

    TOTPProvider::TOTPProvider() : pimpl_(std::make_unique<Impl>()) {}

    TOTPProvider::~TOTPProvider() = default;

    std::vector<std::string> TOTPProvider::get_supported_capabilities() const
    {
        return {"totp_authentication", "qr_code_enrollment", "backup_codes",
                "time_based_verification", "mobile_app_support"};
    }

    AuthenticationResult TOTPProvider::begin_enrollment(const std::string& username,
                                                        AuthenticationContext& context)
    {
        std::lock_guard<std::mutex> lock(pimpl_->enrollment_mutex_);

        // Generate new TOTP configuration
        TOTPConfig config;
        config.secret_key = generate_totp_secret_key();
        config.account_name = username;
        config.issuer = issuer_;
        config.time_step = time_step_;
        config.digits = digits_;
        config.algorithm = algorithm_;

        // Store pending enrollment
        pimpl_->pending_enrollments_[username] = config;

        // Provide QR code URL to client
        context.set_credential("qr_code_url", config.generate_qr_code_url());
        context.set_credential("secret_key", config.secret_key);
        context.set_credential("enrollment_method", "TOTP");

        return AuthenticationResult::Success;
    }

    AuthenticationResult TOTPProvider::complete_enrollment(const std::string& username,
                                                           const std::string& verification_code,
                                                           AuthenticationContext& context)
    {
        std::lock_guard<std::mutex> enrollment_lock(pimpl_->enrollment_mutex_);

        // Find pending enrollment
        auto it = pimpl_->pending_enrollments_.find(username);
        if (it == pimpl_->pending_enrollments_.end()) {
            return AuthenticationResult::InvalidCredentials;
        }

        TOTPConfig config = it->second;
        pimpl_->pending_enrollments_.erase(it);

        // Verify the provided code
        if (!config.verify_code(verification_code)) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Store user TOTP data
        {
            std::lock_guard<std::mutex> data_lock(pimpl_->data_mutex_);
            TOTPProvider::Impl::UserTOTPData user_data;
            user_data.config = config;
            user_data.status = TwoFactorStatus::Enrolled;
            user_data.enrollment_time = std::chrono::system_clock::now();

            // Generate backup codes
            std::string backup_codes_str = pimpl_->generate_backup_codes(10);
            user_data.backup_codes = pimpl_->parse_backup_codes(backup_codes_str);

            pimpl_->user_data_[username] = user_data;

            // Provide backup codes to user
            context.set_credential("backup_codes", backup_codes_str);
        }

        return AuthenticationResult::Success;
    }

    AuthenticationResult TOTPProvider::cancel_enrollment(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->enrollment_mutex_);
        pimpl_->pending_enrollments_.erase(username);
        return AuthenticationResult::Success;
    }

    std::unique_ptr<AuthenticationChallenge>
    TOTPProvider::create_challenge(const std::string& username, AuthenticationContext& context)
    {

        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        if (it == pimpl_->user_data_.end() || it->second.status != TwoFactorStatus::Enrolled) {
            return nullptr;
        }

        // Generate challenge ID
        std::string challenge_id = "totp_" + std::to_string(std::time(nullptr));

        auto challenge = std::make_unique<AuthenticationChallenge>(
            challenge_id, AuthenticationMethod::TwoFactor, "Please enter your 6-digit TOTP code");

        // Set expiry (TOTP codes are time-based, give 2-3 time windows)
        auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(time_step_ * 3);
        challenge->set_expiry(expiry);

        return challenge;
    }

    AuthenticationResult TOTPProvider::verify_response(const AuthenticationChallenge& challenge,
                                                       const std::string& response,
                                                       AuthenticationContext& context)
    {

        const std::string& username = context.get_username();

        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        if (it == pimpl_->user_data_.end() || it->second.status != TwoFactorStatus::Enrolled) {
            return AuthenticationResult::InvalidCredentials;
        }

        auto& user_data = it->second;

        // Check if this is a backup code
        if (response.length() == 8 && std::all_of(response.begin(), response.end(), ::isdigit)) {
            auto backup_it =
                std::find(user_data.backup_codes.begin(), user_data.backup_codes.end(), response);
            if (backup_it != user_data.backup_codes.end()) {
                // Mark backup code as used
                user_data.used_backup_codes.push_back(response);
                user_data.backup_codes.erase(backup_it);

                // Reset failed attempts
                user_data.failed_attempts = 0;

                context.set_credential("auth_method", "backup_code");
                return AuthenticationResult::Success;
            }
        }

        // Verify TOTP code
        if (user_data.config.verify_code(response)) {
            // Reset failed attempts
            user_data.failed_attempts = 0;

            context.set_credential("auth_method", "totp");
            return AuthenticationResult::Success;
        }

        // Record failed attempt
        user_data.failed_attempts++;
        user_data.last_failure = std::chrono::system_clock::now();

        // Lock after too many failures
        if (user_data.failed_attempts >= 5) {
            user_data.status = TwoFactorStatus::Locked;
            return AuthenticationResult::AccountLocked;
        }

        return AuthenticationResult::InvalidCredentials;
    }

    TwoFactorStatus TOTPProvider::get_status(const std::string& username) const
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        return (it != pimpl_->user_data_.end()) ? it->second.status : TwoFactorStatus::NotEnrolled;
    }

    AuthenticationResult TOTPProvider::disable_2fa(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        if (it != pimpl_->user_data_.end()) {
            it->second.status = TwoFactorStatus::Disabled;
        }

        return AuthenticationResult::Success;
    }

    AuthenticationResult TOTPProvider::enable_2fa(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        if (it != pimpl_->user_data_.end() && it->second.status == TwoFactorStatus::Disabled) {
            it->second.status = TwoFactorStatus::Enrolled;
        }

        return AuthenticationResult::Success;
    }

    std::vector<std::string> TOTPProvider::generate_backup_codes(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        if (it == pimpl_->user_data_.end()) {
            return {};
        }

        // Generate new backup codes
        std::string codes_str = pimpl_->generate_backup_codes(10);
        std::vector<std::string> new_codes = pimpl_->parse_backup_codes(codes_str);

        // Replace old backup codes
        it->second.backup_codes = new_codes;
        it->second.used_backup_codes.clear();

        return new_codes;
    }

    bool TOTPProvider::verify_backup_code(const std::string& username, const std::string& code)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        if (it == pimpl_->user_data_.end()) {
            return false;
        }

        auto& user_data = it->second;
        auto code_it =
            std::find(user_data.backup_codes.begin(), user_data.backup_codes.end(), code);

        if (code_it != user_data.backup_codes.end()) {
            // Mark as used and remove
            user_data.used_backup_codes.push_back(code);
            user_data.backup_codes.erase(code_it);
            return true;
        }

        return false;
    }

    AuthenticationResult TOTPProvider::reset_2fa(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        pimpl_->user_data_.erase(username);

        // Also remove pending enrollments
        {
            std::lock_guard<std::mutex> enrollment_lock(pimpl_->enrollment_mutex_);
            pimpl_->pending_enrollments_.erase(username);
        }

        return AuthenticationResult::Success;
    }

    bool TOTPProvider::validate_configuration() const
    {
        return time_step_ > 0 && (digits_ == 6 || digits_ == 8) && !issuer_.empty();
    }

    std::vector<std::string> TOTPProvider::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (time_step_ == 0) {
            errors.push_back("Invalid time step: must be greater than 0");
        }

        if (digits_ != 6 && digits_ != 8) {
            errors.push_back("Invalid digits: must be 6 or 8");
        }

        if (issuer_.empty()) {
            errors.push_back("Issuer name must be specified");
        }

        return errors;
    }

    std::string TOTPProvider::generate_secret_key() const
    {
        return generate_totp_secret_key();
    }

    TOTPConfig TOTPProvider::get_totp_config(const std::string& username) const
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_data_.find(username);
        return (it != pimpl_->user_data_.end()) ? it->second.config : TOTPConfig{};
    }

    std::string TOTPProvider::get_qr_code_url(const std::string& username) const
    {
        TOTPConfig config = get_totp_config(username);
        return config.secret_key.empty() ? "" : config.generate_qr_code_url();
    }

    // Simplified SMS and Email provider implementations
    class SMSProvider::Impl
    {
      public:
        std::unordered_map<std::string, std::string> user_phone_numbers_;
        std::unordered_map<std::string, std::string> pending_codes_;
        std::unordered_map<std::string, std::chrono::system_clock::time_point> code_expiry_;
        mutable std::mutex data_mutex_;

        std::string generate_code()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(100000, 999999); // 6-digit code
            return std::to_string(dis(gen));
        }
    };

    SMSProvider::SMSProvider(const std::string& api_key, const std::string& service_url)
        : api_key_(api_key), service_url_(service_url), pimpl_(std::make_unique<Impl>())
    {
    }

    SMSProvider::~SMSProvider() = default;

    std::vector<std::string> SMSProvider::get_supported_capabilities() const
    {
        return {"sms_authentication", "phone_number_verification", "global_sms_support"};
    }

    AuthenticationResult SMSProvider::begin_enrollment(const std::string& username,
                                                       AuthenticationContext& context)
    {
        // This would typically prompt user to enter phone number
        context.set_credential("enrollment_method", "SMS");
        context.set_credential("instructions", "Please provide your phone number");
        return AuthenticationResult::Success;
    }

    AuthenticationResult SMSProvider::complete_enrollment(const std::string& username,
                                                          const std::string& phone_number,
                                                          AuthenticationContext& context)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        // Store phone number
        pimpl_->user_phone_numbers_[username] = phone_number;

        // Send verification SMS
        std::string code = pimpl_->generate_code();
        if (send_sms(phone_number,
                     message_template_.replace(message_template_.find("{code}"), 6, code))) {
            pimpl_->pending_codes_[username] = code;
            pimpl_->code_expiry_[username] =
                std::chrono::system_clock::now() + std::chrono::minutes(5);
            return AuthenticationResult::Success;
        }

        return AuthenticationResult::InternalError;
    }

    AuthenticationResult SMSProvider::cancel_enrollment(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);
        pimpl_->user_phone_numbers_.erase(username);
        pimpl_->pending_codes_.erase(username);
        pimpl_->code_expiry_.erase(username);
        return AuthenticationResult::Success;
    }

    std::unique_ptr<AuthenticationChallenge>
    SMSProvider::create_challenge(const std::string& username, AuthenticationContext& context)
    {

        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_phone_numbers_.find(username);
        if (it == pimpl_->user_phone_numbers_.end()) {
            return nullptr;
        }

        // Generate and send SMS code
        std::string code = pimpl_->generate_code();
        std::string message = message_template_;
        message.replace(message.find("{code}"), 6, code);

        if (send_sms(it->second, message)) {
            pimpl_->pending_codes_[username] = code;
            pimpl_->code_expiry_[username] =
                std::chrono::system_clock::now() + std::chrono::minutes(5);

            std::string challenge_id = "sms_" + std::to_string(std::time(nullptr));
            auto challenge = std::make_unique<AuthenticationChallenge>(
                challenge_id, AuthenticationMethod::TwoFactor,
                "Please enter the 6-digit code sent to your phone");

            challenge->set_expiry(pimpl_->code_expiry_[username]);
            return challenge;
        }

        return nullptr;
    }

    AuthenticationResult SMSProvider::verify_response(const AuthenticationChallenge& challenge,
                                                      const std::string& response,
                                                      AuthenticationContext& context)
    {

        const std::string& username = context.get_username();

        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->pending_codes_.find(username);
        if (it == pimpl_->pending_codes_.end()) {
            return AuthenticationResult::InvalidCredentials;
        }

        // Check expiry
        auto expiry_it = pimpl_->code_expiry_.find(username);
        if (expiry_it != pimpl_->code_expiry_.end() &&
            std::chrono::system_clock::now() > expiry_it->second) {
            pimpl_->pending_codes_.erase(it);
            pimpl_->code_expiry_.erase(expiry_it);
            return AuthenticationResult::Timeout;
        }

        // Verify code
        if (it->second == response) {
            pimpl_->pending_codes_.erase(it);
            if (expiry_it != pimpl_->code_expiry_.end()) {
                pimpl_->code_expiry_.erase(expiry_it);
            }

            context.set_credential("auth_method", "sms");
            return AuthenticationResult::Success;
        }

        return AuthenticationResult::InvalidCredentials;
    }

    TwoFactorStatus SMSProvider::get_status(const std::string& username) const
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_phone_numbers_.find(username);
        return (it != pimpl_->user_phone_numbers_.end()) ? TwoFactorStatus::Enrolled
                                                         : TwoFactorStatus::NotEnrolled;
    }

    AuthenticationResult SMSProvider::disable_2fa(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);
        pimpl_->user_phone_numbers_.erase(username);
        return AuthenticationResult::Success;
    }

    AuthenticationResult SMSProvider::enable_2fa(const std::string& username)
    {
        // SMS is enabled when phone number is enrolled
        return AuthenticationResult::Success;
    }

    std::vector<std::string> SMSProvider::generate_backup_codes(const std::string& username)
    {
        // SMS doesn't typically use backup codes
        return {};
    }

    bool SMSProvider::verify_backup_code(const std::string& username, const std::string& code)
    {
        return false; // SMS doesn't use backup codes
    }

    AuthenticationResult SMSProvider::reset_2fa(const std::string& username)
    {
        return disable_2fa(username);
    }

    bool SMSProvider::validate_configuration() const
    {
        return !api_key_.empty() && !service_url_.empty();
    }

    std::vector<std::string> SMSProvider::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (api_key_.empty()) {
            errors.push_back("SMS API key not configured");
        }

        if (service_url_.empty()) {
            errors.push_back("SMS service URL not configured");
        }

        return errors;
    }

    bool SMSProvider::send_sms(const std::string& phone_number, const std::string& message)
    {
        // This would implement actual SMS sending via API (Twilio, AWS SNS, etc.)
        // For now, this is a placeholder that always succeeds
        return true;
    }

    std::string SMSProvider::generate_verification_code() const
    {
        return pimpl_->generate_code();
    }

    // Email provider would be similar to SMS provider but with email sending
    class EmailProvider::Impl
    {
      public:
        std::unordered_map<std::string, std::string> user_email_addresses_;
        std::unordered_map<std::string, std::string> pending_codes_;
        std::unordered_map<std::string, std::chrono::system_clock::time_point> code_expiry_;
        mutable std::mutex data_mutex_;

        std::string generate_code()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(100000, 999999);
            return std::to_string(dis(gen));
        }
    };

    EmailProvider::EmailProvider(const std::string& smtp_server, std::uint16_t smtp_port,
                                 const std::string& smtp_username, const std::string& smtp_password)
        : smtp_server_(smtp_server), smtp_port_(smtp_port), smtp_username_(smtp_username),
          smtp_password_(smtp_password), pimpl_(std::make_unique<Impl>())
    {
    }

    EmailProvider::~EmailProvider() = default;

    std::vector<std::string> EmailProvider::get_supported_capabilities() const
    {
        return {"email_authentication", "email_verification", "smtp_support"};
    }

    // Email provider methods would be similar to SMS provider but with email operations
    AuthenticationResult EmailProvider::begin_enrollment(const std::string& username,
                                                         AuthenticationContext& context)
    {
        context.set_credential("enrollment_method", "Email");
        context.set_credential("instructions", "Please provide your email address");
        return AuthenticationResult::Success;
    }

    AuthenticationResult EmailProvider::complete_enrollment(const std::string& username,
                                                            const std::string& email_address,
                                                            AuthenticationContext& context)
    {
        // Similar to SMS but send email instead
        return AuthenticationResult::Success;
    }

    AuthenticationResult EmailProvider::cancel_enrollment(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);
        pimpl_->user_email_addresses_.erase(username);
        return AuthenticationResult::Success;
    }

    std::unique_ptr<AuthenticationChallenge>
    EmailProvider::create_challenge(const std::string& username, AuthenticationContext& context)
    {
        // Similar to SMS challenge creation
        return nullptr;
    }

    AuthenticationResult EmailProvider::verify_response(const AuthenticationChallenge& challenge,
                                                        const std::string& response,
                                                        AuthenticationContext& context)
    {
        // Similar to SMS verification
        return AuthenticationResult::InvalidCredentials;
    }

    TwoFactorStatus EmailProvider::get_status(const std::string& username) const
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);

        auto it = pimpl_->user_email_addresses_.find(username);
        return (it != pimpl_->user_email_addresses_.end()) ? TwoFactorStatus::Enrolled
                                                           : TwoFactorStatus::NotEnrolled;
    }

    AuthenticationResult EmailProvider::disable_2fa(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->data_mutex_);
        pimpl_->user_email_addresses_.erase(username);
        return AuthenticationResult::Success;
    }

    AuthenticationResult EmailProvider::enable_2fa(const std::string& username)
    {
        return AuthenticationResult::Success;
    }

    std::vector<std::string> EmailProvider::generate_backup_codes(const std::string& username)
    {
        return {};
    }

    bool EmailProvider::verify_backup_code(const std::string& username, const std::string& code)
    {
        return false;
    }

    AuthenticationResult EmailProvider::reset_2fa(const std::string& username)
    {
        return disable_2fa(username);
    }

    bool EmailProvider::validate_configuration() const
    {
        return !smtp_server_.empty() && smtp_port_ > 0;
    }

    std::vector<std::string> EmailProvider::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (smtp_server_.empty()) {
            errors.push_back("SMTP server not configured");
        }

        if (smtp_port_ == 0) {
            errors.push_back("SMTP port not configured");
        }

        return errors;
    }

    bool EmailProvider::send_email(const std::string& to_address, const std::string& subject,
                                   const std::string& body)
    {
        // This would implement actual email sending via SMTP
        // For now, this is a placeholder
        return true;
    }

    std::string EmailProvider::generate_verification_code() const
    {
        return pimpl_->generate_code();
    }

    void EmailProvider::set_smtp_config(const std::string& server, std::uint16_t port,
                                        const std::string& username, const std::string& password)
    {
        smtp_server_ = server;
        smtp_port_ = port;
        smtp_username_ = username;
        smtp_password_ = password;
    }

    void EmailProvider::set_email_template(const std::string& subject, const std::string& body)
    {
        email_subject_ = subject;
        email_template_ = body;
    }

    // TwoFactorAuthenticationManager and TwoFactorAuthenticationProvider would follow
    // similar patterns to the main AuthenticationManager but focused on 2FA operations

    class TwoFactorAuthenticationManager::Impl
    {
      public:
        std::unordered_map<std::string, std::unique_ptr<TwoFactorProvider>> providers_;
        std::unordered_map<TwoFactorMethod, std::string> method_to_provider_;
        mutable std::mutex providers_mutex_;

        TwoFactorAuthenticationManager::TwoFactorStats stats_;
        mutable std::mutex stats_mutex_;
    };

    TwoFactorAuthenticationManager::TwoFactorAuthenticationManager()
        : pimpl_(std::make_unique<Impl>())
    {
        pimpl_->stats_.last_reset = std::chrono::system_clock::now();
    }

    TwoFactorAuthenticationManager::~TwoFactorAuthenticationManager() = default;

    bool
    TwoFactorAuthenticationManager::register_provider(std::unique_ptr<TwoFactorProvider> provider)
    {
        if (!provider || !provider->validate_configuration()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        const std::string& provider_name = provider->get_provider_name();
        const TwoFactorMethod method = provider->get_method();

        pimpl_->providers_[provider_name] = std::move(provider);
        pimpl_->method_to_provider_[method] = provider_name;

        return true;
    }

    bool TwoFactorAuthenticationManager::unregister_provider(const std::string& provider_name)
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        auto it = pimpl_->providers_.find(provider_name);
        if (it == pimpl_->providers_.end()) {
            return false;
        }

        const TwoFactorMethod method = it->second->get_method();
        pimpl_->method_to_provider_.erase(method);
        pimpl_->providers_.erase(it);

        return true;
    }

    std::vector<std::string> TwoFactorAuthenticationManager::get_registered_providers() const
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        std::vector<std::string> provider_names;
        for (const auto& [name, provider] : pimpl_->providers_) {
            provider_names.push_back(name);
        }

        return provider_names;
    }

    TwoFactorProvider*
    TwoFactorAuthenticationManager::get_provider(const std::string& provider_name)
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        auto it = pimpl_->providers_.find(provider_name);
        return (it != pimpl_->providers_.end()) ? it->second.get() : nullptr;
    }

    TwoFactorProvider* TwoFactorAuthenticationManager::get_provider(TwoFactorMethod method)
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        auto it = pimpl_->method_to_provider_.find(method);
        if (it != pimpl_->method_to_provider_.end()) {
            auto provider_it = pimpl_->providers_.find(it->second);
            if (provider_it != pimpl_->providers_.end()) {
                return provider_it->second.get();
            }
        }

        return nullptr;
    }

    TwoFactorAuthenticationManager::TwoFactorStats
    TwoFactorAuthenticationManager::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
        return pimpl_->stats_;
    }

    void TwoFactorAuthenticationManager::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
        pimpl_->stats_ = TwoFactorStats{};
        pimpl_->stats_.last_reset = std::chrono::system_clock::now();
    }

    // Simplified implementations for other methods...
    std::vector<TwoFactorMethod>
    TwoFactorAuthenticationManager::get_user_enrolled_methods(const std::string& username) const
    {
        std::vector<TwoFactorMethod> methods;

        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);
        for (const auto& [name, provider] : pimpl_->providers_) {
            if (provider->get_status(username) == TwoFactorStatus::Enrolled) {
                methods.push_back(provider->get_method());
            }
        }

        return methods;
    }

    bool TwoFactorAuthenticationManager::is_user_enrolled(const std::string& username) const
    {
        return !get_user_enrolled_methods(username).empty();
    }

    TwoFactorStatus
    TwoFactorAuthenticationManager::get_user_2fa_status(const std::string& username) const
    {
        return is_user_enrolled(username) ? TwoFactorStatus::Enrolled : TwoFactorStatus::NotEnrolled;
    }

    std::unique_ptr<AuthenticationChallenge> TwoFactorAuthenticationManager::initiate_2fa_challenge(
        const std::string& username, AuthenticationContext& context,
        TwoFactorMethod preferred_method)
    {
        // Try preferred provider first
        if (auto* provider = get_provider(preferred_method)) {
            if (auto challenge = provider->create_challenge(username, context)) {
                return challenge;
            }
        }

        // Fallback: try any enrolled provider
        auto methods = get_user_enrolled_methods(username);
        for (auto method : methods) {
            if (auto* provider = get_provider(method)) {
                if (auto challenge = provider->create_challenge(username, context)) {
                    return challenge;
                }
            }
        }

        return nullptr;
    }

    AuthenticationResult TwoFactorAuthenticationManager::verify_2fa_response(
        const AuthenticationChallenge& challenge, const std::string& response,
        AuthenticationContext& context)
    {
        const std::string& username = context.get_username();

        // Attempt verification against all providers (first success wins)
        {
            std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);
            for (auto& [name, provider] : pimpl_->providers_) {
                auto result = provider->verify_response(challenge, response, context);
                if (result == AuthenticationResult::Success) {
                    std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
                    pimpl_->stats_.successful_verifications++;
                    return result;
                } else if (result == AuthenticationResult::AccountLocked ||
                           result == AuthenticationResult::InvalidCredentials) {
                    // Count as failed attempt for statistics
                    std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
                    pimpl_->stats_.failed_verifications++;
                    // keep trying other providers unless account is locked
                    if (result == AuthenticationResult::AccountLocked)
                        return result;
                }
            }
        }

        return AuthenticationResult::InvalidCredentials;
    }

    // TwoFactorAuthenticationProvider implementation
    TwoFactorAuthenticationProvider::TwoFactorAuthenticationProvider(
        std::unique_ptr<TwoFactorAuthenticationManager> manager)
        : manager_(std::move(manager))
    {
    }

    TwoFactorAuthenticationProvider::~TwoFactorAuthenticationProvider() = default;

    std::vector<std::string> TwoFactorAuthenticationProvider::get_supported_capabilities() const
    {
        return {"two_factor_authentication",
                "totp_support",
                "sms_support",
                "email_support",
                "backup_codes",
                "enrollment_management"};
    }

    AuthenticationResult
    TwoFactorAuthenticationProvider::authenticate(AuthenticationContext& context)
    {
        const std::string& username = context.get_username();

        if (!manager_->is_user_enrolled(username)) {
            return AuthenticationResult::RequiresTwoFactor;
        }

        // 2FA authentication requires challenge-response flow
        return AuthenticationResult::RequiresTwoFactor;
    }

    std::unique_ptr<AuthenticationChallenge>
    TwoFactorAuthenticationProvider::create_challenge(const AuthenticationContext& context)
    {

        return manager_->initiate_2fa_challenge(context.get_username(),
                                                const_cast<AuthenticationContext&>(context));
    }

    AuthenticationResult TwoFactorAuthenticationProvider::validate_challenge_response(
        const AuthenticationChallenge& challenge, AuthenticationContext& context)
    {

        return manager_->verify_2fa_response(challenge, challenge.get_response(), context);
    }

    bool TwoFactorAuthenticationProvider::validate_configuration() const
    {
        return manager_ != nullptr && !manager_->get_registered_providers().empty();
    }

    std::vector<std::string> TwoFactorAuthenticationProvider::get_configuration_errors() const
    {
        std::vector<std::string> errors;

        if (!manager_) {
            errors.push_back("2FA manager not configured");
        } else if (manager_->get_registered_providers().empty()) {
            errors.push_back("No 2FA providers registered");
        }

        return errors;
    }

} // namespace ScratchBird
