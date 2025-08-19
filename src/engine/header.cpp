#include "scratchbird/engine/header.h"

#include <cstring>

namespace scratchbird::engine
{

    void HeaderManager::append_tlv(std::vector<std::uint8_t>& page, ClumpType t,
                                   const std::vector<std::uint8_t>& v) const
    {
        // Assume header and fixed fields use first 64 bytes; start TLV at offset 64.
        if (page.size() < page_size_)
            page.resize(page_size_, 0);
        std::size_t p = 64;
        // find end marker or first unused slot
        while (p + 2 <= page.size()) {
            std::uint8_t marker = page[p];
            if (marker == static_cast<std::uint8_t>(ClumpType::End) || marker == 0) {
                break;
            }
            std::uint8_t len = page[p + 1];
            p += 2 + len;
        }
        if (p + 2 + v.size() + 1 > page.size())
            return; // no space
        page[p] = static_cast<std::uint8_t>(t);
        page[p + 1] = static_cast<std::uint8_t>(v.size());
        if (!v.empty())
            std::memcpy(&page[p + 2], v.data(), v.size());
        page[p + 2 + v.size()] = static_cast<std::uint8_t>(ClumpType::End);
    }

    void HeaderManager::write_new(const HeaderInfo& info)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = 0;
        hdr->space_id = 1;
        hdr->type = static_cast<std::uint16_t>(ods::PageType::Header);
        hdr->page_size = page_size_;

        // Ensure TLV area starts with an End marker on fresh pages
        if (page.size() >= 66 && page[64] == 0 && page[65] == 0) {
            page[64] = static_cast<std::uint8_t>(ClumpType::End);
            page[65] = 0;
        }

