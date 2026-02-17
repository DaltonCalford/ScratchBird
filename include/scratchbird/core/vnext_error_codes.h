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

#include <array>
#include <string_view>
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

    struct VNextErrorCodeEntry
    {
        const char *code;
        Status status;
    };

    inline constexpr std::array<VNextErrorCodeEntry, 124> kVNextErrorCodeRegistry{{
        // 01_Physical_Page_Additions_and_Modifications/ERROR_CODE_REGISTRY.md
        {"PGX_0001", Status::PAGE_CORRUPT},
        {"PGX_0002", Status::PAGE_CORRUPT},
        {"PGX_0003", Status::CHECKSUM_MISMATCH},
        {"PGX_0004", Status::PAGE_CORRUPT},
        {"PGX_0005", Status::PAGE_CORRUPT},
        {"PGX_0006", Status::NOT_SUPPORTED},
        {"PGX_0007", Status::INVALID_ARGUMENT},
        {"PGX_0008", Status::PAGE_CORRUPT},

        // 02_Transaction_Core_Rework/ERROR_CODE_REGISTRY.md
        {"TXN_0201", Status::INVALID_ARGUMENT},
        {"TXN_0202", Status::IO_ERROR},
        {"TXN_0203", Status::DATA_CORRUPTED},
        {"TXN_0204", Status::DATA_CORRUPTED},
        {"TXN_0205", Status::INVALID_ARGUMENT},
        {"TXN_0206", Status::PAGE_CORRUPT},
        {"TXN_0207", Status::PAGE_CORRUPT},
        {"TXN_0208", Status::PAGE_CORRUPT},
        {"TXN_0209", Status::PAGE_CORRUPT},
        {"TXN_0210", Status::INVALID_ARGUMENT},
        {"TXN_0211", Status::INVALID_ARGUMENT},
        {"TXN_0212", Status::INVALID_ARGUMENT},
        {"TXN_0213", Status::IO_ERROR},
        {"TXN_0214", Status::DATA_CORRUPTED},
        {"TXN_0215", Status::PAGE_CORRUPT},
        {"TXN_0216", Status::LOCK_CONFLICT},
        {"TXN_0217", Status::LOCK_TIMEOUT},
        {"TXN_0218", Status::INVALID_ARGUMENT},
        {"TXN_0219", Status::DEADLOCK},
        {"TXN_0220", Status::DATA_CORRUPTED},
        {"TXN_0221", Status::NOT_SUPPORTED},

        // 12_Security_Parity_and_Access_Model/ERROR_CODE_REGISTRY.md
        {"SEC_1201", Status::INVALID_AUTHORIZATION},
        {"SEC_1202", Status::INVALID_AUTHORIZATION},
        {"SEC_1203", Status::INVALID_ARGUMENT},
        {"SEC_1204", Status::INVALID_ARGUMENT},
        {"SEC_1205", Status::CONSTRAINT_VIOLATION},
        {"SEC_1206", Status::NOT_SUPPORTED},
        {"SEC_1207", Status::PERMISSION_DENIED},
        {"SEC_1210", Status::INVALID_AUTHORIZATION},
        {"SEC_1211", Status::CONNECTION_FAILURE},
        {"SEC_1212", Status::CONNECTION_FAILURE},
        {"SEC_1213", Status::INVALID_AUTHORIZATION},
        {"SEC_1214", Status::INVALID_AUTHORIZATION},
        {"SEC_1215", Status::INVALID_AUTHORIZATION},
        {"SEC_1216", Status::INVALID_ARGUMENT},
        {"SEC_1217", Status::INVALID_ARGUMENT},
        {"SEC_1218", Status::INVALID_ARGUMENT},
        {"SEC_1219", Status::INVALID_AUTHORIZATION},
        {"SEC_1220", Status::INVALID_AUTHORIZATION},
        {"SEC_1221", Status::NOT_SUPPORTED},
        {"SEC_1222", Status::INVALID_AUTHORIZATION},
        {"SEC_1223", Status::INVALID_ARGUMENT},
        {"SEC_1224", Status::PERMISSION_DENIED},
        {"SEC_1225", Status::INVALID_ARGUMENT},
        {"SEC_1226", Status::NOT_SUPPORTED},
        {"SEC_1230", Status::INVALID_AUTHORIZATION},
        {"SEC_1231", Status::INVALID_AUTHORIZATION},
        {"SEC_1232", Status::INVALID_AUTHORIZATION},
        {"SEC_1233", Status::INVALID_AUTHORIZATION},
        {"SEC_1234", Status::CONSTRAINT_VIOLATION},
        {"SEC_1235", Status::INVALID_ARGUMENT},
        {"SEC_1236", Status::CONSTRAINT_VIOLATION},
        {"SEC_1237", Status::CONSTRAINT_VIOLATION},
        {"SEC_1240", Status::INVALID_AUTHORIZATION},
        {"SEC_1241", Status::INVALID_AUTHORIZATION},
        {"SEC_1242", Status::INVALID_AUTHORIZATION},
        {"SEC_1243", Status::INVALID_AUTHORIZATION},
        {"SEC_1244", Status::INVALID_AUTHORIZATION},
        {"SEC_1245", Status::INVALID_AUTHORIZATION},
        {"SEC_1246", Status::INVALID_ARGUMENT},
        {"SEC_1247", Status::INVALID_AUTHORIZATION},
        {"SEC_1248", Status::INVALID_ARGUMENT},
        {"SEC_1249", Status::PAGE_CORRUPT},
        {"SEC_1250", Status::INVALID_AUTHORIZATION},
        {"SEC_1251", Status::INVALID_AUTHORIZATION},
        {"SEC_1252", Status::INVALID_AUTHORIZATION},
        {"SEC_1253", Status::INVALID_AUTHORIZATION},
        {"SEC_1254", Status::INVALID_AUTHORIZATION},
        {"SEC_1255", Status::INVALID_AUTHORIZATION},
        {"SEC_1256", Status::PAGE_CORRUPT},
        {"SEC_1257", Status::INVALID_ARGUMENT},
        {"SEC_1258", Status::INVALID_AUTHORIZATION},
        {"SEC_1259", Status::PAGE_CORRUPT},
        {"SEC_1289", Status::INVALID_AUTHORIZATION},
        {"SEC_1290", Status::INVALID_AUTHORIZATION},
        {"SEC_1291", Status::INVALID_AUTHORIZATION},
        {"SEC_1292", Status::INVALID_AUTHORIZATION},
        {"SEC_1293", Status::CONNECTION_FAILURE},
        {"SEC_1294", Status::INVALID_AUTHORIZATION},
        {"SEC_1295", Status::NOT_SUPPORTED},

        // 04_AST_SBLR_Compatibility_and_Extensions/ERROR_CODE_REGISTRY.md
        {"IRX_0401", Status::INVALID_ARGUMENT},
        {"IRX_0402", Status::INVALID_ARGUMENT},
        {"IRX_0403", Status::NOT_SUPPORTED},
        {"IRX_0404", Status::INVALID_ARGUMENT},
        {"IRX_0405", Status::INVALID_ARGUMENT},
        {"IRX_0406", Status::NOT_SUPPORTED},
        {"IRX_0407", Status::INVALID_ARGUMENT},

        // 03_Multi_Model_Optimizer_and_Plan_Generator/ERROR_CODE_REGISTRY.md
        {"OPT_0301", Status::INVALID_ARGUMENT},
        {"OPT_0302", Status::INVALID_ARGUMENT},
        {"OPT_0303", Status::NOT_SUPPORTED},
        {"OPT_0304", Status::NOT_SUPPORTED},
        {"OPT_0305", Status::NOT_SUPPORTED},
        {"OPT_0306", Status::INVALID_ARGUMENT},
        {"OPT_0307", Status::INVALID_ARGUMENT},

        // 15_Language_UDR_Compilation_and_Runtime/ERROR_CODE_REGISTRY.md
        {"UDR_1501", Status::INVALID_ARGUMENT},
        {"UDR_1502", Status::NOT_FOUND},
        {"UDR_1503", Status::NOT_SUPPORTED},
        {"UDR_1504", Status::NOT_SUPPORTED},
        {"UDR_1505", Status::SYNTAX_ERROR},
        {"UDR_1506", Status::INVALID_ARGUMENT},
        {"UDR_1507", Status::PERMISSION_DENIED},
        {"UDR_1508", Status::PERMISSION_DENIED},
        {"UDR_1509", Status::INVALID_ARGUMENT},
        {"UDR_1510", Status::INVALID_ARGUMENT},
        {"UDR_1511", Status::CONSTRAINT_VIOLATION},
        {"UDR_1512", Status::CONFIGURATION_LIMIT_EXCEEDED},
        {"UDR_1513", Status::CONSTRAINT_VIOLATION},
        {"UDR_1514", Status::CONSTRAINT_VIOLATION},
        {"UDR_1515", Status::CONSTRAINT_VIOLATION},
        {"UDR_1516", Status::NOT_SUPPORTED},
        {"UDR_1517", Status::NOT_SUPPORTED},
        {"UDR_1518", Status::INVALID_ARGUMENT},
        {"UDR_1519", Status::INVALID_AUTHORIZATION},
        {"UDR_1520", Status::NOT_SUPPORTED},
        {"UDR_1521", Status::NOT_SUPPORTED},
        {"UDR_1522", Status::NOT_SUPPORTED},
    }};

    inline auto lookupVNextErrorCode(std::string_view code) -> const VNextErrorCodeEntry *
    {
        for (const auto &entry : kVNextErrorCodeRegistry)
        {
            if (code == entry.code)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    inline auto isKnownVNextErrorCode(std::string_view code) -> bool
    {
        return lookupVNextErrorCode(code) != nullptr;
    }

    inline auto tryGetStatusForVNextErrorCode(std::string_view code, Status &status_out) -> bool
    {
        const VNextErrorCodeEntry *entry = lookupVNextErrorCode(code);
        if (entry == nullptr)
        {
            return false;
        }
        status_out = entry->status;
        return true;
    }

    inline auto statusMatchesVNextErrorCode(Status status, std::string_view code) -> bool
    {
        Status expected = Status::OK;
        return tryGetStatusForVNextErrorCode(code, expected) && expected == status;
    }

} // namespace scratchbird::core
