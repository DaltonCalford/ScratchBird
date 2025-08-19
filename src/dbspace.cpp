/**
 * ScratchBird Database Space Monitor Utility
 *
 * Usage: dbspace <database_path>
 *
 * Provides comprehensive analysis of database space utilization,
 * segment distribution, fragmentation levels, and recommendations
 * for maintenance and optimization.
 */

#include "scratchbird/engine/segment_monitor.h"

#include <iostream>
#include <string>

void print_usage(const char* program_name)
{
    std::cout << "ScratchBird Database Space Monitor\n";
    std::cout << "===================================\n\n";
    std::cout << "Usage: " << program_name << " <database_path>\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " /data/mydb.db\n";
    std::cout << "  " << program_name << " ./test_database.db\n\n";
    std::cout << "The utility will analyze all segments (.seg0, .seg1, etc.) and provide:\n";
    std::cout << "  - Space utilization by segment\n";
    std::cout << "  - Fragmentation analysis\n";
    std::cout << "  - Space pressure alerts\n";
    std::cout << "  - Maintenance recommendations\n\n";
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string db_path = argv[1];

    // Remove .seg0 suffix if present
    if (db_path.size() >= 5 && db_path.substr(db_path.size() - 5) == ".seg0") {
        db_path = db_path.substr(0, db_path.size() - 5);
    }

    try {
        scratchbird::engine::SegmentMonitor monitor(db_path);

        // Print comprehensive space report
        monitor.print_segment_report();

        // Check for critical conditions
        auto db_stats = monitor.get_database_space_stats();

        if (db_stats.space_pressure == scratchbird::engine::SpacePressure::Critical) {
            std::cout << "\n🚨 CRITICAL ALERT: Database is critically low on space!\n";
            return 2; // Exit code 2 for critical space pressure
        } else if (db_stats.space_pressure == scratchbird::engine::SpacePressure::High) {
            std::cout << "\n⚠️  WARNING: Database space pressure is high.\n";
            return 1; // Exit code 1 for high space pressure
        }

        return 0; // Success

    } catch (const std::exception& e) {
        std::cerr << "Error analyzing database: " << e.what() << std::endl;
        return 3;
    }
}
