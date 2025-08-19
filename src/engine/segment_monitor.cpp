#include "scratchbird/engine/segment_monitor.h"

#include "scratchbird/engine/alloc.h"
#include "scratchbird/engine/file.h"

#include <filesystem>
#include <iomanip>
#include <iostream>

namespace scratchbird::engine
{
    SegmentMonitor::SegmentMonitor(const std::string& db_path, std::uint32_t page_size)
        : db_path_(db_path), page_size_(page_size)
    {
    }

    SegmentStats SegmentMonitor::get_segment_stats(std::size_t segment_index) const
    {
        SegmentStats stats{};
        stats.segment_index = segment_index;

        // Build segment file path
        auto slash = db_path_.find_last_of('/');
        std::string dir =
            (slash == std::string::npos) ? std::string(".") : db_path_.substr(0, slash);
        std::string base = (slash == std::string::npos) ? db_path_ : db_path_.substr(slash + 1);

        std::string segment_file = dir + "/" + base + ".seg" + std::to_string(segment_index);

        // Check if segment file exists
        if (!std::filesystem::exists(segment_file)) {
            stats.exists = false;
            return stats;
        }

        stats.exists = true;

        // Get file size
        try {
            stats.file_size_bytes = std::filesystem::file_size(segment_file);
            stats.total_pages = stats.file_size_bytes / page_size_;
        } catch (const std::exception&) {
            stats.file_size_bytes = 0;
            stats.total_pages = 0;
        }

        // Calculate utilization by examining PIP pages
        // This is a simplified approach - real implementation would scan PIP bitmaps
        stats.allocated_pages = estimate_allocated_pages(segment_index);

        if (stats.total_pages > 0) {
            stats.utilization_percent = (double)stats.allocated_pages / stats.total_pages * 100.0;
        }

        // Calculate fragmentation (simplified metric)
        stats.fragmentation_percent = estimate_fragmentation(segment_index);

        return stats;
    }

    std::vector<SegmentStats> SegmentMonitor::get_all_segment_stats() const
    {
        std::vector<SegmentStats> all_stats;

        // Check for segments until we find one that doesn't exist
        for (std::size_t i = 0; i < MAX_SEGMENTS_TO_CHECK; ++i) {
            auto stats = get_segment_stats(i);
            if (!stats.exists) {
                break;
            }
            all_stats.push_back(stats);
        }

        return all_stats;
    }

    DatabaseSpaceStats SegmentMonitor::get_database_space_stats() const
    {
        DatabaseSpaceStats db_stats{};
        auto segment_stats = get_all_segment_stats();

        db_stats.total_segments = segment_stats.size();

        for (const auto& seg : segment_stats) {
            db_stats.total_file_size_bytes += seg.file_size_bytes;
            db_stats.total_pages += seg.total_pages;
            db_stats.total_allocated_pages += seg.allocated_pages;
        }

        if (db_stats.total_pages > 0) {
            db_stats.overall_utilization_percent =
                (double)db_stats.total_allocated_pages / db_stats.total_pages * 100.0;
        }

        // Calculate average fragmentation
        double total_fragmentation = 0.0;
        for (const auto& seg : segment_stats) {
            total_fragmentation += seg.fragmentation_percent;
        }
        if (!segment_stats.empty()) {
            db_stats.average_fragmentation_percent = total_fragmentation / segment_stats.size();
        }

        // Determine space pressure level
        if (db_stats.overall_utilization_percent >= 90.0) {
            db_stats.space_pressure = SpacePressure::Critical;
        } else if (db_stats.overall_utilization_percent >= 75.0) {
            db_stats.space_pressure = SpacePressure::High;
        } else if (db_stats.overall_utilization_percent >= 50.0) {
            db_stats.space_pressure = SpacePressure::Medium;
        } else {
            db_stats.space_pressure = SpacePressure::Low;
        }

        return db_stats;
    }

