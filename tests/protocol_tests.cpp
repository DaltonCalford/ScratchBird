#include "scratchbird/protocol.h"

#include <cassert>
#include <vector>

using namespace scratchbird;

int main()
{
    // Supported list should contain v1 for now
    auto supported = supported_protocol_versions();
    assert(!supported.empty());

    // Client proposes new-first, old-second; we should pick v1
    std::vector<std::uint32_t> client{2, 1};
    auto chosen = negotiate_protocol(client);
    assert(chosen == 1);

    // Unknown only -> no match
    client = {99};
    chosen = negotiate_protocol(client);
    assert(chosen == 0);

    // Adapter retrieval returns non-null
    auto adapter = protocol_adapter_for(1);
    assert(adapter);
    assert(adapter->version() == 1);
    return 0;
}
