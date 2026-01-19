#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "scratchbird/client/network_client.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/protocol/wire_protocol.h"

namespace {

struct ServerHarness {
    std::unique_ptr<scratchbird::network::Socket> listener;
    uint16_t port = 0;
    std::thread thread;
    std::string error;

    void start() {
        thread = std::thread([this]() { run(); });
    }

    void stop() {
        if (listener) {
            listener->close();
        }
        if (thread.joinable()) {
            thread.join();
        }
    }

private:
    void run() {
        scratchbird::core::ErrorContext ctx;
        scratchbird::network::NetworkAddress client_addr;
        auto client = listener->accept(&client_addr, &ctx);
        if (!client) {
            error = "accept failed: " + ctx.message;
            return;
        }

        uint8_t header_buf[sizeof(scratchbird::protocol::MessageHeader)];
        auto status = client->readExact(header_buf, sizeof(header_buf), &ctx);
        if (status != scratchbird::core::Status::OK) {
            error = "read header failed: " + ctx.message;
            return;
        }

        scratchbird::protocol::MessageHeader header;
        status = scratchbird::protocol::Message::parseHeader(header_buf, header, &ctx);
        if (status != scratchbird::core::Status::OK) {
            error = "parse header failed: " + ctx.message;
            return;
        }

        if (header.type != static_cast<uint8_t>(scratchbird::protocol::MessageType::CONNECT_REQUEST)) {
            error = "unexpected message type";
            return;
        }

        if (header.payload_length > 0) {
            std::vector<uint8_t> payload(header.payload_length);
            status = client->readExact(payload.data(), payload.size(), &ctx);
            if (status != scratchbird::core::Status::OK) {
                error = "read payload failed: " + ctx.message;
                return;
            }
        }

        uint8_t session_id[16] = {0};
        session_id[0] = 0x42;
        auto response = scratchbird::protocol::ProtocolCodec::buildConnectResponse(
            true, session_id, ""
        );
        std::vector<uint8_t> buffer;
        status = response.serialize(buffer);
        if (status != scratchbird::core::Status::OK) {
            error = "serialize response failed";
            return;
        }

        status = client->writeExact(buffer.data(), buffer.size(), &ctx);
        if (status != scratchbird::core::Status::OK) {
            error = "write response failed: " + ctx.message;
            return;
        }
    }
};

} // namespace

TEST(DriverConnectivitySmokeTest, ConnectsToLocalListener) {
    scratchbird::network::NetworkInitGuard guard;
    ASSERT_TRUE(guard.isInitialized());

    ServerHarness harness;
    harness.listener = scratchbird::network::Socket::create(
        scratchbird::network::AddressFamily::IPV4
    );
    ASSERT_TRUE(harness.listener);

    scratchbird::network::NetworkAddress addr("127.0.0.1", 0);
    scratchbird::core::ErrorContext ctx;
    auto status = harness.listener->bind(addr, &ctx);
    ASSERT_EQ(status, scratchbird::core::Status::OK) << ctx.message;

    status = harness.listener->listen();
    ASSERT_EQ(status, scratchbird::core::Status::OK);

    auto local = harness.listener->getLocalAddress();
    ASSERT_TRUE(local.has_value());
    harness.port = local->port;
    ASSERT_GT(harness.port, 0u);

    harness.start();

    scratchbird::client::NetworkClient client;
    scratchbird::client::NetworkClientConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = harness.port;
    cfg.ssl_mode = scratchbird::network::SSLMode::DISABLED;
    cfg.connect_timeout_ms = 2000;
    cfg.read_timeout_ms = 2000;
    cfg.write_timeout_ms = 2000;
    cfg.database = "default";

    status = client.connect(cfg, &ctx);
    EXPECT_EQ(status, scratchbird::core::Status::OK) << ctx.message;
    client.disconnect();

    harness.stop();
    EXPECT_TRUE(harness.error.empty()) << harness.error;
}
