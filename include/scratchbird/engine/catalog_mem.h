#ifndef SCRATCHBIRD_ENGINE_CATALOG_MEM_H
#define SCRATCHBIRD_ENGINE_CATALOG_MEM_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace scratchbird::engine
{

    struct IndexCatalogEntry {
        std::string name;
        std::string relation;
        std::string method; // BTREE etc
        bool active{false};
        bool valid{false};
        std::uint32_t root_page{0};
    };

    struct StatsEntry {
        std::uint32_t height{0};
        std::uint32_t leaf_pages{0};
        std::uint32_t branch_pages{0};
        std::uint64_t key_count{0};
    };

    class CatalogMem
    {
      public:
        static void put_index(const IndexCatalogEntry& e);
        static bool get_index(const std::string& name, IndexCatalogEntry& out);
        static void put_stats(const std::string& object_name, const StatsEntry& st);
        static bool get_stats(const std::string& object_name, StatsEntry& out);

      private:
        static std::mutex mu_;
        static std::unordered_map<std::string, IndexCatalogEntry> idx_;
        static std::unordered_map<std::string, StatsEntry> stats_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_CATALOG_MEM_H
