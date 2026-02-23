/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/process_control.h"

#include <chrono>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace scratchbird::core
{

    namespace
    {

#ifdef _WIN32
        auto quoteWindowsArg(const std::string& arg) -> std::string
        {
            if (arg.find_first_of(" \t\"") == std::string::npos)
            {
                return arg;
            }

            std::string out;
            out.reserve(arg.size() + 2);
            out.push_back('"');
            for (char ch : arg)
            {
                if (ch == '"')
                {
                    out.append("\\\"");
                }
                else
                {
                    out.push_back(ch);
                }
            }
            out.push_back('"');
            return out;
        }

        auto buildWindowsCommandLine(const ProcessLaunchSpec& spec) -> std::string
        {
            std::string cmd = quoteWindowsArg(spec.executable);
            for (const auto& arg : spec.arguments)
            {
                cmd.push_back(' ');
                cmd.append(quoteWindowsArg(arg));
            }
            return cmd;
        }
#endif

        class PlatformProcessControl final : public ProcessControl
        {
        public:
            auto spawn(const ProcessLaunchSpec& spec,
                       SpawnedProcess* out,
                       ErrorContext* ctx) -> Status override
            {
                if (out == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "out process handle is null");
                    return Status::INVALID_ARGUMENT;
                }
                if (spec.executable.empty())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "executable is required");
                    return Status::INVALID_ARGUMENT;
                }

#ifdef _WIN32
                STARTUPINFOA startup_info{};
                PROCESS_INFORMATION process_info{};
                startup_info.cb = sizeof(startup_info);

                std::string command_line = buildWindowsCommandLine(spec);
                DWORD creation_flags = 0;
                if (spec.create_new_process_group)
                {
                    creation_flags |= CREATE_NEW_PROCESS_GROUP;
                }

                std::string working_directory = spec.working_directory;
                LPSTR mutable_cmd = command_line.empty() ? nullptr : command_line.data();
                LPCSTR cwd = working_directory.empty() ? nullptr : working_directory.c_str();
                BOOL ok = CreateProcessA(
                    nullptr,
                    mutable_cmd,
                    nullptr,
                    nullptr,
                    FALSE,
                    creation_flags,
                    nullptr,
                    cwd,
                    &startup_info,
                    &process_info
                );
                if (!ok)
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "CreateProcessA failed");
                    return Status::IO_ERROR;
                }

                out->process_id = static_cast<uint64_t>(process_info.dwProcessId);
                out->native_handle = reinterpret_cast<uintptr_t>(process_info.hProcess);
                out->has_native_handle = true;
                CloseHandle(process_info.hThread);
                (void)spec.environment_overrides;
                return Status::OK;
#else
                pid_t pid = fork();
                if (pid < 0)
                {
                    std::string message = "fork failed: " + std::string(std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                    return Status::IO_ERROR;
                }

                if (pid == 0)
                {
                    if (!spec.working_directory.empty())
                    {
                        (void)::chdir(spec.working_directory.c_str());
                    }

                    if (spec.create_new_process_group)
                    {
                        (void)::setpgid(0, 0);
                    }

                    for (const auto& kv : spec.environment_overrides)
                    {
                        (void)::setenv(kv.first.c_str(), kv.second.c_str(), 1);
                    }

                    std::vector<char*> argv;
                    argv.reserve(spec.arguments.size() + 2);
                    argv.push_back(const_cast<char*>(spec.executable.c_str()));
                    for (const auto& arg : spec.arguments)
                    {
                        argv.push_back(const_cast<char*>(arg.c_str()));
                    }
                    argv.push_back(nullptr);
                    ::execvp(argv[0], argv.data());
                    _exit(127);
                }

                out->process_id = static_cast<uint64_t>(pid);
                out->native_handle = 0;
                out->has_native_handle = false;
                return Status::OK;
#endif
            }

            auto forkSelf(bool* is_parent_out,
                          uint64_t* child_pid_out,
                          ErrorContext* ctx) -> Status override
            {
                if (is_parent_out == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "is_parent_out is null");
                    return Status::INVALID_ARGUMENT;
                }
                *is_parent_out = false;
                if (child_pid_out != nullptr)
                {
                    *child_pid_out = 0;
                }

#ifdef _WIN32
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "fork is not supported on Windows");
                return Status::NOT_SUPPORTED;
#else
                pid_t pid = ::fork();
                if (pid < 0)
                {
                    std::string message = "fork failed: " + std::string(std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                    return Status::IO_ERROR;
                }
                if (pid > 0)
                {
                    *is_parent_out = true;
                    if (child_pid_out != nullptr)
                    {
                        *child_pid_out = static_cast<uint64_t>(pid);
                    }
                }
                return Status::OK;
#endif
            }

            auto wait(const SpawnedProcess& process,
                      uint32_t timeout_ms,
                      ProcessWaitResult* result,
                      ErrorContext* ctx) -> Status override
            {
                if (result == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "wait result is null");
                    return Status::INVALID_ARGUMENT;
                }
                *result = ProcessWaitResult{};

                if (process.process_id == 0)
                {
                    result->state = ProcessState::NOT_FOUND;
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "process id is not set");
                    return Status::INVALID_ARGUMENT;
                }

