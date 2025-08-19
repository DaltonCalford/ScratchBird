#ifndef SCRATCHBIRD_SESSION_H
#define SCRATCHBIRD_SESSION_H

#include "scratchbird/protocol.h"
#include "scratchbird/provider.h"
#include "scratchbird/yvalve.h"

#include <cstdint>
#include <vector>

namespace scratchbird
{

    struct HandshakeResult {
        std::uint32_t selected_version{0};
        bool ok{false};
    };

    inline HandshakeResult server_handshake(const std::vector<std::uint32_t>& client_versions)
    {
        HandshakeResult r{};
        r.selected_version = negotiate_protocol(client_versions);
        r.ok = r.selected_version != 0;
        return r;
    }

    inline bool server_connect(const ConnectInfo& info, ProviderKind kind)
    {
        return dispatch_connect(info, kind);
    }

} // namespace scratchbird

#endif // SCRATCHBIRD_SESSION_H