    void SegmentMonitor::print_segment_report(std::ostream& os) const
    {
        auto db_stats = get_database_space_stats();
        auto segment_stats = get_all_segment_stats();

        os << "ScratchBird Database Space Report\n";
        os << "=================================\n\n";

        // Database summary
        os << "Database: " << db_path_ << "\n";
        os << "Total Segments: " << db_stats.total_segments << "\n";
        os << "Total Size: " << format_bytes(db_stats.total_file_size_bytes) << "\n";
        os << "Total Pages: " << db_stats.total_pages << " ("
           << format_bytes(db_stats.total_pages * page_size_) << ")\n";
        os << "Allocated Pages: " << db_stats.total_allocated_pages << " ("
           << format_bytes(db_stats.total_allocated_pages * page_size_) << ")\n";
        os << "Overall Utilization: " << std::fixed << std::setprecision(1)
           << db_stats.overall_utilization_percent << "%\n";
        os << "Average Fragmentation: " << std::fixed << std::setprecision(1)
           << db_stats.average_fragmentation_percent << "%\n";
        os << "Space Pressure: " << space_pressure_to_string(db_stats.space_pressure) << "\n\n";

        // Per-segment details
        os << "Segment Details:\n";
        os << "================\n";
        os << std::setw(8) << "Segment" << " " << std::setw(12) << "Size" << " " << std::setw(8)
           << "Pages" << " " << std::setw(10) << "Allocated" << " " << std::setw(8) << "Usage%"
           << " " << std::setw(8) << "Frag%" << "\n";
        os << std::string(60, '-') << "\n";

        for (const auto& seg : segment_stats) {
            os << std::setw(8) << seg.segment_index << " " << std::setw(12)
               << format_bytes(seg.file_size_bytes) << " " << std::setw(8) << seg.total_pages << " "
               << std::setw(10) << seg.allocated_pages << " " << std::setw(7) << std::fixed
               << std::setprecision(1) << seg.utilization_percent << "% " << std::setw(7)
               << std::fixed << std::setprecision(1) << seg.fragmentation_percent << "%\n";
        }

        os << "\n";

        // Recommendations
        print_recommendations(os, db_stats, segment_stats);
    }

    std::uint64_t SegmentMonitor::estimate_allocated_pages(std::size_t segment_index) const
    {
        // This is a simplified estimation
        // Real implementation would read PIP (Page Inventory Page) bitmaps
        // For now, assume 70% allocation on existing segments as a placeholder
        auto stats = get_segment_stats(segment_index);
        if (!stats.exists)
            return 0;

        return static_cast<std::uint64_t>(stats.total_pages * 0.7);
    }

    double SegmentMonitor::estimate_fragmentation(std::size_t segment_index) const
    {
        // This is a simplified estimation
        // Real implementation would analyze page allocation patterns
        // For now, return a moderate fragmentation estimate
        (void)segment_index; // Suppress unused parameter warning
        return 15.0;         // 15% fragmentation as placeholder
    }

    std::string SegmentMonitor::format_bytes(std::uint64_t bytes) const
    {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        double size = static_cast<double>(bytes);
        int unit_index = 0;

        while (size >= 1024.0 && unit_index < 4) {
            size /= 1024.0;
            unit_index++;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
        return oss.str();
    }

    std::string SegmentMonitor::space_pressure_to_string(SpacePressure pressure) const
    {
        switch (pressure) {
        case SpacePressure::Low:
            return "Low";
        case SpacePressure::Medium:
            return "Medium";
        case SpacePressure::High:
            return "High";
        case SpacePressure::Critical:
            return "Critical";
        default:
            return "Unknown";
        }
    }

    void SegmentMonitor::print_recommendations(std::ostream& os, const DatabaseSpaceStats& db_stats,
                                               const std::vector<SegmentStats>& segment_stats) const
    {
        os << "Recommendations:\n";
        os << "================\n";

        if (db_stats.space_pressure == SpacePressure::Critical) {
            os << "⚠️  CRITICAL: Database is > 90% full. Immediate action required!\n";
            os << "   - Consider adding more storage\n";
            os << "   - Run space reclamation procedures\n";
            os << "   - Archive old data\n\n";
        } else if (db_stats.space_pressure == SpacePressure::High) {
            os << "⚠️  WARNING: Database is > 75% full. Plan for expansion soon.\n";
            os << "   - Monitor space usage closely\n";
            os << "   - Prepare for storage expansion\n\n";
        } else if (db_stats.space_pressure == SpacePressure::Low) {
            os << "✅ Space utilization is healthy.\n\n";
        }

        if (db_stats.average_fragmentation_percent > 25.0) {
            os << "⚠️  High fragmentation detected (>" << std::fixed << std::setprecision(1)
               << db_stats.average_fragmentation_percent << "%).\n";
            os << "   - Consider running database defragmentation\n";
            os << "   - Schedule maintenance windows for compaction\n\n";
        }

        // Check for uneven segment utilization
        if (segment_stats.size() > 1) {
            double min_util = 100.0, max_util = 0.0;
            for (const auto& seg : segment_stats) {
                min_util = std::min(min_util, seg.utilization_percent);
                max_util = std::max(max_util, seg.utilization_percent);
            }

            if (max_util - min_util > 30.0) {
                os << "ℹ️  Uneven segment utilization detected.\n";
                os << "   - Consider load balancing across segments\n";
                os << "   - Review allocation patterns\n\n";
            }
        }

        os << "For more detailed analysis, run with verbose monitoring enabled.\n";
    }

} // namespace scratchbird::engine
