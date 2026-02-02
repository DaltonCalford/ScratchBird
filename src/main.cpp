/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird Database Engine - Main Entry Point
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include "scratchbird/core/database.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/version.h"

using namespace scratchbird::core;

void print_usage()
{
    std::cout << "Usage:\n";
    std::cout << "  scratchbird --version\n";
    std::cout << "  scratchbird create database <path> [--page-size=<size>]\n";
    std::cout << "  scratchbird open <path>\n";
}

void print_uuid(const ID &uuid)
{
    for (size_t i = 0; i < 16; i++)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
        {
            std::cout << "-";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(uuid.bytes[i]);
    }
    std::cout << std::dec;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "--version")
    {
        std::cout << "ScratchBird v0.1.0-alpha.1.01\n";
        return 0;
    }

    if (command == "create" && argc >= 4 && std::string(argv[2]) == "database")
    {
        std::string db_path = argv[3];
        uint32_t page_size = 16384; // Default

        // Parse optional page size
        for (int i = 4; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg.find("--page-size=") == 0)
            {
                try
                {
                    page_size = std::stoul(arg.substr(12));
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error: Invalid page size value\n";
                    return 1;
                }
            }
        }

        // Validate page size
        if (!isValidAlphaPageSize(page_size))
        {
            std::cerr << "Error: Invalid page size. Must be 8192, 16384, 32768, 65536, or 131072\n";
            return 1;
        }

        // Create database
        Status status = Database::create(db_path, page_size);
        if (status == Status::OK)
        {
            std::cout << "Database created successfully: " << db_path << " (" << (page_size / 1024)
                      << "KB pages)\n";
            return 0;
        }
        else if (status == Status::FILE_EXISTS)
        {
            std::cerr << "Error: Database file already exists: " << db_path << "\n";
            return 1;
        }
        else
        {
            std::cerr << "Error: Failed to create database\n";
            return 1;
        }
    }

    if (command == "open" && argc >= 3)
    {
        std::string db_path = argv[2];

        Database db;
        Status status = db.open(db_path);
        if (status != Status::OK)
        {
            if (status == Status::FILE_NOT_FOUND)
            {
                std::cerr << "Error: Database file not found: " << db_path << "\n";
            }
            else if (status == Status::PAGE_CORRUPT)
            {
                std::cerr << "Error: Database file is corrupt\n";
            }
            else if (status == Status::CHECKSUM_MISMATCH)
            {
                std::cerr << "Error: Database checksum validation failed\n";
            }
            else
            {
                std::cerr << "Error: Failed to open database\n";
            }
            return 1;
        }

        // Simple REPL for database info
        std::cout << "ScratchBird> ";
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == ".info")
            {
                std::cout << "Database: " << db_path << "\n";
                std::cout << "Page Size: " << db.page_size() << "\n";
                std::cout << "Pages: " << db.total_pages() << "\n";
                std::cout << "UUID: ";
                print_uuid(db.uuid());
                std::cout << " (v7)\n";
            }
            else if (line == ".quit" || line == ".exit")
            {
                break;
            }
            else if (!line.empty())
            {
                // Limited command set is intentional for minimal Alpha 1.01 REPL
                // Full SQL execution will be added in future releases
                std::cout << "Unknown command. Try .info or .quit\n";
            }
            std::cout << "ScratchBird> ";
        }

        return 0;
    }

    print_usage();
    return 1;
}