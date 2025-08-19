#ifndef SCRATCHBIRD_ENGINE_SYSTEM_OIDS_H
#define SCRATCHBIRD_ENGINE_SYSTEM_OIDS_H

#include <array>
#include <cstdint>

namespace scratchbird::engine
{

    using UuidBytes = std::array<std::uint8_t, 16>;

    // Fixed UUIDs reserved for system objects
    const UuidBytes& oid_sys_catalog_schema();
    const UuidBytes& oid_sys_security_schema();
    const UuidBytes& oid_sys_monitoring_schema();
    const UuidBytes& oid_root_schema();
    const UuidBytes& oid_public_schema();

    const UuidBytes& oid_sysdba_user();
    const UuidBytes& oid_public_role();

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_SYSTEM_OIDS_H
