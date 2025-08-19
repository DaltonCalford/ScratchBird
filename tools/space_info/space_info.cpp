#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace scratchbird::engine;
using namespace scratchbird::engine::ods;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <base_dir> <base_name>\n", argv[0]);
        return 1;
    }
    std::string dir = argv[1];
    std::string base = argv[2];
    // Load segment 0
    FileMap::Layout layout{};
    layout.page_size = 4096; // best effort; will read header for actual size
    layout.pages_per_segment = 4096;
    layout.options = FileOptions{};
    FileMap fmap(layout);
    fmap.set_base_path(dir, base);

    std::vector<std::uint8_t> header(layout.page_size, 0);
    fmap.read_page(0, header.data());
    auto* h = reinterpret_cast<PageHeader*>(header.data());
    std::uint32_t ps = h->page_size ? h->page_size : layout.page_size;

    // Dump basic space catalog if present at page 3
    std::vector<std::uint8_t> scpg(ps, 0);
    fmap.read_page(3, scpg.data());
    auto* sch = reinterpret_cast<PageHeader*>(scpg.data());
    if (sch->type == static_cast<std::uint16_t>(PageType::SpaceCatalog)) {
        SpaceCatalogPayload sc{};
        std::memcpy(&sc, scpg.data() + sizeof(PageHeader), sizeof sc);
        std::printf("SpaceCatalog: space_id=%u page_size=%u segments=%u pip_root=%u tip_root=%u "
                    "next_extent_id=%u\n",
                    sc.space_id, sc.page_size, sc.segments, sc.pip_root_page, sc.tip_root_page,
                    sc.next_extent_id);
    } else {
        std::printf("No SpaceCatalog at page 3 (type=%u)\n", sch->type);
    }

    // Scan first few PIP pages and count allocated bits
    std::size_t total_alloc = 0;
    for (std::uint32_t pip = 1; pip < 1 + 16; pip += (pagesPerPIP(ps) + 1)) {
        std::vector<std::uint8_t> pipg(ps, 0);
        fmap.read_page(pip, pipg.data());
        auto* ph = reinterpret_cast<PageHeader*>(pipg.data());
        if (ph->type != static_cast<std::uint16_t>(PageType::Pip))
            continue;
        std::size_t bytes = bytesBitPIP(ps);
        std::size_t alloc = 0;
        for (std::size_t i = 0; i < bytes; ++i)
            alloc += __builtin_popcount(pipg[64 + i]);
        total_alloc += alloc;
        std::printf("PIP@%u: allocated pages=%zu\n", pip, alloc);
    }
    std::printf("Total allocated pages (sampled): %zu\n", total_alloc);
    return 0;
}
