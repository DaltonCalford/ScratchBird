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
 * Git Repository Operations Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/GitRepository.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/sha.h>
#if !defined(_WIN32)
    #include <sys/wait.h>
#endif

// Note: In production, this would use libgit2
// For now, we implement using git CLI commands as a fallback

namespace scratchbird {
namespace git {

namespace fs = std::filesystem;

namespace {
#ifdef _WIN32
inline auto runPipeOpen(const char* command, const char* mode) -> FILE* {
    return _popen(command, mode);
}

inline auto runPipeClose(FILE* pipe) -> int {
    return _pclose(pipe);
}

inline auto didCommandSucceed(int status) -> bool {
    return status == 0;
}

inline auto setEnvironmentValue(const char* key, const char* value) -> void {
    (void)_putenv_s(key, value);
}
#else
inline auto runPipeOpen(const char* command, const char* mode) -> FILE* {
    return popen(command, mode);
}

inline auto runPipeClose(FILE* pipe) -> int {
    return pclose(pipe);
}

inline auto didCommandSucceed(int status) -> bool {
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

inline auto setEnvironmentValue(const char* key, const char* value) -> void {
    setenv(key, value, 1);
}
#endif
} // namespace

//=============================================================================
// Implementation Details
//=============================================================================

struct GitRepository::Impl {
    // In production: git_repository* repo = nullptr;
    // For now, use CLI-based implementation
    bool is_open = false;
    std::string local_path;

    // Execute git command and return output
    std::pair<bool, std::string> execGit(const std::string& args,
                                          const std::string& cwd = "") {
        std::string cmd = "git";
        if (!cwd.empty()) {
            cmd = "cd " + cwd + " && git";
        } else if (!local_path.empty()) {
            cmd = "cd " + local_path + " && git";
        }
        cmd += " " + args + " 2>&1";

        FILE* pipe = runPipeOpen(cmd.c_str(), "r");
        if (!pipe) {
            return {false, "Failed to execute git command"};
        }

        std::string result;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        int status = pclose(pipe);
        bool success = false;
#if defined(_WIN32)
        success = (status == 0);
#else
        success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif

        return {success, result};
    }
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

GitRepository::GitRepository(const GitConfig& config)
    : impl_(std::make_unique<Impl>())
    , config_(config) {
    if (config_.repo_path.empty()) {
        // Default local path based on URL
        if (!config_.repo_url.empty()) {
            auto pos = config_.repo_url.rfind('/');
            if (pos != std::string::npos) {
                auto name = config_.repo_url.substr(pos + 1);
                if (name.size() > 4 && name.substr(name.size() - 4) == ".git") {
                    name = name.substr(0, name.size() - 4);
                }
                auto base = fs::path("build") / "git";
                config_.repo_path = (base / name).string();
            }
        }
    }
    impl_->local_path = config_.repo_path;
}

GitRepository::~GitRepository() {
    close();
}

//=============================================================================
// Repository Lifecycle
//=============================================================================

bool GitRepository::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (impl_->local_path.empty()) {
        setError("Local path not specified");
        return false;
    }

    // Create directory if needed
    try {
        fs::create_directories(impl_->local_path);
    } catch (const fs::filesystem_error& e) {
        setError("Failed to create directory: " + std::string(e.what()));
        return false;
    }

    // Initialize git repo
    auto [success, output] = impl_->execGit("init");
    if (!success) {
        setError("Git init failed: " + output);
        return false;
    }

    // Set default branch if needed
    if (!config_.repo_branch.empty()) {
        impl_->execGit("checkout -b " + config_.repo_branch);
    }

    impl_->is_open = true;
    state_ = RepositoryState::CONNECTED;
    log("INFO", "Initialized repository at " + impl_->local_path);
    return true;
}

bool GitRepository::clone(ProgressCallback progress) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.repo_url.empty()) {
        setError("Repository URL not specified");
        return false;
    }

    if (impl_->local_path.empty()) {
        setError("Local path not specified");
        return false;
    }

    // Create parent directory
    try {
        fs::create_directories(fs::path(impl_->local_path).parent_path());
    } catch (const fs::filesystem_error& e) {
        setError("Failed to create directory: " + std::string(e.what()));
        return false;
    }

    if (progress) {
        progress(0, 100, "Cloning repository...");
    }

    // Build clone command
    std::string cmd = "clone";
    if (!config_.repo_branch.empty()) {
        cmd += " -b " + config_.repo_branch;
    }
    cmd += " " + config_.repo_url + " " + impl_->local_path;