#ifdef _WIN32
                HANDLE handle = process.has_native_handle
                    ? reinterpret_cast<HANDLE>(process.native_handle)
                    : OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                  static_cast<DWORD>(process.process_id));
                if (handle == nullptr)
                {
                    result->state = ProcessState::NOT_FOUND;
                    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "OpenProcess failed");
                    return Status::NOT_FOUND;
                }

                DWORD wait_status = WaitForSingleObject(handle, timeout_ms);
                if (wait_status == WAIT_TIMEOUT)
                {
                    result->state = ProcessState::TIMED_OUT;
                    if (!process.has_native_handle)
                    {
                        CloseHandle(handle);
                    }
                    return Status::OK;
                }
                if (wait_status != WAIT_OBJECT_0)
                {
                    if (!process.has_native_handle)
                    {
                        CloseHandle(handle);
                    }
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "WaitForSingleObject failed");
                    return Status::IO_ERROR;
                }

                DWORD exit_code = 0;
                if (!GetExitCodeProcess(handle, &exit_code))
                {
                    if (!process.has_native_handle)
                    {
                        CloseHandle(handle);
                    }
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "GetExitCodeProcess failed");
                    return Status::IO_ERROR;
                }

                result->state = ProcessState::EXITED;
                result->exit_code = static_cast<int>(exit_code);
                result->signaled = false;
                result->signal_code = 0;

                if (!process.has_native_handle)
                {
                    CloseHandle(handle);
                }
                return Status::OK;
#else
                auto start = std::chrono::steady_clock::now();
                pid_t pid = static_cast<pid_t>(process.process_id);
                while (true)
                {
                    int status = 0;
                    pid_t wait_result = ::waitpid(pid, &status, WNOHANG);
                    if (wait_result == pid)
                    {
                        result->state = ProcessState::EXITED;
                        if (WIFEXITED(status))
                        {
                            result->exit_code = WEXITSTATUS(status);
                        }
                        if (WIFSIGNALED(status))
                        {
                            result->signaled = true;
                            result->signal_code = WTERMSIG(status);
                        }
                        return Status::OK;
                    }
                    if (wait_result < 0)
                    {
                        if (errno == ECHILD)
                        {
                            result->state = ProcessState::NOT_FOUND;
                            return Status::NOT_FOUND;
                        }
                        std::string message = "waitpid failed: " + std::string(std::strerror(errno));
                        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                        return Status::IO_ERROR;
                    }

                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start).count();
                    if (elapsed >= timeout_ms)
                    {
                        result->state = ProcessState::TIMED_OUT;
                        return Status::OK;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
#endif
            }

            auto terminate(const SpawnedProcess& process,
                           bool force,
                           ErrorContext* ctx) -> Status override
            {
                if (process.process_id == 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "process id is not set");
                    return Status::INVALID_ARGUMENT;
                }

#ifdef _WIN32
                HANDLE handle = process.has_native_handle
                    ? reinterpret_cast<HANDLE>(process.native_handle)
                    : OpenProcess(PROCESS_TERMINATE, FALSE,
                                  static_cast<DWORD>(process.process_id));
                if (handle == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "OpenProcess failed");
                    return Status::NOT_FOUND;
                }
                UINT exit_code = force ? 1u : 0u;
                BOOL ok = TerminateProcess(handle, exit_code);
                if (!process.has_native_handle)
                {
                    CloseHandle(handle);
                }
                if (!ok)
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "TerminateProcess failed");
                    return Status::IO_ERROR;
                }
                return Status::OK;
#else
                int signal_no = force ? SIGKILL : SIGTERM;
                if (::kill(static_cast<pid_t>(process.process_id), signal_no) != 0)
                {
                    if (errno == ESRCH)
                    {
                        return Status::NOT_FOUND;
                    }
                    std::string message = "kill failed: " + std::string(std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                    return Status::IO_ERROR;
                }
                return Status::OK;
#endif
            }

            auto isRunning(const SpawnedProcess& process,
                           bool* running_out,
                           ErrorContext* ctx) -> Status override
            {
                if (running_out == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "running_out is null");
                    return Status::INVALID_ARGUMENT;
                }
                if (process.process_id == 0)
                {
                    *running_out = false;
                    return Status::OK;
                }

#ifdef _WIN32
                HANDLE handle = process.has_native_handle
                    ? reinterpret_cast<HANDLE>(process.native_handle)
                    : OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                  static_cast<DWORD>(process.process_id));
                if (handle == nullptr)
                {
                    *running_out = false;
                    return Status::NOT_FOUND;
                }
                DWORD exit_code = 0;
                BOOL ok = GetExitCodeProcess(handle, &exit_code);
                if (!process.has_native_handle)
                {
                    CloseHandle(handle);
                }
                if (!ok)
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "GetExitCodeProcess failed");
                    return Status::IO_ERROR;
                }
                *running_out = (exit_code == STILL_ACTIVE);
                return Status::OK;
#else
                pid_t pid = static_cast<pid_t>(process.process_id);
                if (::kill(pid, 0) == 0)
                {
                    *running_out = true;
                    return Status::OK;
                }
                if (errno == EPERM)
                {
                    *running_out = true;
                    return Status::OK;
                }
                if (errno == ESRCH)
                {
                    *running_out = false;
                    return Status::OK;
                }
                std::string message = "kill(0) failed: " + std::string(std::strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                return Status::IO_ERROR;
#endif
            }

            auto close(SpawnedProcess* process,
                       ErrorContext* ctx) -> Status override
            {
                (void)ctx;
                if (process == nullptr)
                {
                    return Status::INVALID_ARGUMENT;
                }

#ifdef _WIN32
                if (process->has_native_handle && process->native_handle != 0)
                {
                    HANDLE handle = reinterpret_cast<HANDLE>(process->native_handle);
                    CloseHandle(handle);
                }
#endif
                process->native_handle = 0;
                process->has_native_handle = false;
                return Status::OK;
            }
        };

    } // namespace

    auto createDefaultProcessControl() -> std::unique_ptr<ProcessControl>
    {
        return std::make_unique<PlatformProcessControl>();
    }

} // namespace scratchbird::core
