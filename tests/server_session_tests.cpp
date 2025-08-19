#include "scratchbird/session.h"

#include <cassert>

using namespace scratchbird;

int main()
{
    // Handshake picks v1 from client preference list
    HandshakeResult hr = server_handshake({3, 2, 1});
    assert(hr.ok && hr.selected_version == 1);

    // No common version -> not ok
    hr = server_handshake({99});
    assert(!hr.ok && hr.selected_version == 0);

    // Connect via Y-Valve to embedded
    ConnectInfo ci{"/tmp/db.sbk"};
    assert(server_connect(ci, ProviderKind::Embedded));
    return 0;
}