    auto [success, output] = impl_->execGit(cmd, "/tmp");
    if (!success) {
        setError("Git clone failed: " + output);
        return false;
    }

    if (progress) {
        progress(100, 100, "Clone complete");
    }

    impl_->is_open = true;
    state_ = RepositoryState::CONNECTED;
    log("INFO", "Cloned repository to " + impl_->local_path);
    return true;
}

bool GitRepository::open() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (impl_->local_path.empty()) {
        setError("Local path not specified");
        return false;
    }

    // Check if .git exists
    if (!fs::exists(impl_->local_path + "/.git")) {
        setError("Not a git repository: " + impl_->local_path);
        state_ = RepositoryState::NOT_INITIALIZED;
        return false;
    }

    impl_->is_open = true;
    state_ = RepositoryState::CONNECTED;
    log("INFO", "Opened repository at " + impl_->local_path);
    return true;
}

void GitRepository::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->is_open = false;
    state_ = RepositoryState::DISCONNECTED;
}

bool GitRepository::isOpen() const {
    return impl_->is_open;
}

RepositoryState GitRepository::getState() const {
    return state_;
}

//=============================================================================
// Remote Operations
//=============================================================================

bool GitRepository::fetch(ProgressCallback progress) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    if (progress) {
        progress(0, 100, "Fetching from remote...");
    }

    auto [success, output] = impl_->execGit("fetch origin");
    if (!success) {
        setError("Git fetch failed: " + output);
        return false;
    }

    if (progress) {
        progress(100, 100, "Fetch complete");
    }

    log("INFO", "Fetched from remote");
    return true;
}

bool GitRepository::pull(ProgressCallback progress) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    if (progress) {
        progress(0, 100, "Pulling from remote...");
    }

    auto [success, output] = impl_->execGit("pull origin " + config_.repo_branch);
    if (!success) {
        if (output.find("CONFLICT") != std::string::npos) {
            state_ = RepositoryState::CONFLICT;
            setError("Pull resulted in merge conflicts");
        } else {
            setError("Git pull failed: " + output);
        }
        return false;
    }

    if (progress) {
        progress(100, 100, "Pull complete");
    }

    log("INFO", "Pulled from remote");
    return true;
}

bool GitRepository::push(ProgressCallback progress) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    if (progress) {
        progress(0, 100, "Pushing to remote...");
    }

    auto [success, output] = impl_->execGit("push origin " + config_.repo_branch);
    if (!success) {
        if (output.find("non-fast-forward") != std::string::npos) {
            setError("Push rejected: remote has changes. Pull first.");
        } else {
            setError("Git push failed: " + output);
        }
        return false;
    }

    if (progress) {
        progress(100, 100, "Push complete");
    }

    log("INFO", "Pushed to remote");
    return true;
}

bool GitRepository::testConnection() {
    if (config_.repo_url.empty()) {
        return false;
    }

    auto [success, output] = impl_->execGit("ls-remote " + config_.repo_url, "/tmp");
    return success;
}

//=============================================================================
// Branch Operations
//=============================================================================

std::string GitRepository::getCurrentBranch() const {
    if (!impl_->is_open) {
        return "";
    }

    auto [success, output] = impl_->execGit("rev-parse --abbrev-ref HEAD");
    if (!success) {
        return "";
    }

    // Trim newline
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    return output;
}

std::vector<GitBranch> GitRepository::listBranches(bool include_remote) const {
    std::vector<GitBranch> branches;

    if (!impl_->is_open) {
        return branches;
    }

    std::string cmd = include_remote ? "branch -a" : "branch";
    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        return branches;
    }

    std::string current = getCurrentBranch();
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        GitBranch branch;
        branch.is_current = (line[0] == '*');

        // Remove leading "* " or "  "
        std::string name = line.substr(2);

        // Check for remote branch
        if (name.find("remotes/") == 0) {
            branch.is_remote = true;
            name = name.substr(8);  // Remove "remotes/"
        }

        branch.name = name;
        branches.push_back(branch);
    }

    return branches;
}

bool GitRepository::createBranch(const std::string& name, bool checkout_branch) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string cmd = checkout_branch ? "checkout -b " : "branch ";
    auto [success, output] = impl_->execGit(cmd + name);
    if (!success) {
        setError("Failed to create branch: " + output);
        return false;
    }

    log("INFO", "Created branch: " + name);
    return true;
}

