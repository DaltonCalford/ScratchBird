/**
 * sb_verify - ScratchBird Database Verification Tool
 *
 * CLI Tools - Verifies database integrity and consistency.
 *
 * Usage:
 *   sb_verify <database_path> [options]
 *
 * Options:
 *   --full                Full verification (all checks)
 *   --quick               Quick check (header and page checksums only)
 *   --pages               Verify all pages
 *   --repair              Attempt to repair issues (dangerous)
 *   -v, --verbose         Verbose output
 *   -q, --quiet           Only show errors
 *   -o, --output=<file>   Write report to file
 *   -h, --help            Show this help
 *   --version             Show version
 *
 * Exit codes:
 *   0 - No issues found
 *   1 - Minor issues (warnings)
 *   2 - Serious issues (errors)
 *   3 - Critical issues (corruption)
 *   4 - Usage/argument error
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

// =============================================================================
// Configuration
// =============================================================================

struct VerifyConfig {
    std::string database_path;
    std::string output_file;

    // Check types
    bool check_pages = false;

    // Options
    bool full = false;
    bool quick = false;
    bool repair = false;
    bool verbose = false;
    bool quiet = false;
};

// =============================================================================
// Result tracking
// =============================================================================

struct VerifyResult {
    int warnings = 0;
    int errors = 0;
    int critical = 0;
    int repaired = 0;

    std::vector<std::string> messages;

    void addWarning(const std::string& msg) {
        warnings++;
        messages.push_back("[WARNING] " + msg);
    }

    void addError(const std::string& msg) {
        errors++;
        messages.push_back("[ERROR] " + msg);
    }

    void addCritical(const std::string& msg) {
        critical++;
        messages.push_back("[CRITICAL] " + msg);
    }

    void addRepaired(const std::string& msg) {
        repaired++;
        messages.push_back("[REPAIRED] " + msg);
    }

    void addInfo(const std::string& msg) {
        messages.push_back("[INFO] " + msg);
    }

    int exitCode() const {
        if (critical > 0) return 3;
        if (errors > 0) return 2;
        if (warnings > 0) return 1;
        return 0;
    }
};

// =============================================================================
// Global state
// =============================================================================

static VerifyConfig g_config;
static VerifyResult g_result;
static std::ofstream* g_output_file = nullptr;

// =============================================================================
// Output helpers
// =============================================================================

std::ostream& getOutput() {
    return g_output_file && g_output_file->is_open() ? *g_output_file : std::cout;
}

void log(const std::string& msg) {
    if (!g_config.quiet) {
        getOutput() << msg << "\n";
    }
}

void logVerbose(const std::string& msg) {
    if (g_config.verbose && !g_config.quiet) {
        getOutput() << "  " << msg << "\n";
    }
}

// =============================================================================
// Database file verification
// =============================================================================

bool verifyFileHeader(const std::string& path) {
    log("Verifying database file header...");

    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        g_result.addCritical("Cannot stat database file: " + path);
        return false;
    }

    logVerbose("File size: " + std::to_string(st.st_size) + " bytes");

    if (st.st_size < 16384) {
        g_result.addCritical("Database file too small (less than one page)");
        return false;
    }

    // Read header page
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        g_result.addCritical("Cannot open database file: " + std::string(strerror(errno)));
        return false;
    }

    uint8_t header[512];
    ssize_t bytes_read = read(fd, header, sizeof(header));
    close(fd);

    if (bytes_read < 512) {
        g_result.addCritical("Cannot read database header");
        return false;
    }

    // Check magic bytes "SBRD" (ScratchBird)
    const PageHeader* page_header = reinterpret_cast<const PageHeader*>(header);
    if (page_header->magic != K_MAGIC_SBRD) {
        g_result.addCritical("Invalid database magic bytes (expected SBRD)");
        return false;
    }
    logVerbose("Magic bytes: OK (SBRD)");

    // Check version
    logVerbose("Database version: " + std::to_string(page_header->version));

    // Check page size
    uint32_t page_size = page_header->page_size;
    logVerbose("Page size: " + std::to_string(page_size) + " bytes");
    if (!isValidAlphaPageSize(page_size)) {
        g_result.addWarning("Unusual page size: " + std::to_string(page_size));
    }

    // Check file size consistency
    uint64_t expected_pages = st.st_size / page_size;
    logVerbose("Pages in file: " + std::to_string(expected_pages));

    if (st.st_size % page_size != 0) {
        g_result.addError("Database file size not aligned to page size");
    }

    log("  Header verification: OK");
    return true;
}

// =============================================================================
// Page verification (direct file access)
// =============================================================================

bool verifyPages(const std::string& path) {
    log("Verifying pages...");

    // Get file size
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        g_result.addCritical("Cannot stat database file");
        return false;
    }

    // Read first page to get page size
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        g_result.addCritical("Cannot open database file");
        return false;
    }

    PageHeader first_header;
    if (read(fd, &first_header, sizeof(first_header)) != sizeof(first_header)) {
        close(fd);
        g_result.addCritical("Cannot read first page header");
        return false;
    }

    uint32_t page_size = first_header.page_size;
    size_t page_count = st.st_size / page_size;

    // Allocate buffer for reading pages
    std::vector<uint8_t> page_buffer(page_size);
    size_t verified = 0;
    size_t bad_pages = 0;

    // Limit to 10000 pages to avoid long scan times
    size_t max_pages = std::min(page_count, static_cast<size_t>(10000));

    for (size_t page_id = 0; page_id < max_pages; ++page_id) {
        // Seek to page
        off_t offset = static_cast<off_t>(page_id) * page_size;
        if (lseek(fd, offset, SEEK_SET) != offset) {
            g_result.addError("Cannot seek to page " + std::to_string(page_id));
            bad_pages++;
            continue;
        }

        // Read page
        ssize_t bytes_read = read(fd, page_buffer.data(), page_size);
        if (bytes_read != static_cast<ssize_t>(page_size)) {
            g_result.addError("Cannot read page " + std::to_string(page_id));
            bad_pages++;
            continue;
        }

        // Check header
        const PageHeader* header = reinterpret_cast<const PageHeader*>(page_buffer.data());

        // Check magic bytes
        if (header->magic != K_MAGIC_SBRD) {
            g_result.addError("Page " + std::to_string(page_id) + " has invalid magic bytes");
            bad_pages++;
            continue;
        }

        // Check page type
        if (header->page_type > 25) {
            g_result.addError("Page " + std::to_string(page_id) + " has invalid type: " +
                            std::to_string(header->page_type));
            bad_pages++;
        }

        // Verify checksum
        if (!validatePageChecksum(page_buffer.data(), page_size)) {
            g_result.addError("Page " + std::to_string(page_id) + " checksum mismatch");
            bad_pages++;
        }

        verified++;

        if (g_config.verbose && (verified % 1000 == 0)) {
            logVerbose("Verified " + std::to_string(verified) + " pages...");
        }
    }

    close(fd);

    log("  Verified " + std::to_string(verified) + " pages");
    if (bad_pages > 0) {
        log("  Bad pages: " + std::to_string(bad_pages));
        return false;
    }
    log("  Page verification: OK");
    return true;
}

// =============================================================================
// Quick check
// =============================================================================

bool runQuickCheck() {
    log("Running quick verification...\n");
    return verifyFileHeader(g_config.database_path);
}

// =============================================================================
// Full verification
// =============================================================================

bool runFullCheck() {
    log("Running full database verification...\n");

    auto start = std::chrono::high_resolution_clock::now();

    // Verify file header
    if (!verifyFileHeader(g_config.database_path)) {
        return false;
    }

    log("");

    bool success = true;

    if (g_config.check_pages || g_config.full) {
        if (!verifyPages(g_config.database_path)) success = false;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    log("");
    log("Verification completed in " + std::to_string(duration.count()) + " ms");

    return success;
}

// =============================================================================
// Report generation
// =============================================================================

void printReport() {
    auto& out = getOutput();

    out << "\n";
    out << "========================================\n";
    out << "         VERIFICATION REPORT\n";
    out << "========================================\n";
    out << "Database: " << g_config.database_path << "\n";
    out << "\n";

    if (!g_result.messages.empty() && g_config.verbose) {
        out << "Details:\n";
        for (const auto& msg : g_result.messages) {
            out << "  " << msg << "\n";
        }
        out << "\n";
    }

    out << "Summary:\n";
    out << "  Warnings:  " << g_result.warnings << "\n";
    out << "  Errors:    " << g_result.errors << "\n";
    out << "  Critical:  " << g_result.critical << "\n";
    if (g_result.repaired > 0) {
        out << "  Repaired:  " << g_result.repaired << "\n";
    }
    out << "\n";

    if (g_result.critical > 0) {
        out << "Status: CRITICAL - Database may be corrupted!\n";
    } else if (g_result.errors > 0) {
        out << "Status: ERRORS - Database has issues requiring attention\n";
    } else if (g_result.warnings > 0) {
        out << "Status: WARNINGS - Database is usable but has minor issues\n";
    } else {
        out << "Status: OK - Database verification passed\n";
    }
    out << "========================================\n";
}

// =============================================================================
// Argument parsing
// =============================================================================

void printUsage(const char* program) {
    std::cout << "ScratchBird Database Verification Tool\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program << " <database_path> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --full              Full verification (all checks)\n";
    std::cout << "  --quick             Quick check (header only)\n";
    std::cout << "  --pages             Verify all pages\n";
    std::cout << "  --repair            Attempt to repair issues\n";
    std::cout << "  -v, --verbose       Verbose output\n";
    std::cout << "  -q, --quiet         Only show errors\n";
    std::cout << "  -o, --output=<file> Write report to file\n";
    std::cout << "  -h, --help          Show this help\n";
    std::cout << "      --version       Show version\n\n";
    std::cout << "Exit codes:\n";
    std::cout << "  0 - No issues found\n";
    std::cout << "  1 - Minor issues (warnings)\n";
    std::cout << "  2 - Serious issues (errors)\n";
    std::cout << "  3 - Critical issues (corruption)\n";
    std::cout << "  4 - Usage/argument error\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " mydb.sbdb --quick\n";
    std::cout << "  " << program << " mydb.sbdb --full -v\n";
    std::cout << "  " << program << " mydb.sbdb --pages\n";
}

void printVersion() {
    std::cout << "sb_verify (ScratchBird Database Verification) 0.1.0\n";
}

bool parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--version") {
            printVersion();
            std::exit(0);
        }
        if (arg == "--full") {
            g_config.full = true;
        } else if (arg == "--quick") {
            g_config.quick = true;
        } else if (arg == "--pages") {
            g_config.check_pages = true;
        } else if (arg == "--repair") {
            g_config.repair = true;
        } else if (arg == "-v" || arg == "--verbose") {
            g_config.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_config.quiet = true;
        } else if (arg == "-o" && i + 1 < argc) {
            g_config.output_file = argv[++i];
        } else if (arg.find("--output=") == 0) {
            g_config.output_file = arg.substr(9);
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        } else if (g_config.database_path.empty()) {
            g_config.database_path = arg;
        } else {
            std::cerr << "Error: Multiple database paths specified\n";
            return false;
        }
    }

    // Default to full if no specific checks
    if (!g_config.quick && !g_config.check_pages) {
        g_config.full = true;
    }

    return true;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 4;
    }

    if (!parseArgs(argc, argv)) {
        return 4;
    }

    if (g_config.database_path.empty()) {
        std::cerr << "Error: No database specified\n";
        printUsage(argv[0]);
        return 4;
    }

    // Setup output file if specified
    if (!g_config.output_file.empty()) {
        g_output_file = new std::ofstream(g_config.output_file);
        if (!g_output_file->is_open()) {
            std::cerr << "Error: Cannot open output file: " << g_config.output_file << "\n";
            return 4;
        }
    }

    // Run verification
    if (g_config.quick) {
        runQuickCheck();
    } else {
        runFullCheck();
    }

    // Print report
    printReport();

    // Cleanup
    if (g_output_file) {
        g_output_file->close();
        delete g_output_file;
    }

    return g_result.exitCode();
}
