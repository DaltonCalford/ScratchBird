/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/*
 * ScratchBird Database Engine
 * Git Repository Operations
 * Copyright (c) 2025 ScratchBird Project
 */
#pragma once

#include "GitTypes.h"
#include <memory>
#include <mutex>

namespace scratchbird {
namespace git {

/**
 * GitRepository provides Git operations using libgit2.
 *
 * Handles:
 * - Repository initialization and cloning
 * - Branch management
 * - Commit, push, and pull operations
 * - Status and diff queries
 * - Authentication (SSH, HTTPS)
 */
class GitRepository {
public:
    /**
     * Constructor
     * @param config Git configuration
     */
    explicit GitRepository(const GitConfig& config);
    ~GitRepository();

    // Non-copyable
    GitRepository(const GitRepository&) = delete;
    GitRepository& operator=(const GitRepository&) = delete;

    //=========================================================================
    // Repository Lifecycle
    //=========================================================================

    /**
     * Initialize a new local repository
     * @return true on success
     */
    bool init();

    /**
     * Clone a remote repository
     * @param progress Optional progress callback
     * @return true on success
     */
    bool clone(ProgressCallback progress = nullptr);

    /**
     * Open an existing local repository
     * @return true on success
     */
    bool open();

    /**
     * Close the repository
     */
    void close();

    /**
     * Check if repository is open
     */
    bool isOpen() const;

    /**
     * Get current repository state
     */
    RepositoryState getState() const;

    //=========================================================================
    // Remote Operations
    //=========================================================================

    /**
     * Fetch updates from remote
     * @param progress Optional progress callback
     * @return true on success
     */
    bool fetch(ProgressCallback progress = nullptr);

    /**
     * Pull changes from remote (fetch + merge)
     * @param progress Optional progress callback
     * @return true on success
     */
    bool pull(ProgressCallback progress = nullptr);

    /**
     * Push local commits to remote
     * @param progress Optional progress callback
     * @return true on success
     */
    bool push(ProgressCallback progress = nullptr);

    /**
     * Test connection to remote
     * @return true if reachable
     */
    bool testConnection();

    //=========================================================================
    // Branch Operations
    //=========================================================================

    /**
     * Get current branch name
     */
    std::string getCurrentBranch() const;

    /**
     * List all branches
     * @param include_remote Include remote branches
     * @return List of branch info
     */
    std::vector<GitBranch> listBranches(bool include_remote = false) const;

    /**
     * Create a new branch
     * @param name Branch name
     * @param checkout Switch to new branch
     * @return true on success
     */
    bool createBranch(const std::string& name, bool checkout = true);

    /**
     * Switch to a branch
     * @param name Branch name
     * @return true on success
     */
    bool checkout(const std::string& name);

    /**
     * Delete a branch
     * @param name Branch name
     * @param force Force delete even if not merged
     * @return true on success
     */
    bool deleteBranch(const std::string& name, bool force = false);

    /**
     * Merge a branch into current
     * @param name Branch name to merge
     * @return true on success (false if conflicts)
     */
    bool merge(const std::string& name);

    //=========================================================================
    // Commit Operations
    //=========================================================================

    /**
     * Stage files for commit
     * @param paths File paths to stage (empty = all)
     * @return true on success
     */
    bool stage(const std::vector<std::string>& paths = {});

    /**
     * Unstage files
     * @param paths File paths to unstage (empty = all)
     * @return true on success
     */
    bool unstage(const std::vector<std::string>& paths = {});

    /**
     * Create a commit with staged changes
     * @param message Commit message
     * @param author Optional author (uses config if empty)
     * @return Commit SHA on success, empty on failure
     */
    std::string commit(const std::string& message,
                       const std::string& author = "");

    /**
     * Get commit history
     * @param count Number of commits (0 = all)
     * @param path Optional path filter
     * @return List of commits
     */
    std::vector<GitCommit> getHistory(int count = 10,
                                       const std::string& path = "") const;

    /**
     * Get commit by SHA
     * @param sha Full or partial SHA
     * @return Commit info or nullopt
     */
    std::optional<GitCommit> getCommit(const std::string& sha) const;

    //=========================================================================
    // Status and Diff
    //=========================================================================

    /**
     * Get repository status
     * @return Current status
     */
    GitStatus getStatus() const;

    /**
     * Check if working directory is clean
     */
    bool isClean() const;

    /**
     * Check if there are uncommitted changes
     */
    bool hasUncommittedChanges() const;

    /**
     * Get diff between working directory and HEAD
     * @param path Optional path filter
     * @return Unified diff string
     */
    std::string getDiff(const std::string& path = "") const;

    /**
     * Get diff between two commits
     * @param from_sha Source commit
     * @param to_sha Target commit
     * @return Unified diff string
     */
    std::string getDiffBetween(const std::string& from_sha,
                                const std::string& to_sha) const;

    /**
     * Get changed files between two commits
     * @param from_sha Source commit
     * @param to_sha Target commit
     * @return List of changed file paths
     */
    std::vector<std::string> getChangedFiles(const std::string& from_sha,
                                              const std::string& to_sha) const;

    //=========================================================================
    // File Operations
    //=========================================================================

    /**
     * Read file content at specific commit
     * @param path File path
     * @param commit_sha Commit SHA (empty = HEAD)
     * @return File content or nullopt
     */
    std::optional<std::string> readFile(const std::string& path,
                                         const std::string& commit_sha = "") const;

    /**
     * Write file to working directory
     * @param path File path
     * @param content File content
     * @return true on success
     */
    bool writeFile(const std::string& path, const std::string& content);

    /**
     * Delete file from working directory
     * @param path File path
     * @return true on success
     */
    bool deleteFile(const std::string& path);

    /**
     * Check if file exists
     * @param path File path
     * @param commit_sha Commit SHA (empty = working dir)
     */
    bool fileExists(const std::string& path,
                    const std::string& commit_sha = "") const;

    /**
     * List files in directory
     * @param path Directory path
     * @param recursive Include subdirectories
     * @return List of file paths
     */
    std::vector<std::string> listFiles(const std::string& path = "",
                                        bool recursive = false) const;

    //=========================================================================
    // Reset and Revert
    //=========================================================================

    /**
     * Discard changes in working directory
     * @param paths File paths (empty = all)
     * @return true on success
     */
    bool discardChanges(const std::vector<std::string>& paths = {});

    /**
     * Reset to a specific commit
     * @param commit_sha Commit SHA
     * @param hard Hard reset (discard all changes)
     * @return true on success
     */
    bool reset(const std::string& commit_sha, bool hard = false);

    /**
     * Revert a specific commit
     * @param commit_sha Commit SHA to revert
     * @return New commit SHA on success
     */
    std::string revert(const std::string& commit_sha);

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Get configuration
     */
    const GitConfig& getConfig() const { return config_; }

    /**
     * Update configuration
     */
    void setConfig(const GitConfig& config);

    /**
     * Get local repository path
     */
    std::string getLocalPath() const;

    /**
     * Get remote URL
     */
    std::string getRemoteUrl() const;

    /**
     * Set log callback for debugging
     */
    void setLogCallback(LogCallback callback);

    /**
     * Get last error message
     */
    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    GitConfig config_;
    mutable std::mutex mutex_;
    LogCallback log_callback_;
    std::string last_error_;
    RepositoryState state_ = RepositoryState::NOT_INITIALIZED;

    void setError(const std::string& error);
    void log(const std::string& level, const std::string& message);
    bool setupCredentials();
    bool validateConfig();
};

} // namespace git
} // namespace scratchbird