bool GitRepository::checkout(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    auto [success, output] = impl_->execGit("checkout " + name);
    if (!success) {
        setError("Failed to checkout: " + output);
        return false;
    }

    log("INFO", "Checked out: " + name);
    return true;
}

bool GitRepository::deleteBranch(const std::string& name, bool force) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string flag = force ? "-D" : "-d";
    auto [success, output] = impl_->execGit("branch " + flag + " " + name);
    if (!success) {
        setError("Failed to delete branch: " + output);
        return false;
    }

    log("INFO", "Deleted branch: " + name);
    return true;
}

bool GitRepository::merge(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    auto [success, output] = impl_->execGit("merge " + name);
    if (!success) {
        if (output.find("CONFLICT") != std::string::npos) {
            state_ = RepositoryState::CONFLICT;
            setError("Merge resulted in conflicts");
        } else {
            setError("Merge failed: " + output);
        }
        return false;
    }

    log("INFO", "Merged branch: " + name);
    return true;
}

//=============================================================================
// Commit Operations
//=============================================================================

bool GitRepository::stage(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string cmd = "add";
    if (paths.empty()) {
        cmd += " -A";  // Stage all
    } else {
        for (const auto& path : paths) {
            cmd += " \"" + path + "\"";
        }
    }

    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        setError("Failed to stage files: " + output);
        return false;
    }

    return true;
}

bool GitRepository::unstage(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string cmd = "reset HEAD";
    if (!paths.empty()) {
        for (const auto& path : paths) {
            cmd += " \"" + path + "\"";
        }
    }

    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        setError("Failed to unstage files: " + output);
        return false;
    }

    return true;
}

std::string GitRepository::commit(const std::string& message,
                                   const std::string& author) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return "";
    }

    if (message.empty()) {
        setError("Commit message required");
        return "";
    }

    std::string cmd = "commit -m \"" + message + "\"";
    if (!author.empty()) {
        cmd += " --author=\"" + author + "\"";
    }

    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        if (output.find("nothing to commit") != std::string::npos) {
            setError("Nothing to commit");
        } else {
            setError("Commit failed: " + output);
        }
        return "";
    }

    // Get the commit SHA
    auto [sha_success, sha_output] = impl_->execGit("rev-parse HEAD");
    if (!sha_success) {
        return "";
    }

    while (!sha_output.empty() &&
           (sha_output.back() == '\n' || sha_output.back() == '\r')) {
        sha_output.pop_back();
    }

    log("INFO", "Created commit: " + sha_output.substr(0, 7));
    return sha_output;
}

std::vector<GitCommit> GitRepository::getHistory(int count,
                                                  const std::string& path) const {
    std::vector<GitCommit> history;

    if (!impl_->is_open) {
        return history;
    }

    std::string cmd = "log --format=\"%H|%h|%an|%ae|%s|%ai\" -n " +
                      std::to_string(count > 0 ? count : 100);
    if (!path.empty()) {
        cmd += " -- \"" + path + "\"";
    }

    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        return history;
    }

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        GitCommit commit;
        std::istringstream lineStream(line);
        std::string part;

        if (std::getline(lineStream, commit.sha, '|') &&
            std::getline(lineStream, commit.short_sha, '|') &&
            std::getline(lineStream, commit.author, '|') &&
            std::getline(lineStream, commit.email, '|') &&
            std::getline(lineStream, commit.message, '|')) {
            // Parse timestamp (simplified)
            history.push_back(commit);
        }
    }

    return history;
}

std::optional<GitCommit> GitRepository::getCommit(const std::string& sha) const {
    if (!impl_->is_open) {
        return std::nullopt;
    }

    auto [success, output] = impl_->execGit(
        "show -s --format=\"%H|%h|%an|%ae|%s|%ai\" " + sha);
    if (!success) {
        return std::nullopt;
    }

    GitCommit commit;
    std::istringstream lineStream(output);

    if (std::getline(lineStream, commit.sha, '|') &&
        std::getline(lineStream, commit.short_sha, '|') &&
        std::getline(lineStream, commit.author, '|') &&
        std::getline(lineStream, commit.email, '|') &&
        std::getline(lineStream, commit.message, '|')) {
        return commit;
    }

    return std::nullopt;
}

//=============================================================================
// Status and Diff
//=============================================================================

