/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * sb_timezone_loader - Timezone Data Loader for ScratchBird
 *
 * Loads IANA timezone data from TZif files into the ScratchBird catalog.
 *
 * Usage:
 *   sb_timezone_loader <database_path> [options]
 *
 * Options:
 *   --from <dir>          Load timezones from directory (default: /usr/share/zoneinfo)
 *   --file <path>         Load a single timezone file
 *   --stats               Show statistics after loading
 *   --version             Print tzdata version stored in catalog
 *   --help                Show this help message
 *
 * Examples:
 *   # Load from system zoneinfo directory
 *   sb_timezone_loader /data/mydb.sb
 *
 *   # Load from resources directory
 *   sb_timezone_loader /data/mydb.sb --from resources/timezones
 *
 *   # Load a single timezone
 *   sb_timezone_loader /data/mydb.sb --file /usr/share/zoneinfo/America/New_York
 *
 *   # Load and show statistics
 *   sb_timezone_loader /data/mydb.sb --stats
 */

#include "scratchbird/core/database.h"
#include "scratchbird/core/timezone_loader.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

using namespace scratchbird::core;

void printUsage(const char *program_name)
{
    std::cout << "ScratchBird Timezone Data Loader\n";
    std::cout << "\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program_name << " <database_path> [options]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  --from <dir>      Load timezones from directory\n";
    std::cout << "                    Default: /usr/share/zoneinfo\n";
    std::cout << "  --file <path>     Load a single timezone file\n";
    std::cout << "  --stats           Show statistics after loading\n";
    std::cout << "  --version         Print tzdata version stored in catalog\n";
    std::cout << "  --help            Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  # Load from system zoneinfo directory\n";
    std::cout << "  " << program_name << " /data/mydb.sb\n";
    std::cout << "\n";
    std::cout << "  # Load from resources directory\n";
    std::cout << "  " << program_name << " /data/mydb.sb --from resources/timezones\n";
    std::cout << "\n";
    std::cout << "  # Load a single timezone\n";
    std::cout << "  " << program_name << " /data/mydb.sb --file /usr/share/zoneinfo/America/New_York\n";
    std::cout << "\n";
    std::cout << "  # Load and show statistics\n";
    std::cout << "  " << program_name << " /data/mydb.sb --stats\n";
    std::cout << "\n";
    std::cout << "  # Show catalog tzdata version\n";
    std::cout << "  " << program_name << " /data/mydb.sb --version\n";
    std::cout << "\n";
}

std::string readTzdataVersion(const std::string& zoneinfo_dir)
{
    std::string version_path = zoneinfo_dir + "/version";
    std::ifstream file(version_path);
    if (!file.is_open())
    {
        std::ifstream fallback("resources/timezones/version");
        if (!fallback.is_open())
        {
            return "";
        }
        std::string version;
        std::getline(fallback, version);
        return version;
    }
    std::string version;
    std::getline(file, version);
    return version;
}

int main(int argc, char **argv)
{
    // Parse command-line arguments
    if (argc < 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Show help if requested
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::string db_path = argv[1];
    std::string zoneinfo_dir = "/usr/share/zoneinfo"; // Default
    std::string single_file;
    bool show_stats = false;
    bool show_version = false;

    // Parse options
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--from") == 0)
        {
            if (i + 1 < argc)
            {
                zoneinfo_dir = argv[++i];
            }
            else
            {
                std::cerr << "Error: --from requires a directory path\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "--file") == 0)
        {
            if (i + 1 < argc)
            {
                single_file = argv[++i];
            }
            else
            {
                std::cerr << "Error: --file requires a file path\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "--stats") == 0)
        {
            show_stats = true;
        }
        else if (strcmp(argv[i], "--version") == 0)
        {
            show_version = true;
        }
        else
        {
            std::cerr << "Error: Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "ScratchBird Timezone Data Loader\n";
    std::cout << "=================================\n\n";

    // Open database
    std::cout << "Opening database: " << db_path << "\n";
    ErrorContext ctx;
    Database db;
    Status status = db.open(db_path, &ctx);
    if (status != Status::OK)
    {
        std::cerr << "Error: Failed to open database: " << ctx.message << "\n";
        if (ctx.file && ctx.line > 0)
        {
            std::cerr << "  at " << ctx.file << ":" << ctx.line << "\n";
        }
        return 1;
    }

    std::cout << "Database opened successfully\n\n";

    // Create timezone loader
    CatalogManager *catalog = db.catalog_manager();
    if (!catalog)
    {
        std::cerr << "Error: Failed to get catalog from database\n";
        return 1;
    }

    if (show_version)
    {
        std::string version;
        Status vstatus = catalog->getTimezoneVersion(version, &ctx);
        if (vstatus != Status::OK)
        {
            std::cout << "tzdata_version: <not set>\n";
        }
        else
        {
            std::cout << "tzdata_version: " << version << "\n";
        }
        return 0;
    }

    TimezoneLoader loader(catalog);

    // Load timezone data
    if (!single_file.empty())
    {
        // Load single file
        std::cout << "Loading timezone from file: " << single_file << "\n";
        status = loader.loadFromFile(single_file, &ctx);
    }
    else
    {
        // Load from directory
        std::cout << "Loading timezones from directory: " << zoneinfo_dir << "\n";
        std::cout << "This may take a minute...\n";
        status = loader.loadFromDirectory(zoneinfo_dir, &ctx);
    }

    if (status != Status::OK)
    {
        std::cerr << "\nError: Failed to load timezone data\n";
        std::cerr << "  Status: " << static_cast<uint32_t>(status) << "\n";
        std::cerr << "  Message: " << ctx.message << "\n";
        if (ctx.file && ctx.line > 0)
        {
            std::cerr << "  at " << ctx.file << ":" << ctx.line << "\n";
        }
        return 1;
    }

    std::cout << "\nTimezone data loaded successfully!\n";

    std::string version = readTzdataVersion(zoneinfo_dir);
    if (!version.empty())
    {
        Status vstatus = catalog->setTimezoneVersion(version, &ctx);
        if (vstatus != Status::OK)
        {
            std::cerr << "Warning: Failed to record tzdata version: " << ctx.message << "\n";
        }
        else
        {
            std::cout << "Recorded tzdata version: " << version << "\n";
        }
    }

    // Show statistics if requested
    if (show_stats)
    {
        std::cout << "\nTimezone Statistics:\n";
        std::cout << "-------------------\n";

        size_t total_count = 0;
        size_t with_dst_count = 0;
        Status stat_status = loader.getLoadedTimezoneStats(total_count, with_dst_count, &ctx);

        if (stat_status == Status::OK)
        {
            std::cout << "Total timezones loaded: " << total_count << "\n";
            std::cout << "Timezones with DST: " << with_dst_count << "\n";
            std::cout << "Timezones without DST: " << (total_count - with_dst_count) << "\n";
        }
        else
        {
            std::cout << "(Statistics not available - feature not fully implemented)\n";
        }
    }

    // Database closes automatically when db goes out of scope

    std::cout << "\nDone!\n";
    return 0;
}
