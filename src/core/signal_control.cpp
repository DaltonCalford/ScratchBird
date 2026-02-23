/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/signal_control.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>

#ifdef _WIN32
#include <csignal>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <signal.h>
#endif

namespace scratchbird::core
{

    namespace
    {

        class PlatformSignalControl final : public SignalControl
        {
        public:
            PlatformSignalControl() = default;
            ~PlatformSignalControl() override
            {
                (void)uninstall(nullptr);
            }

            auto install(const SignalInstallSpec& spec,
                         ErrorContext* ctx) -> Status override
            {
                std::lock_guard<std::mutex> guard(global_mutex_);
                if (installed_)
                {
                    return Status::OK;
                }
                auto* active = active_instance_.load(std::memory_order_acquire);
                if (active != nullptr && active != this)
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::OBJECT_IN_USE,
                                      "signal control already installed by another instance");
                    return Status::OBJECT_IN_USE;
                }

                install_spec_ = spec;
                previous_handler_count_ = 0;
                pending_signal_.store(static_cast<int>(ControlSignal::NONE),
                                      std::memory_order_release);

                if (install_spec_.enable_shutdown_signal)
                {
                    Status status = installSignal(SIGINT, ctx);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                    status = installSignal(SIGTERM, ctx);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                }

#ifdef SIGHUP
                if (install_spec_.enable_reload_signal)
                {
                    Status status = installSignal(SIGHUP, ctx);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                }
#endif

#ifdef SIGUSR1
                if (install_spec_.enable_rotate_logs_signal)
                {
                    Status status = installSignal(SIGUSR1, ctx);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                }
#endif

#ifdef SIGUSR2
                if (install_spec_.enable_dump_stats_signal)
                {
                    Status status = installSignal(SIGUSR2, ctx);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                }
#endif

#ifdef SIGQUIT
                if (install_spec_.enable_immediate_stop_signal)
                {
                    Status status = installSignal(SIGQUIT, ctx);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                }
#endif

#if !defined(_WIN32) && defined(SIGPIPE)
                if (install_spec_.ignore_broken_pipe)
                {
                    Status status = installSignal(SIGPIPE, ctx, SIG_IGN, SA_RESTART);
                    if (status != Status::OK)
                    {
                        (void)uninstall(ctx);
                        return status;
                    }
                }
#endif

                active_instance_.store(this, std::memory_order_release);
                installed_ = true;
                return Status::OK;
            }

            auto uninstall(ErrorContext* ctx) -> Status override
            {
                std::lock_guard<std::mutex> guard(global_mutex_);
                if (!installed_)
                {
                    return Status::OK;
                }

#ifdef _WIN32
                for (std::size_t i = 0; i < previous_handler_count_; ++i)
                {
                    const auto& entry = previous_handlers_[i];
                    if (!entry.valid)
                    {
                        continue;
                    }
                    std::signal(entry.signal_no, entry.handler);
                }
#else
                for (std::size_t i = 0; i < previous_handler_count_; ++i)
                {
                    const auto& entry = previous_handlers_[i];
                    if (!entry.valid)
                    {
                        continue;
                    }
                    if (::sigaction(entry.signal_no, &entry.previous_action, nullptr) != 0)
                    {
                        std::string message = "sigaction restore failed: ";
                        message.append(std::strerror(errno));
                        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                        return Status::IO_ERROR;
                    }
                }
#endif

                previous_handler_count_ = 0;
                pending_signal_.store(static_cast<int>(ControlSignal::NONE),
                                      std::memory_order_release);
                installed_ = false;

                auto* active = active_instance_.load(std::memory_order_acquire);
                if (active == this)
                {
                    active_instance_.store(nullptr, std::memory_order_release);
                }
                return Status::OK;
            }

            auto poll(ControlSignal* signal_out,
                      ErrorContext* ctx) -> Status override
            {
                if (signal_out == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "signal_out is null");
                    return Status::INVALID_ARGUMENT;
                }
                int raw = pending_signal_.exchange(static_cast<int>(ControlSignal::NONE),
                                                   std::memory_order_acq_rel);
                *signal_out = static_cast<ControlSignal>(raw);
                return Status::OK;
            }

            auto inject(ControlSignal signal,
                        ErrorContext* ctx) -> Status override
            {
                if (signal == ControlSignal::NONE)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "signal cannot be NONE");
                    return Status::INVALID_ARGUMENT;
                }
                queue(signal);
                return Status::OK;
            }

        private:
