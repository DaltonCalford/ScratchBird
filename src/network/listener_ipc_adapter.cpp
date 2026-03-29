/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/network/listener_ipc_adapter.h"

#include "scratchbird/network/control_plane.h"

#include <memory>

// Section 32 invariant: listener_ipc_adapter is a shared boundary between the
// listener front door and downstream parser or engine execution surfaces. It
// must not be treated as sole owner of either side of that handoff.

namespace scratchbird::network
{

    namespace
    {

        class DefaultListenerSocketAcceptor final : public ListenerSocketAcceptor
        {
        public:
            auto start(const ListenerSocketConfig& config,
                       core::ErrorContext* ctx) -> core::Status override
            {
                close();

                listener_ = Socket::create(config.family, SocketType::STREAM, ctx);
                if (!listener_)
                {
                    return core::Status::IO_ERROR;
                }

                core::Status status = listener_->setNonBlocking(config.non_blocking, ctx);
                if (status != core::Status::OK)
                {
                    listener_.reset();
                    return status;
                }

                status = listener_->setReuseAddress(true, ctx);
                if (status != core::Status::OK)
                {
                    listener_.reset();
                    return status;
                }

                NetworkAddress bind_address;
                bind_address.family = config.family;
                bind_address.host = config.bind_address;
                bind_address.port = config.port;

                status = listener_->bind(bind_address, ctx);
                if (status != core::Status::OK)
                {
                    listener_.reset();
                    return status;
                }

                status = listener_->listen(config.backlog, ctx);
                if (status != core::Status::OK)
                {
                    listener_.reset();
                    return status;
                }

                bound_address_ = bind_address;
                return core::Status::OK;
            }

            auto accept(NetworkAddress* client_address,
                        core::ErrorContext* ctx) -> std::unique_ptr<Socket> override
            {
                if (!listener_)
                {
                    SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "listener socket not running");
                    return nullptr;
                }
                return listener_->accept(client_address, ctx);
            }

            void close() override
            {
                if (listener_)
                {
                    listener_->close();
                    listener_.reset();
                }
                bound_address_ = NetworkAddress{};
            }

            auto isRunning() const -> bool override
            {
                return listener_ != nullptr && listener_->isOpen();
            }

            auto boundAddress() const -> NetworkAddress override
            {
                return bound_address_;
            }

        private:
            std::unique_ptr<Socket> listener_;
            NetworkAddress bound_address_;
        };

        class DefaultLocalControlChannel final : public LocalControlChannel
        {
        public:
            auto start(const std::string& path,
                       core::ErrorContext* ctx) -> core::Status override
            {
                return server_.start(path, ctx);
            }

            auto accept(core::ErrorContext* ctx) -> std::unique_ptr<Socket> override
            {
                return server_.accept(ctx);
            }

            void stop() override
            {
                server_.stop();
            }

            auto isRunning() const -> bool override
            {
                return server_.isRunning();
            }

            auto path() const -> std::string override
            {
                return server_.path();
            }

        private:
            ControlPlaneServer server_;
        };

    } // namespace

    auto createDefaultListenerSocketAcceptor() -> std::unique_ptr<ListenerSocketAcceptor>
    {
        return std::make_unique<DefaultListenerSocketAcceptor>();
    }

    auto createDefaultLocalControlChannel() -> std::unique_ptr<LocalControlChannel>
    {
        return std::make_unique<DefaultLocalControlChannel>();
    }

} // namespace scratchbird::network
