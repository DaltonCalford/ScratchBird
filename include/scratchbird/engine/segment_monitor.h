#ifndef SCRATCHBIRD_ENGINE_SEGMENT_MONITOR_H
#define SCRATCHBIRD_ENGINE_SEGMENT_MONITOR_H

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace scratchbird::engine
{
    /**
     * Space pressure levels for database monitoring
     */
    enum class SpacePressure : std::uint8_t {
        Low = 0,     // < 50% utilization
        Medium = 1,  // 50-75% utilization
        High = 2,    // 75-90% utilization
        Critical = 3 // > 90% utilization
    };

    /**
     * Statistics for a single database segment
     */
    struct SegmentStats {
        std::size_t segment_index = 0;
        bool exists = false;
        std::uint64_t file_size_bytes = 0;
        std::uint64_t total_pages = 0;
        std::uint64_t allocated_pages = 0;
        double utilization_percent = 0.0;
        double fragmentation_percent = 0.0;
    };

    /**
     * Overall database space statistics
     */
    struct DatabaseSpaceStats {
        std::size_t total_segments = 0;
        std::uint64_t total_file_size_bytes = 0;
        std::uint64_t total_pages = 0;
        std::uint64_t total_allocated_pages = 0;
        double overall_utilization_percent = 0.0;
        double average_fragmentation_percent = 0.0;
        SpacePressure space_pressure = SpacePressure::Low;
    };

    /**
     * Segment monitoring and space analysis utility
     *
     * Provides comprehensive monitoring of database segment usage,
     * fragmentation analysis, and space pressure alerting.
     */
    class SegmentMonitor
    {
      public:
        /**
         * Constructor
         * @param db_path Path to the database (without .seg0 suffix)
         * @param page_size Database page size in bytes
         */
        explicit SegmentMonitor(const std::string& db_path, std::uint32_t page_size = 4096);

        /**
         * Get statistics for a specific segment
         * @param segment_index Zero-based segment index (0 = .seg0, 1 = .seg1, etc.)
         * @return Segment statistics
         */
        SegmentStats get_segment_stats(std::size_t segment_index) const;

        /**
         * Get statistics for all existing segments
         * @return Vector of segment statistics
         */
        std::vector<SegmentStats> get_all_segment_stats() const;

        /**
         * Get overall database space statistics
         * @return Database-wide space statistics
         */
        DatabaseSpaceStats get_database_space_stats() const;

        /**
         * Print a comprehensive space usage report
         * @param os Output stream to write report to
         */
        void print_segment_report(std::ostream& os = std::cout) const;

        /**
         * Check if database is approaching space limits
         * @return true if space pressure is High or Critical
         */
        bool is_space_pressure_high() const
        {
            auto stats = get_database_space_stats();
            return stats.space_pressure == SpacePressure::High ||
                   stats.space_pressure == SpacePressure::Critical;
        }

        /**
         * Get space utilization percentage for the entire database
         * @return Utilization percentage (0.0 - 100.0)
         */
        double get_overall_utilization() const
        {
            return get_database_space_stats().overall_utilization_percent;
        }

        /**
         * Get average fragmentation across all segments
         * @return Fragmentation percentage (0.0 - 100.0)
         */
        double get_average_fragmentation() const
        {
            return get_database_space_stats().average_fragmentation_percent;
        }

      private:
        static constexpr std::size_t MAX_SEGMENTS_TO_CHECK = 100;

        std::string db_path_;
        std::uint32_t page_size_;

        /**
         * Estimate allocated pages in a segment by analyzing PIP bitmaps
         * @param segment_index Segment to analyze
         * @return Estimated number of allocated pages
         */
        std::uint64_t estimate_allocated_pages(std::size_t segment_index) const;

        /**
         * Estimate fragmentation level in a segment
         * @param segment_index Segment to analyze
         * @return Fragmentation percentage
         */
        double estimate_fragmentation(std::size_t segment_index) const;

        /**
         * Format byte count with appropriate units (KB, MB, GB, etc.)
         * @param bytes Byte count to format
         * @return Formatted string with units
         */
        std::string format_bytes(std::uint64_t bytes) const;

        /**
         * Convert SpacePressure enum to string
         * @param pressure Space pressure level
         * @return String representation
         */
        std::string space_pressure_to_string(SpacePressure pressure) const;

        /**
         * Print optimization and maintenance recommendations
         * @param os Output stream
         * @param db_stats Database statistics
         * @param segment_stats Individual segment statistics
         */
        void print_recommendations(std::ostream& os, const DatabaseSpaceStats& db_stats,
                                   const std::vector<SegmentStats>& segment_stats) const;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_SEGMENT_MONITOR_H
