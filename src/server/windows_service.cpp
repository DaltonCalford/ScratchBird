/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/server/windows_service.h"

#include <atomic>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace scratchbird::server
{

    namespace
    {

        class PlatformWindowsServiceHost final : public WindowsServiceHost
        {
        public:
            auto runConsole(const std::function<int()>& run_callback,
                            core::ErrorContext* ctx) -> core::Status override
            {
                if (!run_callback)
                {
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                                      "run callback is required");
                    return core::Status::INVALID_ARGUMENT;
                }

                const int exit_code = run_callback();
                if (exit_code == 0)
                {
                    return core::Status::OK;
                }

                SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR,
                                  "console execution failed");
                return core::Status::INTERNAL_ERROR;
            }

            auto runAsService(const WindowsServiceOptions& options,
                              const std::function<int()>& run_callback,
                              const std::function<void()>& stop_callback,
                              core::ErrorContext* ctx) -> core::Status override
            {
                if (!run_callback || !stop_callback)
                {
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                                      "service run and stop callbacks are required");
                    return core::Status::INVALID_ARGUMENT;
                }

#ifndef _WIN32
                (void)options;
                (void)run_callback;
                (void)stop_callback;
                SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                  "Windows service mode is not supported on this platform");
                return core::Status::NOT_SUPPORTED;
#else
                if (active_host_ != nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR,
                                      "another Windows service host is already active");
                    return core::Status::INTERNAL_ERROR;
                }

                service_name_ = options.service_name.empty()
                    ? std::string("ScratchBirdServer")
                    : options.service_name;
                run_callback_ = run_callback;
                stop_callback_ = stop_callback;
                worker_exit_code_ = 0;
                stop_requested_.store(false);

                stop_event_ = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);
                worker_done_event_ = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);
                if (!stop_event_ || !worker_done_event_)
                {
                    cleanupHandles();
                    SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                      "CreateEvent failed for Windows service host");
                    return core::Status::IO_ERROR;
                }

                active_host_ = this;
                SERVICE_TABLE_ENTRYA dispatch_table[] = {
                    {const_cast<char*>(service_name_.c_str()),
                     &PlatformWindowsServiceHost::serviceMainThunk},
                    {nullptr, nullptr}
                };

                const BOOL dispatch_ok = ::StartServiceCtrlDispatcherA(dispatch_table);
                const DWORD dispatch_error = dispatch_ok ? NO_ERROR : ::GetLastError();
                active_host_ = nullptr;

                if (worker_thread_.joinable())
                {
                    worker_thread_.join();
                }
                cleanupHandles();

                if (!dispatch_ok)
                {
                    if (dispatch_error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
                    {
                        SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                                          "service mode requested but process is not running under SCM");
                        return core::Status::NOT_SUPPORTED;
                    }
                    SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                      "StartServiceCtrlDispatcher failed");
                    return core::Status::IO_ERROR;
                }

                if (worker_exit_code_ != 0)
                {
                    SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR,
                                      "service worker exited with failure");
                    return core::Status::INTERNAL_ERROR;
                }

                return core::Status::OK;
#endif
            }

        private:
