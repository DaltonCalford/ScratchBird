/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * MariaDB catalog emulation
 *
 * MariaDB catalog overlays for EF-032 are MySQL-compatible in this phase.
 * We reuse MySQL overlay mappings and expose them under ProtocolType::MARIADB.
 */

#include "scratchbird/catalog/mysql_catalog.h"

namespace scratchbird::catalog {

class MariaDBCatalogHandler : public MySQLCatalogHandler {
public:
    explicit MariaDBCatalogHandler(CatalogManager* catalog)
        : MySQLCatalogHandler(catalog) {}

    ProtocolType getProtocolType() const override {
        return ProtocolType::MARIADB;
    }
};

} // namespace scratchbird::catalog

