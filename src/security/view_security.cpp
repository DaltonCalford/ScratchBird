/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-13: View Security Context Implementation
//
// November 25, 2025

#include "scratchbird/security/view_security.h"
#include <algorithm>
#include <sstream>
#include <mutex>

namespace scratchbird::security {

// Thread-local security stack
thread_local ViewSecurityStack ViewSecurityManager::current_stack_;

namespace {

auto setViewSecurityError(core::ErrorContext* ctx,
                          core::Status status,
                          const std::string& message) -> core::Status {
    if (ctx) {
        ctx->code = status;
        ctx->message = message;
    }
    return status;
}

}  // namespace

// ============================================================================
// ViewSecurityContext Implementation
// ============================================================================

ViewSecurityContext::ViewSecurityContext(
    uint32_t caller_id,
    uint32_t effective_id,
    const ViewSecurityOptions& options)
    : caller_id_(caller_id)
    , effective_user_id_(effective_id)
    , options_(options) {
}

// ============================================================================
// ViewSecurityStack Implementation
// ============================================================================

void ViewSecurityStack::push(const ViewSecurityContext& ctx) {
    stack_.push_back(ctx);
}

void ViewSecurityStack::pop() {
    if (!stack_.empty()) {
        stack_.pop_back();
    }
}

const ViewSecurityContext* ViewSecurityStack::current() const {
    return stack_.empty() ? nullptr : &stack_.back();
}

uint32_t ViewSecurityStack::effectiveUserId() const {
    if (stack_.empty()) {
        return 0;  // No view context, use original caller
    }

    // Walk stack from innermost to outermost
    // Return the first DEFINER context's owner, or the caller if all INVOKER
    for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
        if (it->isDefiner()) {
            return it->effectiveUserId();
        }
    }

    // All INVOKER contexts - use caller
    return stack_.back().callerId();
}

bool ViewSecurityStack::hasSecurityBarrier() const {
    for (const auto& ctx : stack_) {
        if (ctx.hasSecurityBarrier()) {
            return true;
        }
    }
    return false;
}

void ViewSecurityStack::clear() {
    stack_.clear();
}

// ============================================================================
// ViewSecurityManager Implementation
// ============================================================================

ViewSecurityManager& ViewSecurityManager::getInstance() {
    static ViewSecurityManager instance;
    return instance;
}

core::Status ViewSecurityManager::registerView(
    uint32_t view_id,
    const ViewSecurityOptions& options,
    core::ErrorContext* ctx) {

    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (view_options_.find(view_id) != view_options_.end()) {
        if (ctx) {
            ctx->message = "View already registered: " + std::to_string(view_id);
            ctx->code = core::Status::CONSTRAINT_VIOLATION;
        }
        return core::Status::CONSTRAINT_VIOLATION;
    }

    view_options_[view_id] = options;
    return core::Status::OK;
}

void ViewSecurityManager::unregisterView(uint32_t view_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    view_options_.erase(view_id);
}

core::Status ViewSecurityManager::updateViewSecurity(
    uint32_t view_id,
    const ViewSecurityOptions& options,
    core::ErrorContext* ctx) {

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = view_options_.find(view_id);
    if (it == view_options_.end()) {
        if (ctx) {
            ctx->message = "View not found: " + std::to_string(view_id);
            ctx->code = core::Status::NOT_FOUND;
        }
        return core::Status::NOT_FOUND;
    }

    it->second = options;
    return core::Status::OK;
}

std::optional<ViewSecurityOptions> ViewSecurityManager::getViewOptions(uint32_t view_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = view_options_.find(view_id);
    if (it != view_options_.end()) {
        return it->second;
    }
    return std::nullopt;
}

ViewSecurityContext ViewSecurityManager::enterView(
    uint32_t view_id,
    uint32_t caller_id) {

    std::shared_lock<std::shared_mutex> lock(mutex_);

    ViewSecurityOptions options;
    auto it = view_options_.find(view_id);
    if (it != view_options_.end()) {
        options = it->second;
    }

    // Determine effective user ID
    uint32_t effective_id = caller_id;
    if (options.mode == ViewSecurityMode::DEFINER) {
        effective_id = options.owner_id;
    }

    ViewSecurityContext ctx(caller_id, effective_id, options);
    current_stack_.push(ctx);

    return ctx;
}

void ViewSecurityManager::exitView(uint32_t /*view_id*/) {
    current_stack_.pop();
}

ViewSecurityStack& ViewSecurityManager::currentStack() {
    return current_stack_;
}

