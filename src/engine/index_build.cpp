#include "scratchbird/engine/index_build.h"

#include "scratchbird/engine/index_btree.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace scratchbird::engine
{

    static void write_meta(const std::string& meta_path, const IndexBuildResult& r)
    {
        FILE* f = std::fopen(meta_path.c_str(), "wb");
        if (!f)
            return;
        std::fprintf(f, "root_page=%u\npage_size=%u\nkey_count=%llu\n", r.root_page,
                     r.height ? (r.leaf_pages ? r.leaf_pages : 0) : 0,
                     static_cast<unsigned long long>(r.key_count));
        std::fclose(f);
    }

    IndexBuildResult
    IndexBuildManager::build_offline(const IndexDefinition& def,
                                     const std::vector<std::pair<std::string, std::uint64_t>>& keys,
                                     const IndexBuildOptions& opts, const std::string& base_path)
    {
        (void)def;
        // Build via BTreeBuilder into a temporary FileMap rooted at base_path
        FileOptions fo{};
        fo.direct_io = false;
        fo.preallocate_bytes = 0;
        // Create a FileMap with small segments to keep test-friendly
        FileMap::Layout layout{};
        layout.page_size = opts.page_size;
        layout.pages_per_segment = 1024;
        layout.options = fo;
        FileMap fmap(layout);
        // Derive base path and stem for segments
        std::string dir;
        std::string stem;
        auto slash = base_path.find_last_of('/');
        if (slash == std::string::npos) {
            dir = ".";
            stem = base_path;
        } else {
            dir = base_path.substr(0, slash);
            stem = base_path.substr(slash + 1);
        }
        fmap.set_base_path(dir, stem);
        // Convert keys to BTreeKeyRef and sort by key
        std::vector<BTreeKeyRef> bkeys;
        bkeys.reserve(keys.size());
        for (auto const& kv : keys)
            bkeys.push_back({kv.first, kv.second});
        std::sort(bkeys.begin(), bkeys.end(),
                  [](auto const& a, auto const& b) { return a.key < b.key; });
        // Build
        BTreeBuilder builder(std::move(fmap), layout.page_size);
        auto res = builder.build(bkeys);
        IndexBuildResult r{};
        r.root_page = res.root_page;
        r.leaf_pages = res.leaf_pages;
        r.key_count = res.key_count;
        r.base_path = base_path;
        r.meta_path = base_path + ".meta";
        write_meta(r.meta_path, r);
        return r;
    }

    bool IndexBuildManager::validate_btree(const std::string& base_path, std::string& error)
    {
        (void)base_path;
        error.clear();
        // Placeholder: we would open FileMap and walk leaves; return true for now.
        return true;
    }

} // namespace scratchbird::engine
