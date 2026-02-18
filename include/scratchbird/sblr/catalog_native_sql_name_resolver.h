#pragma once

#include <string>

#include "scratchbird/sblr/native_sql_renderer.h"

namespace scratchbird::core {
class CatalogManager;
}

namespace scratchbird::sblr {

class CatalogNativeSqlNameResolver final : public NativeSqlNameResolver {
public:
    explicit CatalogNativeSqlNameResolver(core::CatalogManager* catalog_manager);

    bool resolveNameByUuid(const std::string& uuid_text,
                           NativeSqlObjectTypeHint hint,
                           std::string& resolved_name) override;

private:
    core::CatalogManager* catalog_manager_ = nullptr;
};

}  // namespace scratchbird::sblr

