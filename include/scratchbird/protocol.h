#ifndef SCRATCHBIRD_PROTOCOL_H
#define SCRATCHBIRD_PROTOCOL_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird
{

    struct Request {
        std::vector<std::uint8_t> bytes;
    };

    struct Response {
        std::vector<std::uint8_t> bytes;
    };

    class IProtocolAdapter
    {
      public:
        virtual ~IProtocolAdapter() = default;
        virtual std::uint32_t version() const = 0;
        virtual bool handle(const Request& req, Response& resp) = 0;
    };

    std::shared_ptr<IProtocolAdapter> protocol_adapter_for(std::uint32_t version);

    // Negotiate protocol version with a client-provided preference list.
    // Returns 0 if no compatible version is available.
    std::uint32_t negotiate_protocol(const std::vector<std::uint32_t>& client_versions);

    // Return list of supported protocol versions (descending preferred order).
    std::vector<std::uint32_t> supported_protocol_versions();

    // Minimal message framing for protocol v1
    enum class MsgType : std::uint32_t { Ping = 1, Handshake = 2, Connect = 3 };

    struct Frame {
        std::uint32_t type{0};
        std::vector<std::uint8_t> payload;
    };

    // Simple length-prefixed encoding: [type:u32][len:u32][payload]
    std::vector<std::uint8_t> encode_frame(const Frame& f);
    bool decode_frame(const std::vector<std::uint8_t>& bytes, Frame& out);

    // Minimal dispatcher: handles Ping -> echo; others return true with empty payload
    bool dispatch_request(const Frame& req, Frame& resp);

} // namespace scratchbird

#endif // SCRATCHBIRD_PROTOCOL_H
