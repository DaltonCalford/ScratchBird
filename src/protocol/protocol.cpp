#include "scratchbird/protocol.h"

namespace scratchbird
{

    namespace
    {
        class ProtoV1 : public IProtocolAdapter
        {
          public:
            std::uint32_t version() const override
            {
                return 1;
            }
            bool handle(const Request&, Response&) override
            {
                return true;
            }
        };

        std::vector<std::uint32_t> kSupported{1};
    } // namespace

    std::shared_ptr<IProtocolAdapter> protocol_adapter_for(std::uint32_t)
    {
        return std::make_shared<ProtoV1>();
    }

    std::uint32_t negotiate_protocol(const std::vector<std::uint32_t>& client_versions)
    {
        for (auto cv : client_versions) {
            for (auto sv : kSupported) {
                if (cv == sv)
                    return sv;
            }
        }
        return 0;
    }

    std::vector<std::uint32_t> supported_protocol_versions()
    {
        return kSupported;
    }

    std::vector<std::uint8_t> encode_frame(const Frame& f)
    {
        std::vector<std::uint8_t> out;
        out.reserve(8 + f.payload.size());
        auto push32 = [&](std::uint32_t v) {
            out.push_back((v >> 24) & 0xFF);
            out.push_back((v >> 16) & 0xFF);
            out.push_back((v >> 8) & 0xFF);
            out.push_back(v & 0xFF);
        };
        push32(static_cast<std::uint32_t>(f.type));
        push32(static_cast<std::uint32_t>(f.payload.size()));
        out.insert(out.end(), f.payload.begin(), f.payload.end());
        return out;
    }

    bool decode_frame(const std::vector<std::uint8_t>& bytes, Frame& out)
    {
        if (bytes.size() < 8)
            return false;
        auto read32 = [&](size_t off) -> std::uint32_t {
            return (static_cast<std::uint32_t>(bytes[off]) << 24) |
                   (static_cast<std::uint32_t>(bytes[off + 1]) << 16) |
                   (static_cast<std::uint32_t>(bytes[off + 2]) << 8) |
                   (static_cast<std::uint32_t>(bytes[off + 3]));
        };
        std::uint32_t type = read32(0);
        std::uint32_t len = read32(4);
        if (bytes.size() != 8u + len)
            return false;
        out.type = type;
        out.payload.assign(bytes.begin() + 8, bytes.end());
        return true;
    }

    bool dispatch_request(const Frame& req, Frame& resp)
    {
        if (req.type == static_cast<std::uint32_t>(MsgType::Ping)) {
            resp.type = static_cast<std::uint32_t>(MsgType::Ping);
            resp.payload = req.payload;
            return true;
        }
        resp.type = req.type;
        resp.payload.clear();
        return true;
    }
} // namespace scratchbird
