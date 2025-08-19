#include "scratchbird/engine/index_build.h"

#include <cassert>
#include <string>
#include <unistd.h>

using namespace scratchbird::engine;

int main()
{
    IndexDefinition def{};
    def.index_name = "idx_test";
    def.relation_name = "t";
    def.unique = false;
    IndexBuildOptions opts{};
    opts.page_size = 4096;
    opts.online = false;
    std::string base = std::string("/tmp/idx_build_") + std::to_string(::getpid());

    std::vector<std::pair<std::string, std::uint64_t>> keys = {{"b", 2}, {"a", 1}, {"c", 3}};
    auto res = IndexBuildManager::build_offline(def, keys, opts, base);
    assert(res.root_page != 0);
    std::string err;
    bool ok = IndexBuildManager::validate_btree(base, err);
    assert(ok);
    return 0;
}
