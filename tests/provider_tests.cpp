#include "scratchbird/provider.h"

#include <cassert>

using namespace scratchbird;

int main()
{
    auto emb = get_provider(ProviderKind::Embedded);
    auto rem = get_provider(ProviderKind::Remote);
    auto leg = get_provider(ProviderKind::Legacy);

    assert(emb && rem && leg);
    assert(std::string(emb->name()) == "embedded");
    assert(std::string(rem->name()) == "remote");
    assert(std::string(leg->name()) == "legacy");

    ConnectInfo ci{":memory:"};
    assert(emb->connect(ci));
    return 0;
}