GitStatus GitRepository::getStatus() const {
    GitStatus status;
    status.state = state_;
    status.url = config_.repo_url;
    status.branch = getCurrentBranch();

    if (!impl_->is_open) {
        return status;
    }

    // Get HEAD commit
    auto [sha_success, sha_output] = impl_->execGit("rev-parse HEAD");
    if (sha_success) {
        while (!sha_output.empty() &&
               (sha_output.back() == '\n' || sha_output.back() == '\r')) {
            sha_output.pop_back();
        }
        status.last_commit_sha = sha_output;
    }

    // Get status
    auto [success, output] = impl_->execGit("status --porcelain");
    if (success) {
        std::istringstream stream(output);
        std::string line;

        while (std::getline(stream, line)) {
            if (line.size() < 3) continue;

            char index_status = line[0];
            char work_status = line[1];
            std::string file = line.substr(3);

            if (index_status == 'A' || index_status == 'M' ||
                index_status == 'D' || index_status == 'R') {
                status.staged_files.push_back(file);
            }
            if (work_status == 'M' || work_status == 'D') {
                status.modified_files.push_back(file);
            }
            if (index_status == '?' && work_status == '?') {
                status.untracked_files.push_back(file);
            }
            if (index_status == 'U' || work_status == 'U') {
                status.conflicted_files.push_back(file);
                status.has_conflicts = true;
            }
        }
    }

    status.pending_changes = static_cast<int>(status.staged_files.size() +
                                              status.modified_files.size());

    // Check ahead/behind
    auto [ahead_success, ahead_output] = impl_->execGit(
        "rev-list --count origin/" + status.branch + "..HEAD");
    if (ahead_success && !ahead_output.empty()) {
        try {
            int ahead = std::stoi(ahead_output);
            status.needs_push = (ahead > 0);
        } catch (...) {}
    }

    auto [behind_success, behind_output] = impl_->execGit(
        "rev-list --count HEAD..origin/" + status.branch);
    if (behind_success && !behind_output.empty()) {
        try {
            int behind = std::stoi(behind_output);
            status.needs_pull = (behind > 0);
        } catch (...) {}
    }

    return status;
}

bool GitRepository::isClean() const {
    auto status = getStatus();
    return status.pending_changes == 0 && status.untracked_files.empty();
}

bool GitRepository::hasUncommittedChanges() const {
    auto status = getStatus();
    return status.pending_changes > 0;
}

std::string GitRepository::getDiff(const std::string& path) const {
    if (!impl_->is_open) {
        return "";
    }

    std::string cmd = "diff HEAD";
    if (!path.empty()) {
        cmd += " -- \"" + path + "\"";
    }

    auto [success, output] = impl_->execGit(cmd);
    return success ? output : "";
}

std::string GitRepository::getDiffBetween(const std::string& from_sha,
                                           const std::string& to_sha) const {
    if (!impl_->is_open) {
        return "";
    }

    auto [success, output] = impl_->execGit("diff " + from_sha + ".." + to_sha);
    return success ? output : "";
}

std::vector<std::string> GitRepository::getChangedFiles(
    const std::string& from_sha,
    const std::string& to_sha) const {
    std::vector<std::string> files;

    if (!impl_->is_open) {
        return files;
    }

    auto [success, output] = impl_->execGit(
        "diff --name-only " + from_sha + ".." + to_sha);
    if (!success) {
        return files;
    }

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            files.push_back(line);
        }
    }

    return files;
}

//=============================================================================
// File Operations
//=============================================================================

std::optional<std::string> GitRepository::readFile(
    const std::string& path,
    const std::string& commit_sha) const {
    if (!impl_->is_open) {
        return std::nullopt;
    }

    if (commit_sha.empty()) {
        // Read from working directory
        std::string full_path = impl_->local_path + "/" + path;
        std::ifstream file(full_path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } else {
        // Read from specific commit
        auto [success, output] = impl_->execGit("show " + commit_sha + ":" + path);
        if (!success) {
            return std::nullopt;
        }
        return output;
    }
}

bool GitRepository::writeFile(const std::string& path, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string full_path = impl_->local_path + "/" + path;

    // Create parent directories
    try {
        fs::create_directories(fs::path(full_path).parent_path());
    } catch (const fs::filesystem_error& e) {
        setError("Failed to create directory: " + std::string(e.what()));
        return false;
    }

    // Write file
    std::ofstream file(full_path);
    if (!file.is_open()) {
        setError("Failed to open file for writing: " + full_path);
        return false;
    }

    file << content;
    file.close();

    return true;
}

bool GitRepository::deleteFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string full_path = impl_->local_path + "/" + path;

    try {
        if (fs::exists(full_path)) {
            fs::remove(full_path);
        }
    } catch (const fs::filesystem_error& e) {
        setError("Failed to delete file: " + std::string(e.what()));
        return false;
    }

    return true;
}

