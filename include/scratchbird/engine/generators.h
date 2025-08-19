#ifndef SCRATCHBIRD_ENGINE_GENERATORS_H
#define SCRATCHBIRD_ENGINE_GENERATORS_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>

namespace scratchbird::engine
{

    class GeneratorsManager
    {
      public:
        GeneratorsManager(FileMap fmap, std::uint32_t page_size, std::uint32_t page_no)
            : fmap_(std::move(fmap)), page_size_(page_size), page_no_(page_no)
        {
        }

        // Initialize generator page with header and zeroed counters
        void init_new();

        // Increment and return next IDs
        std::uint64_t next_object_id();
        std::uint64_t next_relation_id();
        std::uint64_t next_index_id();

      private:
        std::uint64_t load_at(std::size_t off);
        void store_at(std::size_t off, std::uint64_t v);

        FileMap fmap_;
        std::uint32_t page_size_{4096};
        std::uint32_t page_no_{0};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_GENERATORS_H
