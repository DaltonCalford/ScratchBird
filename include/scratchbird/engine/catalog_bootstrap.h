#ifndef SCRATCHBIRD_ENGINE_CATALOG_BOOTSTRAP_H
#define SCRATCHBIRD_ENGINE_CATALOG_BOOTSTRAP_H

#include <string>

namespace scratchbird::engine
{

    struct BootstrapOptions {
        unsigned catalog_major{1};
        unsigned catalog_minor{0};
    };

    // Generate SQL text that creates system domains, SDB$ catalog tables with comments,
    // seeds core schemas/users, and defines compatibility views.
    std::string generate_catalog_bootstrap_sql(const BootstrapOptions& opts);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_CATALOG_BOOTSTRAP_H
