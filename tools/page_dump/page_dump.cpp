#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace scratchbird::engine;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <seg0 path> <page_no>\n", argv[0]);
        return 1;
    }
    std::string seg0 = argv[1];
    std::uint32_t page_no = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10));
    FileOptions fo{};
    fo.direct_io = false;
    auto fh = FileManager::open(seg0, fo, /*create*/ false);
    // Read header to get page size
    std::vector<std::uint8_t> header(4096, 0);
    FileManager::pread(fh, header.data(), header.size(), 0);
    auto* h = reinterpret_cast<ods::PageHeader*>(header.data());
    std::uint32_t page_size = h->page_size ? h->page_size : 4096u;
    std::vector<std::uint8_t> page(page_size, 0);
    std::uint64_t off = static_cast<std::uint64_t>(page_no) * page_size;
    FileManager::pread(fh, page.data(), page.size(), off);
    auto* ph = reinterpret_cast<ods::PageHeader*>(page.data());
    std::printf("page_no=%u type=%u page_size=%u checksum=0x%08x prev=%u next=%u scn=%llu\n",
                ph->page_no, ph->type, ph->page_size, ph->checksum, ph->prev, ph->next,
                (unsigned long long)ph->scn);
    if (ph->type == static_cast<std::uint16_t>(ods::PageType::Pip)) {
        // print first 64 bits of bitmap coverage
        std::printf("PIP bitmap preview: ");
        for (int i = 0; i < 8; ++i) {
            std::printf("%02x ", page[64 + i]);
        }
        std::printf("\n");
    } else if (ph->type == static_cast<std::uint16_t>(ods::PageType::SpaceCatalog)) {
        ods::SpaceCatalogPayload sc{};
        std::memcpy(&sc, page.data() + sizeof(ods::PageHeader), sizeof sc);
        std::printf("SpaceCatalog v%u space_id=%u page_size=%u pip_root=%u tip_root=%u segments=%u "
                    "next_extent_id=%u\n",
                    sc.version, sc.space_id, sc.page_size, sc.pip_root_page, sc.tip_root_page,
                    sc.segments, sc.next_extent_id);
    } else if (ph->type == static_cast<std::uint16_t>(ods::PageType::Tip)) {
        std::printf("TIP statuses (first 32): ");
        const unsigned char* base = page.data() + 64;
        for (int i = 0; i < 32 && (64 + i) < (int)page.size(); ++i) {
            std::printf("%u ", (unsigned)base[i]);
        }
        std::printf("\n");
    }
    // Emit a quick hex of first 128 bytes
    for (int i = 0; i < 128 && i < (int)page.size(); i += 16) {
        std::printf("%04x: ", i);
        for (int j = 0; j < 16 && (i + j) < (int)page.size(); ++j)
            std::printf("%02x ", page[i + j]);
        std::printf("\n");
    }
    return 0;
}
