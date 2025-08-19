#include "scratchbird/engine/config.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;
using namespace scratchbird::engine::ods;

static void split_path(const std::string& path, std::string& dir, std::string& base)
{
    auto slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        dir = ".";
        base = path;
    } else {
        dir = path.substr(0, slash);
        base = path.substr(slash + 1);
    }
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <base_path> <page_size> <root_page>\n";
        return 1;
    }
    std::string base_path = argv[1];
    std::uint32_t page_size = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10));
    std::uint32_t root_page = static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10));

    FileMap::Layout layout{};
    layout.page_size = page_size;
    layout.pages_per_segment = 4096;
    layout.options = FileOptions{};
    FileMap fmap(layout);
    std::string dir, stem;
    split_path(base_path, dir, stem);
    fmap.set_base_path(dir, stem);

    // Read root page
    std::vector<std::uint8_t> page(page_size, 0);
    fmap.read_page(root_page, page.data());
    auto* ph = reinterpret_cast<PageHeader*>(page.data());
    const auto& cfg = get_engine_config();
    if (cfg.checksum_policy == ChecksumPolicy::VerifyOnRead) {
        std::vector<std::uint8_t> tmp = page;
        reinterpret_cast<PageHeader*>(tmp.data())->checksum = 0;
        std::uint32_t c = crc32c(tmp.data(), tmp.size());
        if (c != ph->checksum) {
            std::cerr << "Checksum mismatch on root page" << std::endl;
            return 2;
        }
    }
    if (ph->type != static_cast<std::uint16_t>(PageType::HeapRoot)) {
        std::cerr << "Root page type mismatch: got " << ph->type << ", expected HeapRoot\n";
        return 2;
    }
    HeapRootPayload hr{};
    std::memcpy(&hr, page.data() + sizeof(PageHeader), sizeof hr);
    std::cout << "Root ok: first=" << hr.first_heap_page << " last=" << hr.last_heap_page
              << " format_id=" << hr.tuple_format_id << "\n";

    std::size_t heap_pages = 0, overflow_pages = 0;
    std::size_t total_slots = 0;
    std::size_t total_free = 0;
    std::size_t bad_pages = 0;

    for (std::uint32_t p = hr.first_heap_page; p <= hr.last_heap_page; ++p) {
        std::vector<std::uint8_t> pg(page_size, 0);
        fmap.read_page(p, pg.data());
        auto* h = reinterpret_cast<PageHeader*>(pg.data());
        if (cfg.checksum_policy == ChecksumPolicy::VerifyOnRead) {
            std::vector<std::uint8_t> tmp = pg;
            reinterpret_cast<PageHeader*>(tmp.data())->checksum = 0;
            std::uint32_t c = crc32c(tmp.data(), tmp.size());
            if (c != h->checksum) {
                std::cerr << "Checksum mismatch on page " << p << "\n";
                ++bad_pages;
                continue;
            }
        }
        if (h->type == static_cast<std::uint16_t>(PageType::HeapData)) {
            ++heap_pages;
            std::string err;
            bool ok = HeapPageCodec::check_heap_page_invariants(pg, err);
            if (!ok) {
                std::cerr << "Page " << p << " invariant error: " << err << "\n";
                ++bad_pages;
                continue;
            }
            auto hh = HeapPageCodec::read_heap_hdr(pg);
            total_slots += hh.num_slots;
            total_free += HeapPageCodec::free_bytes(pg);
            // Attribute-directory vs nullmap quick consistency
            for (int si = 0; si < hh.num_slots; ++si) {
                std::uint16_t off = 0;
                std::memcpy(&off, pg.data() + (pg.size() - (si + 1) * HEAP_SLOT_SIZE_BYTES), 2);
                if (off == 0)
                    continue;
                if (off + sizeof(TupleHeader) > pg.size()) {
                    std::cerr << "Tuple header OOB on page " << p << "\n";
                    ++bad_pages;
                    break;
                }
                TupleHeader th{};
                std::memcpy(&th, pg.data() + off, sizeof th);
                std::size_t nullmap_off = off + sizeof(TupleHeader);
                if (nullmap_off + th.nullmap_bytes > pg.size()) {
                    std::cerr << "Nullmap OOB on page " << p << "\n";
                    ++bad_pages;
                    break;
                }
                const std::uint8_t* nullmap = pg.data() + nullmap_off;
                std::size_t dir_off = nullmap_off + th.nullmap_bytes;
                std::size_t base = dir_off + 2 * th.num_attrs;
                if (base > hh.dir_start) {
                    std::cerr << "Attr dir OOB on page " << p << "\n";
                    ++bad_pages;
                    break;
                }
                for (int ai = 0; ai < th.num_attrs; ++ai) {
                    bool is_null = (nullmap[ai / 8] >> (ai % 8)) & 1u;
                    std::uint16_t rel = 0;
                    std::memcpy(&rel, pg.data() + dir_off + 2 * ai, 2);
                    if (is_null) {
                        continue;
                    } else {
                        if (rel == 0) {
                            std::cerr << "Non-null attr with zero dir at slot " << si << " attr "
                                      << ai << " on page " << p << "\n";
                            ++bad_pages;
                            break;
                        }
                        std::size_t abs = base + rel;
                        if (abs >= hh.dir_start) {
                            std::cerr << "Attr dir beyond dir_start at slot " << si << " attr "
                                      << ai << " on page " << p << "\n";
                            ++bad_pages;
                            break;
                        }
                    }
                }
            }
        } else if (h->type == static_cast<std::uint16_t>(PageType::HeapOverflow)) {
            ++overflow_pages;
            // Basic check: header + 4-byte length fits
            std::uint32_t l32 = 0;
            std::memcpy(&l32, pg.data() + sizeof(PageHeader), 4);
            std::uint32_t nextpg = 0;
            std::memcpy(&nextpg, pg.data() + sizeof(PageHeader) + 4, 4);
            if (sizeof(PageHeader) + 8 + l32 > pg.size()) {
                std::cerr << "Overflow page " << p << " length out of bounds\n";
                ++bad_pages;
            }
            if (nextpg && nextpg <= p) {
                std::cerr << "Overflow page chain back-link anomaly at " << p << " -> " << nextpg
                          << "\n";
                ++bad_pages;
            }
        } else {
            std::cerr << "Unexpected page type at " << p << ": " << h->type << "\n";
            ++bad_pages;
        }
    }

    std::cout << "Scanned heap pages: " << heap_pages << ", overflow pages: " << overflow_pages
              << ", bad pages: " << bad_pages << "\n";
    if (heap_pages) {
        double avg_slots = static_cast<double>(total_slots) / heap_pages;
        double avg_free = static_cast<double>(total_free) / heap_pages;
        std::cout << "Avg slots/page: " << avg_slots << ", avg free bytes/page: " << avg_free
                  << "\n";
        if (avg_slots < 4.0) {
            std::cout << "Advice: low tuple density; consider VACUUM/REBUILD.\n";
        }
        if (avg_free > page_size * 0.5) {
            std::cout << "Advice: high free space; consider fillfactor tuning.\n";
        }
    }

    return bad_pages ? 3 : 0;
}
