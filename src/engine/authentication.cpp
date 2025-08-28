#include "scratchbird/engine/authentication.h"

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ScratchBird
{

    // Utility function implementations
    std::string to_string(AuthenticationResult result)
    {
        switch (result) {
        case AuthenticationResult::Success:
            return "Success";
        case AuthenticationResult::InvalidCredentials:
            return "Invalid credentials";
        case AuthenticationResult::AccountLocked:
            return "Account locked";
        case AuthenticationResult::PasswordExpired:
            return "Password expired";
        case AuthenticationResult::RequiresTwoFactor:
            return "Two-factor authentication required";
        case AuthenticationResult::RequiresPasswordChange:
            return "Password change required";
        case AuthenticationResult::AccessDenied:
            return "Access denied";
        case AuthenticationResult::InternalError:
            return "Internal error";
        case AuthenticationResult::Timeout:
            return "Authentication timeout";
        case AuthenticationResult::Cancelled:
            return "Authentication cancelled";
        default:
            return "Unknown result";
        }
    }

    std::string to_string(AuthenticationMethod method)
    {
        switch (method) {
        case AuthenticationMethod::Password:
            return "Password";
        case AuthenticationMethod::TrustedOS:
            return "TrustedOS";
        case AuthenticationMethod::Certificate:
            return "Certificate";
        case AuthenticationMethod::TwoFactor:
            return "TwoFactor";
        case AuthenticationMethod::SingleSignOn:
            return "SingleSignOn";
        case AuthenticationMethod::Token:
            return "Token";
        case AuthenticationMethod::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    AuthenticationMethod parse_authentication_method(const std::string& method_str)
    {
        if (method_str == "Password")
            return AuthenticationMethod::Password;
        if (method_str == "TrustedOS")
            return AuthenticationMethod::TrustedOS;
        if (method_str == "Certificate")
            return AuthenticationMethod::Certificate;
        if (method_str == "TwoFactor")
            return AuthenticationMethod::TwoFactor;
        if (method_str == "SingleSignOn")
            return AuthenticationMethod::SingleSignOn;
        if (method_str == "Token")
            return AuthenticationMethod::Token;
        if (method_str == "Custom")
            return AuthenticationMethod::Custom;
        return AuthenticationMethod::Password; // Default fallback
    }

    // SecurityContext implementation
    SecurityContext::SecurityContext(const std::string& username, const std::string& database,
                                     const std::vector<std::string>& roles)
        : username_(username), database_(database), roles_(roles),
          auth_time_(std::chrono::system_clock::now())
    {
    }

    bool SecurityContext::has_role(const std::string& role) const
    {
        return std::find(roles_.begin(), roles_.end(), role) != roles_.end();
    }

    void SecurityContext::add_role(const std::string& role)
    {
        if (!has_role(role)) {
            roles_.push_back(role);
        }
    }

    void SecurityContext::remove_role(const std::string& role)
    {
        roles_.erase(std::remove(roles_.begin(), roles_.end(), role), roles_.end());
    }

    bool SecurityContext::has_permission(const std::string& permission) const
    {
        return std::find(permissions_.begin(), permissions_.end(), permission) !=
               permissions_.end();
    }

    void SecurityContext::grant_permission(const std::string& permission)
    {
        if (!has_permission(permission)) {
            permissions_.push_back(permission);
        }
    }

    void SecurityContext::revoke_permission(const std::string& permission)
    {
        permissions_.erase(std::remove(permissions_.begin(), permissions_.end(), permission),
                           permissions_.end());
    }

    // AuthenticationManager::Impl - Private implementation
    class AuthenticationManager::Impl
    {
      public:
        // Provider management
        std::unordered_map<std::string, std::unique_ptr<AuthenticationProvider>> providers_;
        std::unordered_map<AuthenticationMethod, std::string> method_to_provider_;
        mutable std::mutex providers_mutex_;

        // Session tracking
        struct SessionInfo {
            std::string username;
            std::string database;
            std::chrono::system_clock::time_point created;
            std::chrono::system_clock::time_point last_activity;
            AuthenticationMethod auth_method;
            bool is_valid = true;
        };

        std::unordered_map<std::string, SessionInfo> active_sessions_;
        mutable std::mutex sessions_mutex_;

        // Challenge tracking
        std::unordered_map<std::string, std::unique_ptr<AuthenticationChallenge>>
            active_challenges_;
        mutable std::mutex challenges_mutex_;

        // Account lockout tracking
        struct AccountInfo {
            int failed_attempts = 0;
            std::chrono::system_clock::time_point last_failure;
            std::chrono::system_clock::time_point locked_until;
            bool is_locked = false;
        };

        std::unordered_map<std::string, AccountInfo> account_info_;
        mutable std::mutex accounts_mutex_;

        // Audit logging
        struct AuditEvent {
            std::string event_type;
            std::string username;
            std::string remote_address;
            AuthenticationResult result;
            std::chrono::system_clock::time_point timestamp;
            std::string details;
        };

        std::vector<AuditEvent> audit_log_;
        mutable std::mutex audit_mutex_;

        // Statistics
        AuthenticationManager::AuthenticationStats stats_;
        mutable std::mutex stats_mutex_;

        // Utility methods
        std::string generate_session_id()
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(0, 15);

            std::stringstream ss;
            for (int i = 0; i < 32; ++i) {
                ss << std::hex << dis(gen);
            }
            return ss.str();
        }

        std::string generate_challenge_id()
        {
            return generate_session_id(); // Same format for challenge IDs
        }
    };

    // AuthenticationManager implementation
    AuthenticationManager::AuthenticationManager() : pimpl_(std::make_unique<Impl>())
    {
        pimpl_->stats_.last_reset = std::chrono::system_clock::now();
    }

    AuthenticationManager::~AuthenticationManager() = default;

    bool AuthenticationManager::register_provider(std::unique_ptr<AuthenticationProvider> provider)
    {
        if (!provider || !provider->validate_configuration()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        const std::string& provider_name = provider->get_provider_name();
        const AuthenticationMethod method = provider->get_authentication_method();

        // Check for duplicate provider names
        if (pimpl_->providers_.find(provider_name) != pimpl_->providers_.end()) {
            return false;
        }

        // Register provider
        pimpl_->providers_[provider_name] = std::move(provider);
        pimpl_->method_to_provider_[method] = provider_name;

        return true;
    }

    bool AuthenticationManager::unregister_provider(const std::string& provider_name)
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        auto it = pimpl_->providers_.find(provider_name);
        if (it == pimpl_->providers_.end()) {
            return false;
        }

        // Remove method mapping
        const AuthenticationMethod method = it->second->get_authentication_method();
        pimpl_->method_to_provider_.erase(method);

        // Remove provider
        pimpl_->providers_.erase(it);

        return true;
    }

    std::vector<std::string> AuthenticationManager::get_registered_providers() const
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        std::vector<std::string> provider_names;
        provider_names.reserve(pimpl_->providers_.size());

        for (const auto& [name, provider] : pimpl_->providers_) {
            provider_names.push_back(name);
        }

        return provider_names;
    }

    AuthenticationProvider* AuthenticationManager::get_provider(const std::string& provider_name)
    {
        std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

        auto it = pimpl_->providers_.find(provider_name);
        return (it != pimpl_->providers_.end()) ? it->second.get() : nullptr;
    }

    AuthenticationProvider* AuthenticationManager::get_provider(AuthenticationMethod method)
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

    AuthenticationResult
    AuthenticationManager::authenticate_user(AuthenticationContext& context,
                                             const std::string& preferred_provider)
    {

        // Check if account is locked
        if (is_account_locked(context.get_username())) {
            log_authentication_event("AUTHENTICATION_FAILED", context,
                                     AuthenticationResult::AccountLocked);

            std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
            pimpl_->stats_.failed_authentications++;

            return AuthenticationResult::AccountLocked;
        }

        AuthenticationProvider* provider = nullptr;

        {
            std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);

            // Try preferred provider first
            if (!preferred_provider.empty()) {
                auto it = pimpl_->providers_.find(preferred_provider);
                if (it != pimpl_->providers_.end()) {
                    provider = it->second.get();
                }
            }

            // Fall back to first available provider if no preferred provider
            if (!provider && !pimpl_->providers_.empty()) {
                provider = pimpl_->providers_.begin()->second.get();
            }
        }

        if (!provider) {
            log_authentication_event("NO_PROVIDER", context, AuthenticationResult::InternalError);

            std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
            pimpl_->stats_.failed_authentications++;

            return AuthenticationResult::InternalError;
        }

        // Attempt authentication
        AuthenticationResult result = provider->authenticate(context);

        if (result == AuthenticationResult::Success) {
            // Clear failed attempts on successful authentication
            clear_failed_attempts(context.get_username());
            context.set_authenticated(true);

            // Create session
            std::string session_id = pimpl_->generate_session_id();
            context.set_credential("session_id", session_id);

            {
                std::lock_guard<std::mutex> lock(pimpl_->sessions_mutex_);
                AuthenticationManager::Impl::SessionInfo session_info;
                session_info.username = context.get_username();
                session_info.database = context.get_database();
                session_info.created = std::chrono::system_clock::now();
                session_info.last_activity = session_info.created;
                session_info.auth_method = provider->get_authentication_method();

                pimpl_->active_sessions_[session_id] = session_info;
            }

            log_authentication_event("AUTHENTICATION_SUCCESS", context, result);

            std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
            pimpl_->stats_.successful_authentications++;
            pimpl_->stats_.active_sessions++;
        } else {
            // Record failed attempt
            record_failed_attempt(context.get_username());
            log_authentication_event("AUTHENTICATION_FAILED", context, result);

            std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
            pimpl_->stats_.failed_authentications++;
        }

        return result;
    }

    std::unique_ptr<AuthenticationChallenge>
    AuthenticationManager::initiate_challenge(const std::string& username,
                                              AuthenticationMethod method)
    {

        AuthenticationProvider* provider = get_provider(method);
        if (!provider || !provider->supports_challenge_response()) {
            return nullptr;
        }

        AuthenticationContext temp_context;
        temp_context.set_username(username);

        auto challenge = provider->create_challenge(temp_context);
        if (challenge) {
            std::lock_guard<std::mutex> lock(pimpl_->challenges_mutex_);
            const std::string& challenge_id = challenge->get_challenge_id();
            pimpl_->active_challenges_[challenge_id] =
                std::make_unique<AuthenticationChallenge>(*challenge);
        }

        return challenge;
    }

    AuthenticationResult
    AuthenticationManager::complete_challenge(const AuthenticationChallenge& challenge,
                                              AuthenticationContext& context)
    {

        // Find and validate challenge
        {
            std::lock_guard<std::mutex> lock(pimpl_->challenges_mutex_);
            auto it = pimpl_->active_challenges_.find(challenge.get_challenge_id());
            if (it == pimpl_->active_challenges_.end() || it->second->is_expired()) {
                return AuthenticationResult::Timeout;
            }

            // Remove used challenge
            pimpl_->active_challenges_.erase(it);
        }

        AuthenticationProvider* provider = get_provider(challenge.get_method());
        if (!provider) {
            return AuthenticationResult::InternalError;
        }

        return provider->validate_challenge_response(challenge, context);
    }

    bool AuthenticationManager::validate_session(const std::string& session_id)
    {
        std::lock_guard<std::mutex> lock(pimpl_->sessions_mutex_);

        auto it = pimpl_->active_sessions_.find(session_id);
        if (it == pimpl_->active_sessions_.end() || !it->second.is_valid) {
            return false;
        }

        // Check session timeout
        auto now = std::chrono::system_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_activity);

        if (elapsed > session_timeout_) {
            it->second.is_valid = false;
            return false;
        }

        // Update last activity
        it->second.last_activity = now;
        return true;
    }

    void AuthenticationManager::invalidate_session(const std::string& session_id)
    {
        std::lock_guard<std::mutex> lock(pimpl_->sessions_mutex_);

        auto it = pimpl_->active_sessions_.find(session_id);
        if (it != pimpl_->active_sessions_.end()) {
            it->second.is_valid = false;

            std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
            if (pimpl_->stats_.active_sessions > 0) {
                pimpl_->stats_.active_sessions--;
            }
        }
    }

    void AuthenticationManager::cleanup_expired_sessions()
    {
        std::lock_guard<std::mutex> lock(pimpl_->sessions_mutex_);

        auto now = std::chrono::system_clock::now();

        for (auto it = pimpl_->active_sessions_.begin(); it != pimpl_->active_sessions_.end();) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_activity);

            if (elapsed > session_timeout_ || !it->second.is_valid) {
                it = pimpl_->active_sessions_.erase(it);

                std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
                if (pimpl_->stats_.active_sessions > 0) {
                    pimpl_->stats_.active_sessions--;
                }
            } else {
                ++it;
            }
        }

        // Also cleanup expired challenges
        {
            std::lock_guard<std::mutex> challenges_lock(pimpl_->challenges_mutex_);
            for (auto it = pimpl_->active_challenges_.begin();
                 it != pimpl_->active_challenges_.end();) {
                if (it->second->is_expired()) {
                    it = pimpl_->active_challenges_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    AuthenticationResult AuthenticationManager::change_user_password(
        const std::string& username, const std::string& old_password,
        const std::string& new_password, const std::string& provider_name)
    {

        AuthenticationProvider* provider = nullptr;

        if (!provider_name.empty()) {
            provider = get_provider(provider_name);
        } else {
            // Use first provider that supports password management
            std::lock_guard<std::mutex> lock(pimpl_->providers_mutex_);
            for (const auto& [name, prov] : pimpl_->providers_) {
                if (prov->supports_password_management()) {
                    provider = prov.get();
                    break;
                }
            }
        }

        if (!provider || !provider->supports_password_management()) {
            return AuthenticationResult::InternalError;
        }

        return provider->change_password(username, old_password, new_password);
    }

    AuthenticationResult AuthenticationManager::lock_user_account(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->accounts_mutex_);

        pimpl_->account_info_[username].is_locked = true;
        pimpl_->account_info_[username].locked_until =
            std::chrono::system_clock::now() + lockout_duration_;

        std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
        pimpl_->stats_.locked_accounts++;

        return AuthenticationResult::Success;
    }

    AuthenticationResult AuthenticationManager::unlock_user_account(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->accounts_mutex_);

        auto it = pimpl_->account_info_.find(username);
        if (it != pimpl_->account_info_.end()) {
            it->second.is_locked = false;
            it->second.failed_attempts = 0;
            it->second.locked_until = std::chrono::system_clock::time_point{};

            std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
            if (pimpl_->stats_.locked_accounts > 0) {
                pimpl_->stats_.locked_accounts--;
            }
        }

        return AuthenticationResult::Success;
    }

    std::vector<std::string>
    AuthenticationManager::get_audit_events(const std::string& username,
                                            std::chrono::system_clock::time_point since) const
    {

        std::lock_guard<std::mutex> lock(pimpl_->audit_mutex_);

        std::vector<std::string> events;

        for (const auto& event : pimpl_->audit_log_) {
            if (event.timestamp < since)
                continue;
            if (!username.empty() && event.username != username)
                continue;

            std::stringstream ss;
            auto time_t = std::chrono::system_clock::to_time_t(event.timestamp);
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            ss << " [" << event.event_type << "] ";
            ss << "User: " << event.username;
            ss << " Result: " << to_string(event.result);
            if (!event.remote_address.empty()) {
                ss << " From: " << event.remote_address;
            }
            if (!event.details.empty()) {
                ss << " Details: " << event.details;
            }

            events.push_back(ss.str());
        }

        return events;
    }

    AuthenticationManager::AuthenticationStats AuthenticationManager::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
        return pimpl_->stats_;
    }

    void AuthenticationManager::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
        pimpl_->stats_ = AuthenticationStats{};
        pimpl_->stats_.last_reset = std::chrono::system_clock::now();
    }

    // Private helper methods
    bool AuthenticationManager::is_account_locked(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->accounts_mutex_);

        auto it = pimpl_->account_info_.find(username);
        if (it == pimpl_->account_info_.end()) {
            return false;
        }

        if (!it->second.is_locked) {
            return false;
        }

        // Check if lockout has expired
        auto now = std::chrono::system_clock::now();
        if (now >= it->second.locked_until) {
            it->second.is_locked = false;
            it->second.failed_attempts = 0;

            std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
            if (pimpl_->stats_.locked_accounts > 0) {
                pimpl_->stats_.locked_accounts--;
            }

            return false;
        }

        return true;
    }

    void AuthenticationManager::record_failed_attempt(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->accounts_mutex_);

        auto& account_info = pimpl_->account_info_[username];
        account_info.failed_attempts++;
        account_info.last_failure = std::chrono::system_clock::now();

        // Lock account if too many failed attempts
        if (account_info.failed_attempts >= failed_attempt_limit_) {
            account_info.is_locked = true;
            account_info.locked_until = std::chrono::system_clock::now() + lockout_duration_;

            std::lock_guard<std::mutex> stats_lock(pimpl_->stats_mutex_);
            pimpl_->stats_.locked_accounts++;
        }
    }

    void AuthenticationManager::clear_failed_attempts(const std::string& username)
    {
        std::lock_guard<std::mutex> lock(pimpl_->accounts_mutex_);

        auto it = pimpl_->account_info_.find(username);
        if (it != pimpl_->account_info_.end()) {
            it->second.failed_attempts = 0;
        }
    }

    void AuthenticationManager::log_authentication_event(const std::string& event,
                                                         const AuthenticationContext& context,
                                                         AuthenticationResult result)
    {

        if (!audit_logging_enabled_)
            return;

        std::lock_guard<std::mutex> lock(pimpl_->audit_mutex_);

        AuthenticationManager::Impl::AuditEvent audit_event;
        audit_event.event_type = event;
        audit_event.username = context.get_username();
        audit_event.remote_address = context.get_remote_address();
        audit_event.result = result;
        audit_event.timestamp = std::chrono::system_clock::now();
        audit_event.details = context.get_client_info();

        pimpl_->audit_log_.push_back(audit_event);

        // Keep audit log size manageable (keep last 10000 events)
        if (pimpl_->audit_log_.size() > 10000) {
            pimpl_->audit_log_.erase(pimpl_->audit_log_.begin(), pimpl_->audit_log_.begin() + 1000);
        }
    }

} // namespace ScratchBird
