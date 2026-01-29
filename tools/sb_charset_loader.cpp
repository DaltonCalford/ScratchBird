/**
 * sb_charset_loader - ScratchBird Character Set Loader
 *
 * Loader tool for character sets and collations.
 *
 * Original Purpose:
 * Command-line tool to load character set and collation data into the database catalog.
 * Supports loading:
 * - Built-in character sets (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)
 * - Character sets from JSON file (resources/charsets/charsets.json)
 * - Collations from JSON file (resources/collations/collations.json)
 *
 * Usage:
 *   sb_charset_loader <database_path> [options]
 *
 * Options:
 *   --builtin              Load built-in charsets only (UTF-8, ASCII, etc.)
 *   --all                  Load all charsets from resources/ directory
 *   --json <file>          Load from specific JSON file
 *   --collations <file>    Load collations from specific JSON file
 *
 * Examples:
 *   sb_charset_loader /data/mydb.sb --builtin
 *   sb_charset_loader /data/mydb.sb --all
 *   sb_charset_loader /data/mydb.sb --json resources/charsets/charsets.json
 *   sb_charset_loader /data/mydb.sb --json resources/charsets/charsets.json --collations resources/collations/collations.json
 */

#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/charset_loader.h"
#include "scratchbird/core/charset_parser.h"
#include "scratchbird/core/error_context.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

using namespace scratchbird::core;

void printUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <database_path> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --builtin              Load built-in charsets only (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)\n";
    std::cout << "  --all                  Load all charsets from resources/ directory (default)\n";
    std::cout << "  --json <file>          Load charsets from specific JSON file\n";
    std::cout << "  --collations <file>    Load collations from specific JSON file\n";
    std::cout << "  --help                 Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " /data/mydb.sb --builtin\n";
    std::cout << "  " << program_name << " /data/mydb.sb --all\n";
    std::cout << "  " << program_name << " /data/mydb.sb --json resources/charsets/charsets.json\n";
    std::cout << "  " << program_name << " /data/mydb.sb --json resources/charsets/charsets.json --collations resources/collations/collations.json\n";
}

std::string readI18nVersion()
{
    std::ifstream file("resources/i18n/version");
    if (!file.is_open())
    {
        return "";
    }
    std::string version;
    std::getline(file, version);
    return version;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    // Check for help flag
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::string db_path = argv[1];
    std::string mode = "--all"; // Default mode
    std::string json_file;
    std::string collations_file;

    // Parse arguments
    for (int i = 2; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--builtin") == 0)
        {
            mode = "--builtin";
        }
        else if (std::strcmp(argv[i], "--all") == 0)
        {
            mode = "--all";
        }
        else if (std::strcmp(argv[i], "--json") == 0)
        {
            if (i + 1 < argc)
            {
                mode = "--json";
                json_file = argv[++i];
            }
            else
            {
                std::cerr << "Error: --json requires a file path argument\n";
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--collations") == 0)
        {
            if (i + 1 < argc)
            {
                collations_file = argv[++i];
            }
            else
            {
                std::cerr << "Error: --collations requires a file path argument\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "Error: Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Open database
    std::cout << "Opening database: " << db_path << "\n";
    ErrorContext ctx;
    Database db;
    Status status = db.open(db_path, &ctx);

    if (status != Status::OK)
    {
        std::cerr << "Failed to open database: " << ctx.message << "\n";
        std::cerr << "Error code: " << static_cast<uint32_t>(ctx.code) << "\n";
        if (ctx.file)
        {
            std::cerr << "Location: " << ctx.file << ":" << ctx.line << " in " << ctx.function << "\n";
        }
        return 1;
    }

    // Get catalog manager
    CatalogManager *catalog = db.catalog_manager();
    if (!catalog)
    {
        std::cerr << "Failed to get catalog manager\n";
        db.close();
        return 1;
    }

    // Create charset loader
    CharsetLoader loader(catalog, &db);

    // Execute based on mode
    if (mode == "--builtin")
    {
        std::cout << "Loading built-in character sets (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)...\n";
        status = loader.loadBuiltinCharsets(&ctx);

        if (status != Status::OK)
        {
            std::cerr << "Failed to load built-in character sets: " << ctx.message << "\n";
            db.close();
            return 1;
        }

        std::cout << "Built-in character sets and default collations loaded successfully!\n";
    }
    else if (mode == "--all")
    {
        std::cout << "Loading all character sets from resources/ directory...\n";
        status = loader.loadFromDirectory("resources/charsets", &ctx);

        if (status != Status::OK)
        {
            std::cerr << "Warning: Some character sets may not have loaded: " << ctx.message << "\n";
            // Continue anyway - best effort
        }

        status = loader.loadFromDirectory("resources/collations", &ctx);

        if (status != Status::OK)
        {
            std::cerr << "Warning: Some collations may not have loaded: " << ctx.message << "\n";
            // Continue anyway - best effort
        }

        std::cout << "Character sets and collations loaded successfully!\n";
    }
    else if (mode == "--json")
    {
        std::cout << "Loading character sets from " << json_file << "...\n";
        status = loader.loadFromJSONFile(json_file, &ctx);

        if (status != Status::OK)
        {
            std::cerr << "Failed to load character sets from JSON: " << ctx.message << "\n";
            db.close();
            return 1;
        }

        std::cout << "Character sets loaded from " << json_file << "\n";

        // Load collations if specified
        if (!collations_file.empty())
        {
            std::cout << "Loading collations from " << collations_file << "...\n";
            status = loader.loadCollationsFromJSONFile(collations_file, &ctx);

            if (status != Status::OK)
            {
                std::cerr << "Failed to load collations from JSON: " << ctx.message << "\n";
                db.close();
                return 1;
            }

            std::cout << "Collations loaded from " << collations_file << "\n";
        }

        std::cout << "Loading complete!\n";
    }

    std::string i18n_version = readI18nVersion();
    if (!i18n_version.empty())
    {
        Status vstatus = catalog->setI18nResourceVersion(i18n_version, &ctx);
        if (vstatus != Status::OK)
        {
            std::cerr << "Warning: Failed to record i18n resource version: " << ctx.message << "\n";
        }
    }

    // Close database
    db.close();

    std::cout << "\nTo verify the loaded character sets, run:\n";
    std::cout << "  SELECT charset_name, description, max_bytes, is_variable_width FROM pg_charsets ORDER BY charset_name;\n";
    std::cout << "\nTo verify the loaded collations, run:\n";
    std::cout << "  SELECT collation_name, charset_name, case_insensitive, accent_insensitive FROM pg_collations ORDER BY collation_name;\n";

    return 0;
}
