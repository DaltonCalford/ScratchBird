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
// P3-13: View Security Context
//
// Implements SECURITY DEFINER vs SECURITY INVOKER semantics for views.
//
// SECURITY DEFINER: View executes with the privileges of the view owner
//   - Useful for providing controlled access to underlying tables
//   - User doesn't need direct access to base tables
//   - Similar to PostgreSQL's security_barrier views
//
// SECURITY INVOKER (default): View executes with the privileges of the caller
//   - Standard SQL behavior
//   - User must have access to base tables
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <string_view>
#include <cstdint>
#include <optional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <mutex>

namespace scratchbird::security {

// Forward declarations
class SecurityContext;
class PermissionChecker;

// ============================================================================
// View Security Mode
// ============================================================================

enum class ViewSecurityMode : uint8_t {
    INVOKER = 0,      // Execute with caller's privileges (default)
    DEFINER = 1,      // Execute with view owner's privileges
};

// ============================================================================
// View Security Options
// ============================================================================

struct ViewSecurityOptions {
    ViewSecurityMode mode = ViewSecurityMode::INVOKER;
    uint32_t owner_id = 0;                    // View owner user ID
    bool security_barrier = false;            // Prevent optimizer from pushing conditions
    bool check_option = false;                // WITH CHECK OPTION for updatable views
    bool local_check_only = false;            // LOCAL vs CASCADED check option

    // Create default (INVOKER) options
    static ViewSecurityOptions defaultOptions(uint32_t owner_id) {
        ViewSecurityOptions opts;
        opts.owner_id = owner_id;
        return opts;
    }

    // Create SECURITY DEFINER options
    static ViewSecurityOptions definerOptions(uint32_t owner_id) {
        ViewSecurityOptions opts;
        opts.mode = ViewSecurityMode::DEFINER;
        opts.owner_id = owner_id;
        return opts;
    }

    // Create security barrier options
    static ViewSecurityOptions barrierOptions(uint32_t owner_id) {
        ViewSecurityOptions opts;
        opts.mode = ViewSecurityMode::DEFINER;
        opts.owner_id = owner_id;
        opts.security_barrier = true;
        return opts;
    }
};

// ============================================================================
// View Security Context
// ============================================================================

// Represents the effective security context when executing a view
class ViewSecurityContext {
public:
    ViewSecurityContext() = default;

    // Create context for a view execution
    ViewSecurityContext(
        uint32_t caller_id,
        uint32_t effective_id,
        const ViewSecurityOptions& options);

    // Get the caller's user ID (who invoked the view)
    uint32_t callerId() const { return caller_id_; }

    // Get the effective user ID for permission checks
    uint32_t effectiveUserId() const { return effective_user_id_; }

    // Get view options
    const ViewSecurityOptions& options() const { return options_; }

    // Check if using definer privileges
    bool isDefiner() const { return options_.mode == ViewSecurityMode::DEFINER; }

    // Check if security barrier is enabled
    bool hasSecurityBarrier() const { return options_.security_barrier; }

    // Check if this is an updatable view with check option
    bool hasCheckOption() const { return options_.check_option; }

private:
    uint32_t caller_id_ = 0;
    uint32_t effective_user_id_ = 0;
    ViewSecurityOptions options_;
};

// ============================================================================
// View Security Stack
// ============================================================================

// Manages nested view security contexts (for views that reference other views)
class ViewSecurityStack {
public:
    // Push a new view context onto the stack
    void push(const ViewSecurityContext& ctx);

    // Pop the top context
    void pop();

    // Get current (innermost) context
    const ViewSecurityContext* current() const;

    // Get effective user ID for permission checks
    // This considers the full stack of nested views
    uint32_t effectiveUserId() const;

    // Check if any view in stack has security barrier
    bool hasSecurityBarrier() const;

    // Get stack depth
    size_t depth() const { return stack_.size(); }

    // Check if stack is empty
    bool empty() const { return stack_.empty(); }

    // Clear all contexts (e.g., at end of query)
    void clear();

private:
    std::vector<ViewSecurityContext> stack_;
};

// ============================================================================
// View Security Manager
// ============================================================================

class ViewSecurityManager {
public:
    static ViewSecurityManager& getInstance();

    // ========================================================================
    // View Registration
    // ========================================================================

    // Register a view with its security options
    core::Status registerView(
        uint32_t view_id,
        const ViewSecurityOptions& options,
        core::ErrorContext* ctx = nullptr);

    // Unregister a view
    void unregisterView(uint32_t view_id);