#ifdef _WIN32
            struct PreviousSignalHandler
            {
                int signal_no{0};
                void (*handler)(int){SIG_DFL};
                bool valid{false};
            };
#else
            struct PreviousSignalHandler
            {
                int signal_no{0};
                struct sigaction previous_action
                {
                };
                bool valid{false};
            };
#endif

            static void staticSignalHandler(int signal_no)
            {
                auto* instance = active_instance_.load(std::memory_order_acquire);
                if (instance == nullptr)
                {
                    return;
                }
                ControlSignal mapped = instance->mapSignal(signal_no);
                if (mapped != ControlSignal::NONE)
                {
                    instance->queue(mapped);
                }
            }

            auto installSignal(int signal_no,
                               ErrorContext* ctx) -> Status
            {
#ifdef _WIN32
                return installSignal(signal_no, ctx, &PlatformSignalControl::staticSignalHandler);
#else
                return installSignal(signal_no, ctx, &PlatformSignalControl::staticSignalHandler, SA_RESTART);
#endif
            }

#ifdef _WIN32
            auto installSignal(int signal_no,
                               ErrorContext* ctx,
                               void (*handler)(int)) -> Status
            {
                if (previous_handler_count_ >= previous_handlers_.size())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "signal handler store capacity exceeded");
                    return Status::INTERNAL_ERROR;
                }

                void (*previous_handler)(int) = std::signal(signal_no, handler);
                if (previous_handler == SIG_ERR)
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "signal install failed");
                    return Status::IO_ERROR;
                }

                auto& entry = previous_handlers_[previous_handler_count_++];
                entry.signal_no = signal_no;
                entry.handler = previous_handler;
                entry.valid = true;
                return Status::OK;
            }
#else
            auto installSignal(int signal_no,
                               ErrorContext* ctx,
                               void (*handler)(int),
                               int flags) -> Status
            {
                if (previous_handler_count_ >= previous_handlers_.size())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "signal handler store capacity exceeded");
                    return Status::INTERNAL_ERROR;
                }

                struct sigaction action
                {
                };
                action.sa_handler = handler;
                ::sigemptyset(&action.sa_mask);
                action.sa_flags = flags;

                struct sigaction previous
                {
                };
                if (::sigaction(signal_no, &action, &previous) != 0)
                {
                    std::string message = "sigaction install failed: ";
                    message.append(std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                    return Status::IO_ERROR;
                }

                auto& entry = previous_handlers_[previous_handler_count_++];
                entry.signal_no = signal_no;
                entry.previous_action = previous;
                entry.valid = true;
                return Status::OK;
            }
#endif

            auto mapSignal(int signal_no) const -> ControlSignal
            {
                switch (signal_no)
                {
                    case SIGINT:
                    case SIGTERM:
                        return ControlSignal::SHUTDOWN;
#ifdef SIGHUP
                    case SIGHUP:
                        return ControlSignal::RELOAD;
#endif
#ifdef SIGUSR1
                    case SIGUSR1:
                        return ControlSignal::ROTATE_LOGS;
#endif
#ifdef SIGUSR2
                    case SIGUSR2:
                        return ControlSignal::DUMP_STATS;
#endif
#ifdef SIGQUIT
                    case SIGQUIT:
                        return ControlSignal::IMMEDIATE_STOP;
#endif
                    default:
                        return ControlSignal::NONE;
                }
            }

            void queue(ControlSignal signal)
            {
                pending_signal_.store(static_cast<int>(signal), std::memory_order_release);
            }

            SignalInstallSpec install_spec_{};
            std::atomic<int> pending_signal_{static_cast<int>(ControlSignal::NONE)};
            bool installed_{false};
            std::array<PreviousSignalHandler, 8> previous_handlers_{};
            std::size_t previous_handler_count_{0};

            static std::mutex global_mutex_;
            static std::atomic<PlatformSignalControl*> active_instance_;
        };

        std::mutex PlatformSignalControl::global_mutex_;
        std::atomic<PlatformSignalControl*> PlatformSignalControl::active_instance_{nullptr};

    } // namespace

    auto createDefaultSignalControl() -> std::unique_ptr<SignalControl>
    {
        return std::make_unique<PlatformSignalControl>();
    }

} // namespace scratchbird::core
