#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace scratchbird::engine;

static void usage(const char* argv0)
{
    std::fprintf(stderr, "Usage: %s <db_path_without_extension>\n", argv0);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    std::string path = argv[1];
    // Open seg0
    std::string seg0 = path + ".seg0";
    FileOptions fo{};
    try {
        FileHandle fh = FileManager::open(seg0, fo, /*create*/ false);
        std::vector<std::uint8_t> buf(4096, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::uint32_t page_size = hdr->page_size ? hdr->page_size : 4096u;
        buf.assign(page_size, 0);
        FileManager::pread(fh, buf.data(), buf.size(), 0);
        hdr = reinterpret_cast<const ods::PageHeader*>(buf.data());
        std::printf("ScratchBird header:\n");
        std::printf("  Page size: %u\n", hdr->page_size);
        std::printf("  Page type: %u\n", static_cast<unsigned>(hdr->type));
        std::printf("  Checksum : 0x%08x\n", hdr->checksum);
        std::printf("  Space id : %u\n", hdr->space_id);
        std::printf("  Page no  : %u\n", hdr->page_no);
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "dbhdr: %s\n", ex.what());
        return 1;
    }
}
