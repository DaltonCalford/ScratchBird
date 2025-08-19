#include "scratchbird/protocol.h"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace scratchbird;

int main()
{
    Frame f{static_cast<std::uint32_t>(MsgType::Ping), {1, 2, 3}};
    auto bytes = encode_frame(f);
    Frame g{};
    assert(decode_frame(bytes, g));
    assert(f.type == g.type);
    assert(f.payload == g.payload);

    Frame resp{};
    assert(dispatch_request(f, resp));
    assert(resp.type == f.type);
    assert(resp.payload == f.payload);

    std::vector<std::uint8_t> bad{0, 0, 0, 1, 0, 0, 0, 5, 1, 2};
    Frame dummy{};
    assert(!decode_frame(bad, dummy));
    return 0;
}
