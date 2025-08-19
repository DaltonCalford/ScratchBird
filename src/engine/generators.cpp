#include "scratchbird/engine/generators.h"

#include <cstring>
#include <vector>

namespace scratchbird::engine
{

    static constexpr std::size_t kHdrReserve = 64;
    static constexpr std::size_t kOffObject = kHdrReserve + 0;
    static constexpr std::size_t kOffRelation = kHdrReserve + 8;
    static constexpr std::size_t kOffIndex = kHdrReserve + 16;

    void GeneratorsManager::init_new()
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->page_no = page_no_;
        hdr->space_id = 1;
        hdr->type = static_cast<std::uint16_t>(ods::PageType::Generator);
        hdr->page_size = page_size_;
        std::uint64_t zero = 0;
        std::memcpy(&page[kOffObject], &zero, 8);
        std::memcpy(&page[kOffRelation], &zero, 8);
        std::memcpy(&page[kOffIndex], &zero, 8);
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(page_no_, page.data());
    }

    std::uint64_t GeneratorsManager::load_at(std::size_t off)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(page_no_, page.data());
        std::uint64_t v = 0;
        std::memcpy(&v, &page[off], 8);
        return v;
    }

    void GeneratorsManager::store_at(std::size_t off, std::uint64_t v)
    {
        std::vector<std::uint8_t> page(page_size_, 0);
        fmap_.read_page(page_no_, page.data());
        std::memcpy(&page[off], &v, 8);
        auto* hdr = reinterpret_cast<ods::PageHeader*>(page.data());
        hdr->checksum = 0;
        hdr->checksum = ods::crc32c(page.data(), page.size());
        fmap_.write_page(page_no_, page.data());
    }

    std::uint64_t GeneratorsManager::next_object_id()
    {
        auto v = load_at(kOffObject) + 1;
        store_at(kOffObject, v);
        return v;
    }

    std::uint64_t GeneratorsManager::next_relation_id()
    {
        auto v = load_at(kOffRelation) + 1;
        store_at(kOffRelation, v);
        return v;
    }

    std::uint64_t GeneratorsManager::next_index_id()
    {
        auto v = load_at(kOffIndex) + 1;
        store_at(kOffIndex, v);
        return v;
    }

} // namespace scratchbird::engine
