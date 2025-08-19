#ifndef SCRATCHBIRD_ENGINE_HEADER_H
#define SCRATCHBIRD_ENGINE_HEADER_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    enum class ClumpType : std::uint8_t {
        End = 0xFF,
        PageCache = 1,
        SweepInterval = 2,
        ReserveSpace = 3,
        EncryptionInfo = 4,
        JournalCheckpoint = 5,
        RootPointers = 6,
        OdsVersion = 7,
        Schemas = 8,
        CatalogVersion = 9,
        CatalogRoots = 10
    };

    struct RootPointers {
        std::uint32_t space_catalog{0};
        std::uint32_t generators{0};
        std::uint32_t first_pip{1};
        std::uint32_t first_tip{2};
    };

    struct HeaderInfo {
        std::uint16_t ods_major{14};
        std::uint16_t ods_minor{0};
        std::uint32_t page_size{4096};
        RootPointers roots{};
        std::optional<std::uint32_t> page_cache;                           // pages
        std::optional<std::uint32_t> sweep_interval;                       // seconds
        std::optional<std::uint8_t> reserve_space;                         // 0/1
        std::vector<std::pair<std::string, std::uint32_t>> seeded_schemas; // name->id
        std::optional<std::uint16_t> catalog_major;
        std::optional<std::uint16_t> catalog_minor;
        // Optional root page numbers for key catalog relations
        std::optional<std::uint32_t> sdb_schema_root_page;
        std::optional<std::uint32_t> sdb_object_root_page;
        std::optional<std::uint32_t> sdb_relation_root_page;
        std::optional<std::uint32_t> sdb_column_root_page;
        std::optional<std::uint32_t> sdb_domain_root_page;
        std::optional<std::uint32_t> sdb_source_root_page;
        std::optional<std::uint32_t> sdb_stats_root_page;
    };

    class HeaderManager
    {
      public:
        explicit HeaderManager(FileMap fmap, std::uint32_t page_size)
            : fmap_(std::move(fmap)), page_size_(page_size)
        {
        }

        void write_new(const HeaderInfo& info);
        HeaderInfo read() const;
        bool validate() const;

      private:
        void append_tlv(std::vector<std::uint8_t>& page, ClumpType t,
                        const std::vector<std::uint8_t>& v) const;

        FileMap fmap_;
        std::uint32_t page_size_{4096};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_HEADER_H
