/**
 * ScratchBird Database Integrity Check Utility
 *
 * Usage: dbcheck <database_path> [options]
 *
 * Provides comprehensive heap validation and corruption detection,
 * including page-level integrity checks, tuple validation, and
 * cross-reference verification.
 */

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/header.h"
#include "scratchbird/engine/heap_validator.h"

#include <iostream>
#include <string>

void print_usage(const char* program_name)
{
    std::cout << "ScratchBird Database Integrity Check\n";
    std::cout << "====================================\n\n";
    std::cout << "Usage: " << program_name << " <database_path> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --quick           Perform quick corruption check only\n";
    std::cout << "  --verbose         Enable detailed progress output\n";
    std::cout << "  --no-checksums    Skip page checksum verification\n";
    std::cout << "  --no-tuples       Skip tuple-level validation\n";
    std::cout << "  --max-issues N    Limit output to N issues (default: 1000)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " /data/mydb.db\n";
    std::cout << "  " << program_name << " ./test.db --quick\n";
    std::cout << "  " << program_name << " /data/mydb.db --verbose --max-issues 500\n\n";
    std::cout << "Exit codes:\n";
    std::cout << "  0 = No corruption detected\n";
    std::cout << "  1 = Warnings found (investigation recommended)\n";
    std::cout << "  2 = Corruption detected (repair needed)\n";
    std::cout << "  3 = Critical corruption (database may be unusable)\n";
    std::cout << "  4 = Validation failed (I/O error or tool error)\n\n";
}

struct CheckOptions {
    bool quick_check = false;
    bool verbose = false;
    bool check_checksums = true;
    bool check_tuples = true;
    std::uint32_t max_issues = 1000;
};

CheckOptions parse_options(int argc, char* argv[])
{
    CheckOptions opts{};

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--quick") {
            opts.quick_check = true;
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--no-checksums") {
            opts.check_checksums = false;
        } else if (arg == "--no-tuples") {
            opts.check_tuples = false;
        } else if (arg == "--max-issues" && i + 1 < argc) {
            opts.max_issues = std::stoul(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
        }
    }

    return opts;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string db_path = argv[1];
    CheckOptions opts = parse_options(argc, argv);

    // Remove .seg0 suffix if present
    if (db_path.size() >= 5 && db_path.substr(db_path.size() - 5) == ".seg0") {
        db_path = db_path.substr(0, db_path.size() - 5);
    }

    try {
        if (opts.verbose) {
            std::cout << "ScratchBird Database Integrity Check\n";
            std::cout << "Database: " << db_path << "\n";
            std::cout << "Mode: " << (opts.quick_check ? "Quick Check" : "Full Validation")
                      << "\n\n";
        }

        // Set up file access
        scratchbird::engine::FileMap::Layout layout{};
        layout.page_size = 4096; // Default, will be corrected after reading header
        layout.pages_per_segment = 262144;
        layout.options.direct_io = false;

        auto slash = db_path.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path : db_path.substr(slash + 1);

        scratchbird::engine::FileMap fmap(layout);
        fmap.set_base_path(dir, base);

        // Read database header to get correct page size
        scratchbird::engine::HeaderManager hm(std::move(fmap), layout.page_size);
        auto header_info = hm.read();

        // Create new FileMap with correct page size
        layout.page_size = header_info.page_size;
        scratchbird::engine::FileMap fmap2(layout);
        fmap2.set_base_path(dir, base);

        // Create heap validator
        scratchbird::engine::HeapValidator validator(fmap2, header_info.page_size);

        if (opts.quick_check) {
            // Quick corruption check
            std::cout << "Performing quick corruption check...\n";

            // Check a few key pages for obvious corruption
            bool healthy = true;

            // Check page 0 (header)
            if (!validator.quick_corruption_check(0)) {
                std::cout << "❌ Header page corruption detected\n";
                healthy = false;
            }

            // Quick check of catalog pages if they exist
            if (header_info.sdb_object_root_page) {
                if (!validator.quick_corruption_check(*header_info.sdb_object_root_page)) {
                    std::cout << "❌ Object catalog corruption detected\n";
                    healthy = false;
                }
            }

            if (healthy) {
                std::cout << "✅ Quick check passed - no obvious corruption detected\n";
                return 0;
            } else {
                std::cout << "⚠️  Quick check failed - run full validation for details\n";
                return 2;
            }
        } else {
            // Full validation
            scratchbird::engine::ValidationOptions val_opts{};
            val_opts.check_checksums = opts.check_checksums;
            val_opts.check_tuple_headers = opts.check_tuples;
            val_opts.check_tuple_data = opts.check_tuples;
            val_opts.verbose_output = opts.verbose;
            val_opts.max_issues = opts.max_issues;

            std::cout << "Performing comprehensive heap validation...\n\n";

            // Validate critical system heaps
            scratchbird::engine::ValidationResult overall_result{};
            bool validation_success = true;

            // Validate object catalog heap if it exists
            if (header_info.sdb_object_root_page) {
                auto result =
                    validator.validate_heap_relation(*header_info.sdb_object_root_page,
                                                     scratchbird::engine::TupleLayout{}, val_opts);

                if (opts.verbose) {
                    std::cout << "Object Catalog Validation:\n";
                    validator.print_validation_report(result);
                    std::cout << "\n";
                }

                // Merge results
                overall_result.stats.pages_checked += result.stats.pages_checked;
                overall_result.stats.tuples_checked += result.stats.tuples_checked;
                overall_result.stats.slots_checked += result.stats.slots_checked;
                overall_result.stats.bytes_validated += result.stats.bytes_validated;
                overall_result.stats.info_count += result.stats.info_count;
                overall_result.stats.warning_count += result.stats.warning_count;
                overall_result.stats.error_count += result.stats.error_count;
                overall_result.stats.critical_count += result.stats.critical_count;

                overall_result.issues.insert(overall_result.issues.end(), result.issues.begin(),
                                             result.issues.end());

                if (!result.success)
                    validation_success = false;
            }

            // TODO: Add validation of other critical heaps (relations, columns, etc.)

            // Print final report
            overall_result.success = validation_success;
            overall_result.summary = "Database integrity check completed";

            validator.print_validation_report(overall_result);

            // Determine exit code based on findings
            if (overall_result.stats.critical_count > 0) {
                std::cout << "\n💥 CRITICAL: Database has severe corruption and may be unusable\n";
                return 3;
            } else if (overall_result.stats.error_count > 0) {
                std::cout << "\n🔴 ERROR: Database corruption detected - repair recommended\n";
                return 2;
            } else if (overall_result.stats.warning_count > 0) {
                std::cout << "\n🟡 WARNING: Issues found that should be investigated\n";
                return 1;
            } else {
                std::cout << "\n✅ SUCCESS: No corruption detected - database appears healthy\n";
                return 0;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Database integrity check failed: " << e.what() << std::endl;
        return 4;
    }
}
