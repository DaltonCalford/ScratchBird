#include "scratchbird/engine/catalog_mem.h"

namespace scratchbird::engine
{

    std::mutex CatalogMem::mu_{};
    std::unordered_map<std::string, IndexCatalogEntry> CatalogMem::idx_{};
    std::unordered_map<std::string, StatsEntry> CatalogMem::stats_{};

    void CatalogMem::put_index(const IndexCatalogEntry& e)
    {
        std::lock_guard<std::mutex> lg(mu_);
        idx_[e.name] = e;
    }

    bool CatalogMem::get_index(const std::string& name, IndexCatalogEntry& out)
    {
        std::lock_guard<std::mutex> lg(mu_);
        auto it = idx_.find(name);
        if (it == idx_.end())
            return false;
        out = it->second;
        return true;
    }

    void CatalogMem::put_stats(const std::string& object_name, const StatsEntry& st)
    {
        std::lock_guard<std::mutex> lg(mu_);
        stats_[object_name] = st;
    }

    bool CatalogMem::get_stats(const std::string& object_name, StatsEntry& out)
    {
        std::lock_guard<std::mutex> lg(mu_);
        auto it = stats_.find(object_name);
        if (it == stats_.end())
            return false;
        out = it->second;
        return true;
    }

} // namespace scratchbird::engine
