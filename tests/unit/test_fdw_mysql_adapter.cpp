#include "gtest/gtest.h"

#include "scratchbird/fdw/mysql_adapter.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace scratchbird::fdw {
namespace {

constexpr uint32_t kClientLongPassword = 0x00000001;
constexpr uint32_t kClientLongFlag = 0x00000004;
constexpr uint32_t kClientConnectWithDb = 0x00000008;
constexpr uint32_t kClientProtocol41 = 0x00000200;
constexpr uint32_t kClientTransactions = 0x00002000;
constexpr uint32_t kClientSecureConnection = 0x00008000;
constexpr uint32_t kClientPluginAuth = 0x00080000;
constexpr uint32_t kClientDeprecateEof = 0x01000000;
constexpr uint32_t kHandshakeCapabilities =
    kClientLongPassword |
    kClientLongFlag |
    kClientConnectWithDb |
    kClientProtocol41 |
    kClientTransactions |
    kClientSecureConnection |
    kClientPluginAuth |
    kClientDeprecateEof;

void appendInt2(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendInt4(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendLenEncString(std::vector<uint8_t>& out, const std::string& value) {
    ASSERT_LT(value.size(), 0xfbU);
    out.push_back(static_cast<uint8_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

void appendCString(std::vector<uint8_t>& out, const std::string& value) {
    out.insert(out.end(), value.begin(), value.end());
    out.push_back(0);
}

bool recvFully(int fd, void* buffer, size_t length) {
    auto* bytes = static_cast<uint8_t*>(buffer);
    size_t offset = 0;
    while (offset < length) {
        const ssize_t received = ::recv(fd, bytes + offset, length - offset, MSG_WAITALL);
        if (received <= 0) {
            return false;
        }
        offset += static_cast<size_t>(received);
    }
    return true;
}

bool readPacket(int fd, uint8_t& seq_out, std::vector<uint8_t>& payload_out) {
    uint8_t header[4];
    if (!recvFully(fd, header, sizeof(header))) {
        return false;
    }

    const uint32_t length =
        static_cast<uint32_t>(header[0]) |
        (static_cast<uint32_t>(header[1]) << 8) |
        (static_cast<uint32_t>(header[2]) << 16);
    seq_out = header[3];
    payload_out.assign(length, 0);
    if (length == 0) {
        return true;
    }
    return recvFully(fd, payload_out.data(), payload_out.size());
}

bool sendPacket(int fd, uint8_t seq, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> packet;
    packet.reserve(4 + payload.size());
    const uint32_t length = static_cast<uint32_t>(payload.size());
    packet.push_back(static_cast<uint8_t>(length & 0xFF));
    packet.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
    packet.push_back(seq);
    packet.insert(packet.end(), payload.begin(), payload.end());

    size_t offset = 0;
    while (offset < packet.size()) {
        const ssize_t sent = ::send(fd, packet.data() + offset, packet.size() - offset, 0);
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<size_t>(sent);
    }
    return true;
}

std::vector<uint8_t> buildHandshakePacket() {
    std::vector<uint8_t> payload;
    payload.push_back(0x0a);
    appendCString(payload, "8.0.36");
    appendInt4(payload, 77);

    static constexpr char kScramble1[] = "12345678";
    payload.insert(payload.end(), kScramble1, kScramble1 + 8);
    payload.push_back(0x00);

    appendInt2(payload, static_cast<uint16_t>(kHandshakeCapabilities & 0xFFFF));
    payload.push_back(33);  // utf8_general_ci
    appendInt2(payload, 0x0002);  // autocommit
    appendInt2(payload, static_cast<uint16_t>((kHandshakeCapabilities >> 16) & 0xFFFF));
    payload.push_back(21);  // auth plugin data length

    for (int i = 0; i < 10; ++i) {
        payload.push_back(0x00);
    }

    static constexpr char kScramble2[] = "abcdefghijkl";
    payload.insert(payload.end(), kScramble2, kScramble2 + 12);
    payload.push_back(0x00);
    appendCString(payload, "mysql_native_password");
    return payload;
}

std::vector<uint8_t> buildAuthOkPacket() {
    return {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
}

std::vector<uint8_t> buildResultsetTerminator() {
    return {0xfe, 0x00, 0x00, 0x02, 0x00};
}

std::vector<uint8_t> buildColumnDefinitionPacket() {
    std::vector<uint8_t> payload;
    appendLenEncString(payload, "def");
    appendLenEncString(payload, "compat_mysql");
    appendLenEncString(payload, "");
    appendLenEncString(payload, "");
    appendLenEncString(payload, "one");
    appendLenEncString(payload, "one");
    payload.push_back(0x0c);
    appendInt2(payload, 33);
    appendInt4(payload, 11);
    payload.push_back(0x03);  // MYSQL_TYPE_LONG
    appendInt2(payload, 0x0020);  // numeric
    payload.push_back(0x00);
    appendInt2(payload, 0x0000);
    return payload;
}

std::vector<uint8_t> buildRowPacket(const std::string& value) {
    std::vector<uint8_t> payload;
    appendLenEncString(payload, value);
    return payload;
}

class MockMySqlServer {
public:
    MockMySqlServer(int response_delay_ms, bool omit_column_terminator)
        : response_delay_ms_(response_delay_ms),
          omit_column_terminator_(omit_column_terminator) {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            ADD_FAILURE() << "Failed to create mock MySQL listener socket";
            return;
        }

        int reuse = 1;
        if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
            ADD_FAILURE() << "Failed to set SO_REUSEADDR on mock MySQL listener";
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ADD_FAILURE() << "Failed to bind mock MySQL listener";
            return;
        }
        if (::listen(listen_fd_, 1) != 0) {
            ADD_FAILURE() << "Failed to listen on mock MySQL listener";
            return;
        }

        socklen_t addr_len = sizeof(addr);
        if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
            ADD_FAILURE() << "Failed to query mock MySQL listener port";
            return;
        }
        port_ = ntohs(addr.sin_port);
        ready_ = true;

        server_thread_ = std::thread([this]() { run(); });
    }

    ~MockMySqlServer() {
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    uint16_t port() const {
        return port_;
    }

    bool ready() const {
        return ready_;
    }

private:
    void run() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd =
            ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            return;
        }

        (void)sendPacket(client_fd, 0, buildHandshakePacket());

        uint8_t client_seq = 0;
        std::vector<uint8_t> payload;
        if (!readPacket(client_fd, client_seq, payload)) {
            ::close(client_fd);
            return;
        }

        (void)sendPacket(client_fd, static_cast<uint8_t>(client_seq + 1), buildAuthOkPacket());

        while (readPacket(client_fd, client_seq, payload)) {
            if (payload.empty()) {
                continue;
            }

            if (payload[0] == 0x01) {  // COM_QUIT
                break;
            }

            if (payload[0] != 0x03) {  // COM_QUERY
                continue;
            }

            if (response_delay_ms_ > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(response_delay_ms_));
            }

            uint8_t seq = static_cast<uint8_t>(client_seq + 1);
            (void)sendPacket(client_fd, seq++, {0x01});
            (void)sendPacket(client_fd, seq++, buildColumnDefinitionPacket());
            if (!omit_column_terminator_) {
                (void)sendPacket(client_fd, seq++, buildResultsetTerminator());
            }
            (void)sendPacket(client_fd, seq++, buildRowPacket("1"));
            (void)sendPacket(client_fd, seq++, buildResultsetTerminator());
        }

        ::close(client_fd);
    }

    int listen_fd_ = -1;
    uint16_t port_ = 0;
    int response_delay_ms_ = 0;
    bool omit_column_terminator_ = false;
    bool ready_ = false;
    std::thread server_thread_;
};

ServerDefinition makeServerDefinition(uint16_t port) {
    ServerDefinition server;
    server.host = "127.0.0.1";
    server.port = port;
    server.database = "compat_mysql";
    server.connection_timeout_ms = 100;
    server.query_timeout_ms = 2000;
    return server;
}

UserMapping makeUserMapping() {
    UserMapping mapping;
    mapping.remote_user = "root";
    mapping.remote_password = "root";
    return mapping;
}

TEST(FDWMySQLAdapterTest, ExecuteQueryUsesQueryTimeoutAfterConnect) {
    MockMySqlServer server(/*response_delay_ms=*/250, /*omit_column_terminator=*/false);
    ASSERT_TRUE(server.ready());

    MySQLAdapter adapter;
    auto connect_result = adapter.connect(makeServerDefinition(server.port()), makeUserMapping());
    ASSERT_TRUE(connect_result) << connect_result.errorMessage();

    auto query_result = adapter.executeQuery("SELECT 1");
    ASSERT_TRUE(query_result) << query_result.errorMessage();
    ASSERT_TRUE(query_result->success);
    ASSERT_EQ(query_result->rows.size(), 1u);
    ASSERT_EQ(query_result->rows.front().size(), 1u);
    EXPECT_EQ(std::get<int32_t>(query_result->rows.front().front()), 1);

    auto disconnect_result = adapter.disconnect();
    ASSERT_TRUE(disconnect_result) << disconnect_result.errorMessage();
}

TEST(FDWMySQLAdapterTest, DeprecateEofResultsetKeepsFirstRowWithoutColumnTerminator) {
    MockMySqlServer server(/*response_delay_ms=*/0, /*omit_column_terminator=*/true);
    ASSERT_TRUE(server.ready());

    MySQLAdapter adapter;
    auto connect_result = adapter.connect(makeServerDefinition(server.port()), makeUserMapping());
    ASSERT_TRUE(connect_result) << connect_result.errorMessage();

    auto query_result = adapter.executeQuery("SELECT 1");
    ASSERT_TRUE(query_result) << query_result.errorMessage();
    ASSERT_TRUE(query_result->success);
    ASSERT_EQ(query_result->rows.size(), 1u);
    ASSERT_EQ(query_result->rows.front().size(), 1u);
    EXPECT_EQ(std::get<int32_t>(query_result->rows.front().front()), 1);

    auto disconnect_result = adapter.disconnect();
    ASSERT_TRUE(disconnect_result) << disconnect_result.errorMessage();
}

}  // namespace
}  // namespace scratchbird::fdw
