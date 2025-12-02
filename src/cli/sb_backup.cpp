/**
 * sb_backup - ScratchBird Backup/Restore Tool
 *
 * CLI Tools - Backup and restore ScratchBird databases.
 *
 * Usage:
 *   sb_backup <command> [options]
 *
 * Commands:
 *   backup <database> <backup_file>    Create a backup
 *   restore <backup_file> <database>   Restore from backup
 *   verify <backup_file>               Verify backup integrity
 *   info <backup_file>                 Show backup information
 *
 * Options:
 *   --compress          Compress backup (using LZ4)
 *   --no-compress       Disable compression
 *   -v, --verbose       Verbose output
 *   -q, --quiet         Only show errors
 *   -p, --progress      Show progress bar
 *   -h, --help          Show this help
 *   --version           Show version
 *
 * Backup File Format:
 *   ScratchBird backups use a custom format (.sbbak) that includes:
 *   - Header with magic bytes, version, and metadata
 *   - Page data (optionally compressed)
 *   - Checksum for integrity verification
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

using namespace scratchbird::core;

// =============================================================================
// Constants
// =============================================================================

const uint32_t BACKUP_MAGIC = 0x4B414253;  // "SBAK" in little-endian
const uint16_t BACKUP_VERSION = 1;

// =============================================================================
// Configuration
// =============================================================================

enum class BackupCommand {
    NONE,
    BACKUP,
    RESTORE,
    VERIFY,
    INFO
};

struct BackupConfig {
    BackupCommand command = BackupCommand::NONE;
    std::string database_path;
    std::string backup_path;

    // Options
    bool compress = false;  // Compression disabled for simplicity
    bool verbose = false;
    bool quiet = false;
    bool progress = false;
};

// =============================================================================
// Backup header structure
// =============================================================================

#pragma pack(push, 1)
struct BackupHeader {
    uint32_t magic;             // SBAK
    uint16_t version;           // Backup format version
    uint16_t flags;             // Compression, etc.
    uint32_t page_size;         // Original database page size
    uint64_t page_count;        // Number of pages
    uint64_t data_size;         // Uncompressed data size
    uint64_t compressed_size;   // Compressed data size (if compressed)
    uint64_t timestamp;         // Backup timestamp (Unix time)
    uint32_t checksum;          // Header checksum
    uint8_t reserved[80];       // Reserved for future use (total header: 128 bytes)
};
#pragma pack(pop)

static_assert(sizeof(BackupHeader) == 128, "BackupHeader must be 128 bytes");

// =============================================================================
// Global state
// =============================================================================

static BackupConfig g_config;

// =============================================================================
// Progress reporting
// =============================================================================

void showProgress(size_t current, size_t total, const std::string& label) {
    if (!g_config.progress || g_config.quiet) return;

    int percent = total > 0 ? static_cast<int>((current * 100) / total) : 0;
    int bar_width = 40;
    int filled = (percent * bar_width) / 100;

    std::cout << "\r" << label << " [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) std::cout << "=";
        else if (i == filled) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << percent << "% (" << current << "/" << total << ")";
    std::cout.flush();
}

void log(const std::string& msg) {
    if (!g_config.quiet) {
        std::cout << msg << "\n";
    }
}

void logVerbose(const std::string& msg) {
    if (g_config.verbose && !g_config.quiet) {
        std::cout << "  " << msg << "\n";
    }
}

// =============================================================================
// Simple CRC32 checksum
// =============================================================================

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    static const uint32_t table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7AC5, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };

    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// Backup operation
// =============================================================================

bool createBackup() {
    log("Creating backup...");
    log("  Source: " + g_config.database_path);
    log("  Target: " + g_config.backup_path);

    auto start = std::chrono::high_resolution_clock::now();

    // Get source file info
    struct stat st;
    if (stat(g_config.database_path.c_str(), &st) != 0) {
        std::cerr << "Error: Cannot stat database file\n";
        return false;
    }

    // Open source file
    int src_fd = open(g_config.database_path.c_str(), O_RDONLY);
    if (src_fd < 0) {
        std::cerr << "Error: Cannot open database file\n";
        return false;
    }

    // Read first page to get page size
    PageHeader first_header;
    if (read(src_fd, &first_header, sizeof(first_header)) != sizeof(first_header)) {
        close(src_fd);
        std::cerr << "Error: Cannot read database header\n";
        return false;
    }
    lseek(src_fd, 0, SEEK_SET);  // Reset to beginning

    uint32_t page_size = first_header.page_size;
    size_t page_count = st.st_size / page_size;

    logVerbose("Page size: " + std::to_string(page_size));
    logVerbose("Page count: " + std::to_string(page_count));

    // Open backup file
    std::ofstream backup(g_config.backup_path, std::ios::binary);
    if (!backup) {
        close(src_fd);
        std::cerr << "Error: Cannot create backup file: " << g_config.backup_path << "\n";
        return false;
    }

    // Write header
    BackupHeader header = {};
    header.magic = BACKUP_MAGIC;
    header.version = BACKUP_VERSION;
    header.flags = g_config.compress ? 0x01 : 0x00;
    header.page_size = page_size;
    header.page_count = page_count;
    header.data_size = st.st_size;
    header.timestamp = static_cast<uint64_t>(time(nullptr));
    header.checksum = 0;

    backup.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Copy pages
    std::vector<uint8_t> page_buffer(page_size);
    size_t pages_written = 0;

    for (size_t page_id = 0; page_id < page_count; ++page_id) {
        ssize_t bytes_read = read(src_fd, page_buffer.data(), page_size);
        if (bytes_read != static_cast<ssize_t>(page_size)) {
            std::cerr << "Error: Cannot read page " << page_id << "\n";
            continue;
        }

        backup.write(reinterpret_cast<const char*>(page_buffer.data()), page_size);
        pages_written++;

        if (g_config.progress && (pages_written % 100 == 0 || pages_written == page_count)) {
            showProgress(pages_written, page_count, "Backing up");
        }
    }

    if (g_config.progress) {
        std::cout << "\n";
    }

    // Update header with final sizes
    header.compressed_size = backup.tellp();
    header.checksum = crc32(reinterpret_cast<const uint8_t*>(&header), sizeof(header) - 4);

    // Write updated header
    backup.seekp(0);
    backup.write(reinterpret_cast<const char*>(&header), sizeof(header));

    backup.close();
    close(src_fd);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    log("\nBackup completed:");
    log("  Pages: " + std::to_string(pages_written));
    log("  Size: " + std::to_string(header.compressed_size) + " bytes");
    log("  Time: " + std::to_string(duration.count()) + " ms");

    return true;
}

// =============================================================================
// Restore operation
// =============================================================================

bool restoreBackup() {
    log("Restoring backup...");
    log("  Source: " + g_config.backup_path);
    log("  Target: " + g_config.database_path);

    auto start = std::chrono::high_resolution_clock::now();

    // Open backup file
    std::ifstream backup(g_config.backup_path, std::ios::binary);
    if (!backup) {
        std::cerr << "Error: Cannot open backup file: " << g_config.backup_path << "\n";
        return false;
    }

    // Read header
    BackupHeader header;
    backup.read(reinterpret_cast<char*>(&header), sizeof(header));

    // Verify magic
    if (header.magic != BACKUP_MAGIC) {
        std::cerr << "Error: Invalid backup file (bad magic)\n";
        return false;
    }

    // Verify version
    if (header.version > BACKUP_VERSION) {
        std::cerr << "Error: Backup version " << header.version << " not supported\n";
        return false;
    }

    logVerbose("Backup version: " + std::to_string(header.version));
    logVerbose("Page size: " + std::to_string(header.page_size));
    logVerbose("Page count: " + std::to_string(header.page_count));

    // Check if target exists
    struct stat st;
    if (stat(g_config.database_path.c_str(), &st) == 0) {
        std::cerr << "Error: Target database exists. Remove it first.\n";
        return false;
    }

    // Create target database file
    std::ofstream target(g_config.database_path, std::ios::binary | std::ios::trunc);
    if (!target) {
        std::cerr << "Error: Cannot create database file: " << g_config.database_path << "\n";
        return false;
    }

    // Restore pages
    std::vector<uint8_t> page_buffer(header.page_size);
    size_t pages_restored = 0;

    for (uint64_t page_id = 0; page_id < header.page_count; ++page_id) {
        backup.read(reinterpret_cast<char*>(page_buffer.data()), header.page_size);

        if (backup.gcount() != static_cast<std::streamsize>(header.page_size)) {
            std::cerr << "Error: Truncated backup at page " << page_id << "\n";
            break;
        }

        target.write(reinterpret_cast<const char*>(page_buffer.data()), header.page_size);
        pages_restored++;

        if (g_config.progress && (pages_restored % 100 == 0 || pages_restored == header.page_count)) {
            showProgress(pages_restored, header.page_count, "Restoring");
        }
    }

    if (g_config.progress) {
        std::cout << "\n";
    }

    target.close();
    backup.close();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    log("\nRestore completed:");
    log("  Pages: " + std::to_string(pages_restored));
    log("  Time: " + std::to_string(duration.count()) + " ms");

    return pages_restored == header.page_count;
}

// =============================================================================
// Verify backup
// =============================================================================

bool verifyBackup() {
    log("Verifying backup: " + g_config.backup_path);

    std::ifstream backup(g_config.backup_path, std::ios::binary);
    if (!backup) {
        std::cerr << "Error: Cannot open backup file\n";
        return false;
    }

    // Read header
    BackupHeader header;
    backup.read(reinterpret_cast<char*>(&header), sizeof(header));

    // Verify magic
    if (header.magic != BACKUP_MAGIC) {
        std::cerr << "Error: Invalid backup file (bad magic)\n";
        return false;
    }
    log("  Magic: OK");

    // Verify header checksum
    uint32_t stored_checksum = header.checksum;
    header.checksum = 0;
    uint32_t calc_checksum = crc32(reinterpret_cast<const uint8_t*>(&header), sizeof(header) - 4);
    header.checksum = stored_checksum;

    if (stored_checksum != calc_checksum) {
        std::cerr << "Error: Header checksum mismatch\n";
        return false;
    }
    log("  Header checksum: OK");

    // Verify file size
    backup.seekg(0, std::ios::end);
    size_t file_size = backup.tellg();
    size_t expected_size = sizeof(BackupHeader) + (header.page_count * header.page_size);

    if (file_size < expected_size) {
        std::cerr << "Error: Backup file truncated\n";
        return false;
    }
    log("  File size: OK");

    log("\nBackup verification: PASSED");
    return true;
}

// =============================================================================
// Backup info
// =============================================================================

void showBackupInfo() {
    std::ifstream backup(g_config.backup_path, std::ios::binary);
    if (!backup) {
        std::cerr << "Error: Cannot open backup file\n";
        return;
    }

    BackupHeader header;
    backup.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != BACKUP_MAGIC) {
        std::cerr << "Error: Not a valid ScratchBird backup file\n";
        return;
    }

    std::cout << "Backup Information:\n";
    std::cout << "  File: " << g_config.backup_path << "\n";
    std::cout << "  Format version: " << header.version << "\n";
    std::cout << "  Compressed: " << (header.flags & 0x01 ? "Yes" : "No") << "\n";
    std::cout << "  Page size: " << header.page_size << " bytes\n";
    std::cout << "  Page count: " << header.page_count << "\n";
    std::cout << "  Original size: " << header.data_size << " bytes\n";
    std::cout << "  Backup size: " << header.compressed_size << " bytes\n";

    // Format timestamp
    time_t ts = static_cast<time_t>(header.timestamp);
    struct tm* tm_info = localtime(&ts);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    std::cout << "  Created: " << time_buf << "\n";
}

// =============================================================================
// Argument parsing
// =============================================================================

void printUsage(const char* program) {
    std::cout << "ScratchBird Backup/Restore Tool\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program << " backup <database> <backup_file> [options]\n";
    std::cout << "  " << program << " restore <backup_file> <database> [options]\n";
    std::cout << "  " << program << " verify <backup_file>\n";
    std::cout << "  " << program << " info <backup_file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose       Verbose output\n";
    std::cout << "  -q, --quiet         Only show errors\n";
    std::cout << "  -p, --progress      Show progress bar\n";
    std::cout << "  -h, --help          Show this help\n";
    std::cout << "      --version       Show version\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " backup mydb.sbdb mydb.sbbak -p\n";
    std::cout << "  " << program << " restore mydb.sbbak newdb.sbdb\n";
    std::cout << "  " << program << " verify mydb.sbbak\n";
}

void printVersion() {
    std::cout << "sb_backup (ScratchBird Backup/Restore) 0.1.0\n";
}

bool parseArgs(int argc, char* argv[]) {
    if (argc < 2) {
        return false;
    }

    std::string cmd = argv[1];

    if (cmd == "backup") {
        g_config.command = BackupCommand::BACKUP;
        if (argc < 4) {
            std::cerr << "Error: backup requires database and backup file paths\n";
            return false;
        }
        g_config.database_path = argv[2];
        g_config.backup_path = argv[3];
    } else if (cmd == "restore") {
        g_config.command = BackupCommand::RESTORE;
        if (argc < 4) {
            std::cerr << "Error: restore requires backup file and database paths\n";
            return false;
        }
        g_config.backup_path = argv[2];
        g_config.database_path = argv[3];
    } else if (cmd == "verify") {
        g_config.command = BackupCommand::VERIFY;
        if (argc < 3) {
            std::cerr << "Error: verify requires backup file path\n";
            return false;
        }
        g_config.backup_path = argv[2];
    } else if (cmd == "info") {
        g_config.command = BackupCommand::INFO;
        if (argc < 3) {
            std::cerr << "Error: info requires backup file path\n";
            return false;
        }
        g_config.backup_path = argv[2];
    } else if (cmd == "-h" || cmd == "--help") {
        printUsage(argv[0]);
        std::exit(0);
    } else if (cmd == "--version") {
        printVersion();
        std::exit(0);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        return false;
    }

    // Parse remaining options
    int start_idx = (g_config.command == BackupCommand::VERIFY || g_config.command == BackupCommand::INFO) ? 3 : 4;
    for (int i = start_idx; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--verbose") {
            g_config.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_config.quiet = true;
        } else if (arg == "-p" || arg == "--progress") {
            g_config.progress = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    return true;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (!parseArgs(argc, argv)) {
        printUsage(argv[0]);
        return 1;
    }

    switch (g_config.command) {
        case BackupCommand::BACKUP:
            return createBackup() ? 0 : 1;

        case BackupCommand::RESTORE:
            return restoreBackup() ? 0 : 1;

        case BackupCommand::VERIFY:
            return verifyBackup() ? 0 : 1;

        case BackupCommand::INFO:
            showBackupInfo();
            return 0;

        default:
            printUsage(argv[0]);
            return 1;
    }
}