        // ODS version
        {
            std::vector<std::uint8_t> v(4);
            v[0] = static_cast<std::uint8_t>(info.ods_major & 0xFF);
            v[1] = static_cast<std::uint8_t>((info.ods_major >> 8) & 0xFF);
            v[2] = static_cast<std::uint8_t>(info.ods_minor & 0xFF);
            v[3] = static_cast<std::uint8_t>((info.ods_minor >> 8) & 0xFF);
            append_tlv(page, ClumpType::OdsVersion, v);
        }
        // Root pointers
        {
            std::vector<std::uint8_t> v(16, 0);
            std::uint32_t vals[4] = {info.roots.space_catalog, info.roots.generators,
                                     info.roots.first_pip, info.roots.first_tip};
            std::memcpy(v.data(), vals, sizeof(vals));
            append_tlv(page, ClumpType::RootPointers, v);
        }
        if (info.page_cache) {
            std::vector<std::uint8_t> v(4);
            std::uint32_t x = *info.page_cache;
            std::memcpy(v.data(), &x, 4);
            append_tlv(page, ClumpType::PageCache, v);
        }
        if (info.sweep_interval) {
            std::vector<std::uint8_t> v(4);
            std::uint32_t x = *info.sweep_interval;
            std::memcpy(v.data(), &x, 4);
            append_tlv(page, ClumpType::SweepInterval, v);
        }
        if (info.reserve_space) {
            std::vector<std::uint8_t> v(1);
            v[0] = *info.reserve_space;
            append_tlv(page, ClumpType::ReserveSpace, v);
        }
        if (!info.seeded_schemas.empty()) {
            // encode as name\0id (4 bytes LE) repeated
            std::vector<std::uint8_t> v;
            for (const auto& [name, id] : info.seeded_schemas) {
                v.insert(v.end(), name.begin(), name.end());
                v.push_back('\0');
                std::uint32_t le = id;
                std::uint8_t tmp[4];
                std::memcpy(tmp, &le, 4);
                v.insert(v.end(), tmp, tmp + 4);
            }
            append_tlv(page, ClumpType::Schemas, v);
        }
        // Catalog version
        if (info.catalog_major && info.catalog_minor) {
            std::vector<std::uint8_t> v(4);
            v[0] = static_cast<std::uint8_t>(*info.catalog_major & 0xFF);
            v[1] = static_cast<std::uint8_t>((*info.catalog_major >> 8) & 0xFF);
            v[2] = static_cast<std::uint8_t>(*info.catalog_minor & 0xFF);
            v[3] = static_cast<std::uint8_t>((*info.catalog_minor >> 8) & 0xFF);
            append_tlv(page, ClumpType::CatalogVersion, v);
        }
        // Catalog roots (optional)
        if (info.sdb_schema_root_page || info.sdb_object_root_page || info.sdb_relation_root_page ||
            info.sdb_column_root_page || info.sdb_domain_root_page || info.sdb_source_root_page) {
            std::vector<std::uint8_t> v(24, 0);
            std::uint32_t a = info.sdb_schema_root_page.value_or(0);
            std::uint32_t b = info.sdb_object_root_page.value_or(0);
            std::uint32_t c = info.sdb_relation_root_page.value_or(0);
            std::uint32_t d = info.sdb_column_root_page.value_or(0);
            std::uint32_t e = info.sdb_domain_root_page.value_or(0);
            std::uint32_t f = info.sdb_source_root_page.value_or(0);
            std::memcpy(v.data(), &a, 4);
            std::memcpy(v.data() + 4, &b, 4);
            std::memcpy(v.data() + 8, &c, 4);
            std::memcpy(v.data() + 12, &d, 4);
            std::memcpy(v.data() + 16, &e, 4);
            std::memcpy(v.data() + 20, &f, 4);
            append_tlv(page, ClumpType::CatalogRoots, v);
        }

        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(0, page.data());
    }

    HeaderInfo HeaderManager::read() const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(0, page.data());
        HeaderInfo out{};
        out.page_size = page_size_;
        // parse TLVs at offset 64
        std::size_t p = 64;
        while (p + 2 <= page.size() && page[p] != static_cast<std::uint8_t>(ClumpType::End)) {
            std::uint8_t t = page[p];
            std::uint8_t len = page[p + 1];
            const std::uint8_t* data = &page[p + 2];
            switch (static_cast<ClumpType>(t)) {
            case ClumpType::OdsVersion:
                if (len == 4) {
                    out.ods_major = data[0] | (data[1] << 8);
                    out.ods_minor = data[2] | (data[3] << 8);
                }
                break;
            case ClumpType::RootPointers:
                if (len >= 16) {
                    std::memcpy(&out.roots, data, 16);
                }
                break;
            case ClumpType::PageCache:
                if (len == 4) {
                    std::uint32_t x;
                    std::memcpy(&x, data, 4);
                    out.page_cache = x;
                }
                break;
            case ClumpType::SweepInterval:
                if (len == 4) {
                    std::uint32_t x;
                    std::memcpy(&x, data, 4);
                    out.sweep_interval = x;
                }
                break;
            case ClumpType::ReserveSpace:
                if (len == 1) {
                    out.reserve_space = data[0];
                }
                break;
            case ClumpType::Schemas: {
                std::size_t i = 0;
                while (i < len) {
                    // read name until NUL
                    std::size_t start = i;
                    while (i < len && data[i] != '\0')
                        ++i;
                    std::string name(reinterpret_cast<const char*>(data + start), i - start);
                    if (i < len && data[i] == '\0')
                        ++i;
                    if (i + 4 <= len) {
                        std::uint32_t id = 0;
                        std::memcpy(&id, data + i, 4);
                        i += 4;
                        out.seeded_schemas.push_back({name, id});
                    } else
                        break;
                }
                break;
            }
            case ClumpType::CatalogVersion:
                if (len == 4) {
                    out.catalog_major = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
                    out.catalog_minor = static_cast<std::uint16_t>(data[2] | (data[3] << 8));
                }
                break;
            case ClumpType::CatalogRoots:
                if (len >= 8) {
                    std::uint32_t a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
                    std::memcpy(&a, data, 4);
                    std::memcpy(&b, data + 4, 4);
                    if (len >= 12)
                        std::memcpy(&c, data + 8, 4);
                    if (len >= 16)
                        std::memcpy(&d, data + 12, 4);
                    if (len >= 20)
                        std::memcpy(&e, data + 16, 4);
                    if (len >= 24)
                        std::memcpy(&f, data + 20, 4);
                    if (a)
                        out.sdb_schema_root_page = a;
                    if (b)
                        out.sdb_object_root_page = b;
                    if (c)
                        out.sdb_relation_root_page = c;
                    if (d)
                        out.sdb_column_root_page = d;
                    if (e)
                        out.sdb_domain_root_page = e;
                    if (f)
                        out.sdb_source_root_page = f;
                }
                break;
            default:
                break;
            }
            p += 2 + len;
        }
        return out;
    }

    bool HeaderManager::validate() const
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(0, page.data());
        auto* hdr = reinterpret_cast<const ods::PageHeader*>(page.data());
        std::uint32_t saved = hdr->checksum;
        std::vector<std::uint8_t> tmp = page;
        reinterpret_cast<ods::PageHeader*>(tmp.data())->checksum = 0;
        return saved == ods::crc32c(tmp.data(), tmp.size());
    }

} // namespace scratchbird::engine