    // Update view security options
    core::Status updateViewSecurity(
        uint32_t view_id,
        const ViewSecurityOptions& options,
        core::ErrorContext* ctx = nullptr);

    // Get view security options
    std::optional<ViewSecurityOptions> getViewOptions(uint32_t view_id) const;

    // ========================================================================
    // Execution Context Management
    // ========================================================================

    // Enter a view's security context for execution
    // Returns the security context to use for permission checks
    ViewSecurityContext enterView(
        uint32_t view_id,
        uint32_t caller_id);

    // Exit a view's security context
    void exitView(uint32_t view_id);

    // Get current security stack for the calling thread
    ViewSecurityStack& currentStack();

    // ========================================================================
    // Permission Checking
    // ========================================================================

    // Check if current context allows access to a table
    core::Status checkTableAccess(
        uint32_t table_id,
        uint32_t required_privileges,
        core::ErrorContext* ctx = nullptr);

    // Check if current context allows a column access
    core::Status checkColumnAccess(
        uint32_t table_id,
        uint32_t column_id,
        uint32_t required_privileges,
        core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Security Barrier Support
    // ========================================================================

    // Check if predicate pushdown is allowed through security barriers
    bool canPushPredicate(uint32_t view_id) const;

    // Mark a subquery as requiring security barrier isolation
    void markSecurityBarrier(uint32_t subquery_id);

    // ========================================================================
    // WITH CHECK OPTION Support
    // ========================================================================

    // Validate a row modification against check option constraints
    core::Status validateCheckOption(
        uint32_t view_id,
        const void* row_data,  // Would be actual row type
        core::ErrorContext* ctx = nullptr);

    // Get check option mode for a view
    enum class CheckOptionMode : uint8_t {
        NONE,
        LOCAL,      // Only check this view's WHERE clause
        CASCADED,   // Check this and all underlying views
    };
    CheckOptionMode getCheckOptionMode(uint32_t view_id) const;

private:
    ViewSecurityManager() = default;

    // View options by view ID
    std::unordered_map<uint32_t, ViewSecurityOptions> view_options_;

    // Per-thread security stacks
    thread_local static ViewSecurityStack current_stack_;

    // Security barrier markers
    std::unordered_set<uint32_t> security_barrier_subqueries_;

    mutable std::shared_mutex mutex_;
};

// ============================================================================
// RAII View Context Guard
// ============================================================================

// Automatically manages view security context entry/exit
class ViewSecurityGuard {
public:
    ViewSecurityGuard(uint32_t view_id, uint32_t caller_id)
        : view_id_(view_id), entered_(false) {
        context_ = ViewSecurityManager::getInstance().enterView(view_id, caller_id);
        entered_ = true;
    }

    ~ViewSecurityGuard() {
        if (entered_) {
            ViewSecurityManager::getInstance().exitView(view_id_);
        }
    }

    // Non-copyable
    ViewSecurityGuard(const ViewSecurityGuard&) = delete;
    ViewSecurityGuard& operator=(const ViewSecurityGuard&) = delete;

    // Movable
    ViewSecurityGuard(ViewSecurityGuard&& other) noexcept
        : view_id_(other.view_id_), context_(other.context_), entered_(other.entered_) {
        other.entered_ = false;
    }

    ViewSecurityGuard& operator=(ViewSecurityGuard&& other) noexcept {
        if (this != &other) {
            if (entered_) {
                ViewSecurityManager::getInstance().exitView(view_id_);
            }
            view_id_ = other.view_id_;
            context_ = other.context_;
            entered_ = other.entered_;
            other.entered_ = false;
        }
        return *this;
    }

    // Get the context
    const ViewSecurityContext& context() const { return context_; }

    // Get effective user ID
    uint32_t effectiveUserId() const { return context_.effectiveUserId(); }

private:
    uint32_t view_id_;
    ViewSecurityContext context_;
    bool entered_;
};

// ============================================================================
// SQL Syntax Support
// ============================================================================

// Parse security options from CREATE VIEW statement
// CREATE VIEW ... WITH (security_barrier = true, security_definer = true)
// or
// CREATE VIEW ... SECURITY DEFINER
struct ViewSecurityParser {
    // Parse SECURITY INVOKER/DEFINER clause
    static ViewSecurityMode parseSecurityMode(std::string_view clause);

    // Parse WITH options
    static ViewSecurityOptions parseWithOptions(
        const std::vector<std::pair<std::string, std::string>>& options,
        uint32_t owner_id);

    // Generate SQL for security options
    static std::string toSQL(const ViewSecurityOptions& options);
};

} // namespace scratchbird::security