core::Status ViewSecurityManager::checkTableAccess(
    uint32_t table_id,
    uint32_t required_privileges,
    core::ErrorContext* ctx) {
    if (table_id == 0 || required_privileges == 0) {
        return setViewSecurityError(
            ctx,
            core::Status::INVALID_ARGUMENT,
            "View security table access requires non-zero table and privilege ids");
    }

    const ViewSecurityContext* current = current_stack_.current();
    if (current == nullptr) {
        return core::Status::OK;
    }

    // SECURITY INVOKER does not substitute privileges. The caller's normal
    // permission path remains authoritative.
    if (!current->isDefiner()) {
        return core::Status::OK;
    }

    if (current->effectiveUserId() == 0) {
        return setViewSecurityError(
            ctx,
            core::Status::PERMISSION_DENIED,
            "SECURITY DEFINER view has no effective owner identity");
    }

    return setViewSecurityError(
        ctx,
        core::Status::PERMISSION_DENIED,
        "SECURITY DEFINER table access requires an integrated permission backend");
}

core::Status ViewSecurityManager::checkColumnAccess(
    uint32_t table_id,
    uint32_t column_id,
    uint32_t required_privileges,
    core::ErrorContext* ctx) {
    if (table_id == 0 || column_id == 0 || required_privileges == 0) {
        return setViewSecurityError(
            ctx,
            core::Status::INVALID_ARGUMENT,
            "View security column access requires non-zero table column and privilege ids");
    }

    const ViewSecurityContext* current = current_stack_.current();
    if (current == nullptr) {
        return core::Status::OK;
    }

    if (!current->isDefiner()) {
        return core::Status::OK;
    }

    if (current->effectiveUserId() == 0) {
        return setViewSecurityError(
            ctx,
            core::Status::PERMISSION_DENIED,
            "SECURITY DEFINER view has no effective owner identity");
    }

    return setViewSecurityError(
        ctx,
        core::Status::PERMISSION_DENIED,
        "SECURITY DEFINER column access requires an integrated permission backend");
}

bool ViewSecurityManager::canPushPredicate(uint32_t view_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = view_options_.find(view_id);
    if (it != view_options_.end()) {
        // Cannot push predicates through security barrier
        return !it->second.security_barrier;
    }

    return true;  // No special restrictions
}

void ViewSecurityManager::markSecurityBarrier(uint32_t subquery_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    security_barrier_subqueries_.insert(subquery_id);
}

core::Status ViewSecurityManager::validateCheckOption(
    uint32_t view_id,
    const void* row_data,
    core::ErrorContext* ctx) {

    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = view_options_.find(view_id);
    if (it == view_options_.end() || !it->second.check_option) {
        return core::Status::OK;  // No check option
    }

    if (row_data == nullptr) {
        return setViewSecurityError(
            ctx,
            core::Status::INVALID_ARGUMENT,
            "WITH CHECK OPTION validation requires row data");
    }

    return setViewSecurityError(
        ctx,
        core::Status::PERMISSION_DENIED,
        "WITH CHECK OPTION requires an integrated predicate evaluation backend");
}

ViewSecurityManager::CheckOptionMode ViewSecurityManager::getCheckOptionMode(uint32_t view_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = view_options_.find(view_id);
    if (it == view_options_.end() || !it->second.check_option) {
        return CheckOptionMode::NONE;
    }

    return it->second.local_check_only ? CheckOptionMode::LOCAL : CheckOptionMode::CASCADED;
}

// ============================================================================
// ViewSecurityParser Implementation
// ============================================================================

ViewSecurityMode ViewSecurityParser::parseSecurityMode(std::string_view clause) {
    std::string upper(clause);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (upper == "DEFINER" || upper == "SECURITY DEFINER") {
        return ViewSecurityMode::DEFINER;
    }
    return ViewSecurityMode::INVOKER;
}

ViewSecurityOptions ViewSecurityParser::parseWithOptions(
    const std::vector<std::pair<std::string, std::string>>& options,
    uint32_t owner_id) {

    ViewSecurityOptions result;
    result.owner_id = owner_id;

    for (const auto& [key, value] : options) {
        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::string lower_value = value;
        std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower_key == "security_definer") {
            if (lower_value == "true" || lower_value == "1" || lower_value == "on") {
                result.mode = ViewSecurityMode::DEFINER;
            }
        } else if (lower_key == "security_barrier") {
            if (lower_value == "true" || lower_value == "1" || lower_value == "on") {
                result.security_barrier = true;
            }
        } else if (lower_key == "check_option") {
            if (lower_value == "local") {
                result.check_option = true;
                result.local_check_only = true;
            } else if (lower_value == "cascaded" || lower_value == "true") {
                result.check_option = true;
                result.local_check_only = false;
            }
        }
    }

    return result;
}

std::string ViewSecurityParser::toSQL(const ViewSecurityOptions& options) {
    std::stringstream ss;
    std::vector<std::string> parts;

    if (options.mode == ViewSecurityMode::DEFINER) {
        parts.push_back("security_definer = true");
    }

    if (options.security_barrier) {
        parts.push_back("security_barrier = true");
    }

    if (options.check_option) {
        if (options.local_check_only) {
            parts.push_back("check_option = local");
        } else {
            parts.push_back("check_option = cascaded");
        }
    }

    if (!parts.empty()) {
        ss << "WITH (";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << parts[i];
        }
        ss << ")";
    }

    return ss.str();
}

} // namespace scratchbird::security