#ifdef _WIN32
            static void WINAPI serviceMainThunk(DWORD argc, LPSTR* argv)
            {
                (void)argc;
                (void)argv;
                if (active_host_ != nullptr)
                {
                    active_host_->serviceMain();
                }
            }

            static DWORD WINAPI serviceControlThunk(DWORD control,
                                                    DWORD event_type,
                                                    LPVOID event_data,
                                                    LPVOID context)
            {
                (void)event_type;
                (void)event_data;
                auto* host = static_cast<PlatformWindowsServiceHost*>(context);
                if (!host)
                {
                    return ERROR_INVALID_HANDLE;
                }
                return host->serviceControl(control);
            }

            void serviceMain()
            {
                service_status_handle_ = ::RegisterServiceCtrlHandlerExA(
                    service_name_.c_str(),
                    &PlatformWindowsServiceHost::serviceControlThunk,
                    this);
                if (!service_status_handle_)
                {
                    worker_exit_code_ = 1;
                    if (worker_done_event_)
                    {
                        ::SetEvent(worker_done_event_);
                    }
                    return;
                }

                service_status_ = {};
                service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
                service_status_.dwCurrentState = SERVICE_START_PENDING;
                service_status_.dwControlsAccepted = 0;
                service_status_.dwWin32ExitCode = NO_ERROR;
                service_status_.dwServiceSpecificExitCode = 0;
                service_status_.dwCheckPoint = 1;
                service_status_.dwWaitHint = 3000;
                updateServiceStatus();

                worker_thread_ = std::thread([this]() {
                    worker_exit_code_ = run_callback_ ? run_callback_() : 1;
                    if (worker_done_event_)
                    {
                        ::SetEvent(worker_done_event_);
                    }
                });

                service_status_.dwCurrentState = SERVICE_RUNNING;
                service_status_.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
                service_status_.dwCheckPoint = 0;
                service_status_.dwWaitHint = 0;
                updateServiceStatus();

                HANDLE wait_handles[2] = {stop_event_, worker_done_event_};
                const DWORD wait_result = ::WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

                if (wait_result == WAIT_OBJECT_0)
                {
                    service_status_.dwCurrentState = SERVICE_STOP_PENDING;
                    service_status_.dwControlsAccepted = 0;
                    service_status_.dwCheckPoint = 2;
                    service_status_.dwWaitHint = 30000;
                    updateServiceStatus();

                    if (stop_callback_)
                    {
                        stop_callback_();
                    }

                    (void)::WaitForSingleObject(worker_done_event_, 30000);
                }

                if (worker_thread_.joinable())
                {
                    worker_thread_.join();
                }

                service_status_.dwCurrentState = SERVICE_STOPPED;
                service_status_.dwControlsAccepted = 0;
                service_status_.dwCheckPoint = 0;
                service_status_.dwWaitHint = 0;
                if (worker_exit_code_ == 0)
                {
                    service_status_.dwWin32ExitCode = NO_ERROR;
                    service_status_.dwServiceSpecificExitCode = 0;
                }
                else
                {
                    service_status_.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
                    service_status_.dwServiceSpecificExitCode =
                        static_cast<DWORD>(worker_exit_code_ < 0 ? 1 : worker_exit_code_);
                }
                updateServiceStatus();
            }

            auto serviceControl(DWORD control) -> DWORD
            {
                switch (control)
                {
                case SERVICE_CONTROL_STOP:
                case SERVICE_CONTROL_SHUTDOWN:
                    if (!stop_requested_.exchange(true) && stop_event_)
                    {
                        service_status_.dwCurrentState = SERVICE_STOP_PENDING;
                        service_status_.dwControlsAccepted = 0;
                        service_status_.dwCheckPoint = 3;
                        service_status_.dwWaitHint = 30000;
                        updateServiceStatus();
                        ::SetEvent(stop_event_);
                    }
                    return NO_ERROR;
                default:
                    return ERROR_CALL_NOT_IMPLEMENTED;
                }
            }

            void updateServiceStatus()
            {
                if (service_status_handle_)
                {
                    ::SetServiceStatus(service_status_handle_, &service_status_);
                }
            }

            void cleanupHandles()
            {
                if (stop_event_)
                {
                    ::CloseHandle(stop_event_);
                    stop_event_ = nullptr;
                }
                if (worker_done_event_)
                {
                    ::CloseHandle(worker_done_event_);
                    worker_done_event_ = nullptr;
                }
            }

            static PlatformWindowsServiceHost* active_host_;

            SERVICE_STATUS_HANDLE service_status_handle_{nullptr};
            SERVICE_STATUS service_status_{};
            HANDLE stop_event_{nullptr};
            HANDLE worker_done_event_{nullptr};
            std::thread worker_thread_;
            std::atomic<bool> stop_requested_{false};
            std::string service_name_;
            std::function<int()> run_callback_;
            std::function<void()> stop_callback_;
            int worker_exit_code_{0};
#endif
        };

#ifdef _WIN32
        PlatformWindowsServiceHost* PlatformWindowsServiceHost::active_host_ = nullptr;
#endif

    } // namespace

    auto createDefaultWindowsServiceHost() -> std::unique_ptr<WindowsServiceHost>
    {
        return std::make_unique<PlatformWindowsServiceHost>();
    }

} // namespace scratchbird::server
