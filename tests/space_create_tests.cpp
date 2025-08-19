#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"
#include "scratchbird/engine/space.h"

#include <cstring>
#include <iostream>
#include <vector>

using namespace scratchbird::engine;
using namespace scratchbird::engine::ods;

int main()
{
    const std::string dir = "/tmp";
    const std::string base = "sb_space_create";
    FileOptions opts{};
    space_create(dir, base, 4096, 1024, opts);

    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 1024;
    layout.options = opts;
    FileMap fmap(layout);
    fmap.set_base_path(dir, base);

    // Verify header
    std::vector<std::uint8_t> hdr(layout.page_size, 0);
    fmap.read_page(0, hdr.data());
    auto* h = reinterpret_cast<PageHeader*>(hdr.data());
    if (h->page_size != 4096) {
        std::cerr << "header page_size mismatch" << std::endl;
        return 1;
    }

    // Verify SpaceCatalog at page 3
    std::vector<std::uint8_t> sc(layout.page_size, 0);
    fmap.read_page(3, sc.data());
    auto* sch = reinterpret_cast<PageHeader*>(sc.data());
    if (sch->type != static_cast<std::uint16_t>(PageType::SpaceCatalog)) {
        std::cerr << "missing SpaceCatalog" << std::endl;
        return 1;
    }
    SpaceCatalogPayload scp{};
    std::memcpy(&scp, sc.data() + sizeof(PageHeader), sizeof scp);
    if (scp.page_size != 4096 || scp.pip_root_page != 1 || scp.tip_root_page != 2) {
        std::cerr << "SpaceCatalog fields mismatch" << std::endl;
        return 1;
    }

    std::cout << "space_create_tests ok" << std::endl;
    return 0;
}