bool GitRepository::fileExists(const std::string& path,
                                const std::string& commit_sha) const {
    if (!impl_->is_open) {
        return false;
    }

    if (commit_sha.empty()) {
        return fs::exists(impl_->local_path + "/" + path);
    } else {
        auto [success, output] = impl_->execGit(
            "cat-file -e " + commit_sha + ":" + path);
        return success;
    }
}

std::vector<std::string> GitRepository::listFiles(const std::string& path,
                                                   bool recursive) const {
    std::vector<std::string> files;

    if (!impl_->is_open) {
        return files;
    }

    std::string full_path = impl_->local_path;
    if (!path.empty()) {
        full_path += "/" + path;
    }

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(full_path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string().substr(
                        impl_->local_path.size() + 1));
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(full_path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string().substr(
                        impl_->local_path.size() + 1));
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Directory doesn't exist or no permissions
    }

    return files;
}

//=============================================================================
// Reset and Revert
//=============================================================================

bool GitRepository::discardChanges(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string cmd = "checkout --";
    if (paths.empty()) {
        cmd += " .";
    } else {
        for (const auto& path : paths) {
            cmd += " \"" + path + "\"";
        }
    }

    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        setError("Failed to discard changes: " + output);
        return false;
    }

    return true;
}

bool GitRepository::reset(const std::string& commit_sha, bool hard) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return false;
    }

    std::string cmd = "reset";
    if (hard) {
        cmd += " --hard";
    }
    cmd += " " + commit_sha;

    auto [success, output] = impl_->execGit(cmd);
    if (!success) {
        setError("Reset failed: " + output);
        return false;
    }

    log("INFO", "Reset to " + commit_sha);
    return true;
}

std::string GitRepository::revert(const std::string& commit_sha) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!impl_->is_open) {
        setError("Repository not open");
        return "";
    }

    auto [success, output] = impl_->execGit("revert --no-edit " + commit_sha);
    if (!success) {
        setError("Revert failed: " + output);
        return "";
    }

    // Get new commit SHA
    auto [sha_success, sha_output] = impl_->execGit("rev-parse HEAD");
    if (!sha_success) {
        return "";
    }

    while (!sha_output.empty() &&
           (sha_output.back() == '\n' || sha_output.back() == '\r')) {
        sha_output.pop_back();
    }

    log("INFO", "Reverted " + commit_sha);
    return sha_output;
}

//=============================================================================
// Configuration
//=============================================================================

void GitRepository::setConfig(const GitConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    impl_->local_path = config_.repo_path;
}

std::string GitRepository::getLocalPath() const {
    return impl_->local_path;
}

std::string GitRepository::getRemoteUrl() const {
    return config_.repo_url;
}

void GitRepository::setLogCallback(LogCallback callback) {
    log_callback_ = callback;
}

std::string GitRepository::getLastError() const {
    return last_error_;
}

//=============================================================================
// Private Methods
//=============================================================================

void GitRepository::setError(const std::string& error) {
    last_error_ = error;
    log("ERROR", error);
}

void GitRepository::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, "[GitRepository] " + message);
    }
}

bool GitRepository::setupCredentials() {
    // For SSH key authentication
    if (!config_.ssh_key_path.empty()) {
        // Would configure libgit2 credential callback
        // For CLI, set GIT_SSH_COMMAND environment variable
        std::string ssh_cmd = "ssh -i " + config_.ssh_key_path;
        if (!config_.ssh_passphrase.empty()) {
            // Note: passphrase handling is more complex in practice
        }
#if defined(_WIN32)
        _putenv_s("GIT_SSH_COMMAND", ssh_cmd.c_str());
#else
        setenv("GIT_SSH_COMMAND", ssh_cmd.c_str(), 1);
#endif
    }

    // For username/password
    if (!config_.username.empty() && !config_.password.empty()) {
        // Would use credential helper or modify URL
    }

    return true;
}

bool GitRepository::validateConfig() {
    if (config_.repo_url.empty() && config_.repo_path.empty()) {
        setError("Either URL or local path must be specified");
        return false;
    }
    return true;
}

} // namespace git
} // namespace scratchbird
