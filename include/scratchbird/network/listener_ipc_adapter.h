/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"

namespace scratchbird::network
{

    struct ListenerSocketConfig
    {
        AddressFamily family{AddressFamily::IPV4};
        std::string bind_address{"0.0.0.0"};
        uint16_t port{0};
        int backlog{static_cast<int>(DEFAULT_LISTEN_BACKLOG)};
        bool non_blocking{true};
    };

    class ListenerSocketAcceptor
    {
    public:
        virtual ~ListenerSocketAcceptor() = default;

        virtual auto start(const ListenerSocketConfig& config,
                           core::ErrorContext* ctx) -> core::Status = 0;

        virtual auto accept(NetworkAddress* client_address,
                            core::ErrorContext* ctx) -> std::unique_ptr<Socket> = 0;

        virtual void close() = 0;
        virtual auto isRunning() const -> bool = 0;
        virtual auto boundAddress() const -> NetworkAddress = 0;
    };

    class LocalControlChannel
    {
    public:
        virtual ~LocalControlChannel() = default;

        virtual auto start(const std::string& path,
                           core::ErrorContext* ctx) -> core::Status = 0;

        virtual auto accept(core::ErrorContext* ctx) -> std::unique_ptr<Socket> = 0;

        virtual void stop() = 0;
        virtual auto isRunning() const -> bool = 0;
        virtual auto path() const -> std::string = 0;
    };

    auto createDefaultListenerSocketAcceptor() -> std::unique_ptr<ListenerSocketAcceptor>;
    auto createDefaultLocalControlChannel() -> std::unique_ptr<LocalControlChannel>;

} // namespace scratchbird::network
