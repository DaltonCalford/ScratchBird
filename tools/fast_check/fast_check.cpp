#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_btree.h"
#include "scratchbird/engine/ods.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace scratchbird::engine;

static std::uint32_t read_page_size(const std::string& seg0)
{
    FileOptions fo{};
    fo.direct_io = false;
    auto fh = FileManager::open(seg0, fo, /*create*/ false);
    std::vector<std::uint8_t> hdr(4096, 0);
    FileManager::pread(fh, hdr.data(), hdr.size(), 0);
    auto* h = reinterpret_cast<ods::PageHeader*>(hdr.data());
    return h->page_size ? h->page_size : 4096u;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <seg0 base path> <root_page>\n", argv[0]);
        return 1;
    }
    std::string seg0 = argv[1];
    std::uint32_t root_page = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10));
    std::uint32_t page_size = read_page_size(seg0);

    // Build a FileMap to read pages
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout layout{};
    layout.page_size = page_size;
    layout.pages_per_segment = 4096;
    layout.options = fo;
    FileMap fmap(layout);
    // Derive directory and base name from seg0 path (expects .../name.seg0)
    auto slash = seg0.find_last_of('/');
    std::string dir = (slash == std::string::npos) ? std::string(".") : seg0.substr(0, slash);
    std::string base = (slash == std::string::npos) ? seg0 : seg0.substr(slash + 1);
    // strip .seg0 suffix if present
    if (base.size() > 5 && base.substr(base.size() - 5) == ".seg0")
        base = base.substr(0, base.size() - 5);
    fmap.set_base_path(dir, base);

    BTreeIndex idx(std::move(fmap), page_size, /*unique*/ false);
    idx.open_existing(root_page);

    std::string err;
    bool ok = idx.validate(err);
    auto st = idx.compute_stats();
    double avg_per_leaf = st.leaf_pages ? static_cast<double>(st.key_count) / st.leaf_pages : 0.0;
    std::printf("Index fast-check: %s\n", ok ? "OK" : "FAIL");
    if (!ok)
        std::printf("  issue: %s\n", err.c_str());
    std::printf("Stats: height=%u leaf_pages=%u keys=%llu avg_keys_per_leaf=%.2f\n", st.height,
                st.leaf_pages, (unsigned long long)st.key_count, avg_per_leaf);
    if (ok && avg_per_leaf < 4.0) {
        std::printf("Advice: index may be bloated; consider REINDEX/REBUILD.\n");
    } else if (!ok) {
        std::printf("Advice: index inconsistencies found; schedule REINDEX/REBUILD.\n");
    }
    return ok ? 0 : 2;
}
