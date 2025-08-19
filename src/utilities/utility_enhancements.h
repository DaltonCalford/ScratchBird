#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>

// Forward declarations
namespace SBEnhanced {
    struct QueryResults;
    struct DatabaseStatistics;
    struct PerformanceMetrics;
}

namespace SBEnhanced {

// Output formatting enums
enum class OutputFormat {
    TABLE,          // ASCII table format
    CSV,            // Comma-separated values
    JSON,           // JSON format
    XML,            // XML format
    HTML,           // HTML table format
    MARKDOWN,       // Markdown table format
    FIXED_WIDTH,    // Fixed-width columns
    DELIMITED,      // Custom delimiter
    YAML,           // YAML format
    EXCEL,          // Excel-compatible format
    SQL_INSERT      // SQL INSERT statements
};

enum class DDLFormat {
    STANDARD,       // Standard SQL DDL
    FORMATTED,      // Formatted with indentation
    COMPACT,        // Compact single-line
    COMMENTED,      // With explanatory comments
    STRUCTURED,     // Hierarchical structure
    FIREBIRD,       // Firebird-specific syntax
    ANSI_SQL        // ANSI SQL standard
};

enum class StatFormat {
    SUMMARY,        // Summary statistics
    DETAILED,       // Detailed breakdown
    PERFORMANCE,    // Performance-focused
    COMPARISON,     // Comparison format
    TRENDS,         // Trend analysis
    RECOMMENDATIONS // With recommendations
};

enum class ExportFormat {
    TEXT,           // Plain text
    CSV,            // CSV format
    JSON,           // JSON format
    XML,            // XML format
    HTML,           // HTML format
    PDF,            // PDF format (if supported)
    EXCEL,          // Excel format
    BINARY          // Binary format
};

// Query execution plan
struct QueryPlan {
    std::string plan_text;
    std::vector<std::string> plan_steps;
    std::map<std::string, std::string> plan_metadata;
    std::chrono::microseconds estimated_cost{0};
    std::chrono::microseconds actual_cost{0};
    uint64_t estimated_rows = 0;
    uint64_t actual_rows = 0;
    std::vector<std::string> indexes_used;
    std::vector<std::string> tables_accessed;
    std::vector<std::string> joins_performed;
    std::vector<std::string> optimization_notes;
    std::vector<std::string> performance_warnings;
};

// Performance profile
struct PerformanceProfile {
    std::string query_hash;
    std::chrono::microseconds parse_time{0};
    std::chrono::microseconds compile_time{0};
    std::chrono::microseconds execution_time{0};
    std::chrono::microseconds fetch_time{0};
    uint64_t logical_reads = 0;
    uint64_t physical_reads = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t cpu_time_ms = 0;
    uint64_t io_time_ms = 0;
    uint64_t lock_time_ms = 0;
    uint64_t memory_usage_bytes = 0;
    uint64_t temp_space_bytes = 0;
    std::map<std::string, uint64_t> custom_metrics;
    std::vector<std::string> performance_issues;
    std::vector<std::string> optimization_suggestions;
};

// Table statistics
struct TableStatistics {
    std::string table_name;
    std::string schema_name;
    uint64_t row_count = 0;
    uint64_t data_pages = 0;
    uint64_t index_pages = 0;
    uint64_t blob_pages = 0;
    uint64_t average_row_size = 0;
    uint64_t data_size_bytes = 0;
    uint64_t index_size_bytes = 0;
    uint64_t blob_size_bytes = 0;
    uint64_t total_size_bytes = 0;
    double fragmentation_ratio = 0.0;
    std::chrono::steady_clock::time_point last_updated;
    std::map<std::string, uint64_t> column_statistics;
    std::vector<std::string> indexes;
    std::vector<std::string> constraints;
    std::vector<std::string> triggers;
    std::vector<std::string> dependencies;
};

// Index statistics
struct IndexStatistics {
    std::string index_name;
    std::string table_name;
    std::string schema_name;
    std::vector<std::string> columns;
    bool is_unique = false;
    bool is_primary = false;
    bool is_foreign = false;
    uint64_t leaf_pages = 0;
    uint64_t depth = 0;
    uint64_t distinct_values = 0;
    uint64_t total_values = 0;
    double selectivity = 0.0;
    uint64_t size_bytes = 0;
    double fragmentation_ratio = 0.0;
    std::chrono::steady_clock::time_point last_updated;
    std::map<std::string, uint64_t> usage_statistics;
    std::vector<std::string> optimization_notes;
};

// Schema statistics
struct SchemaStatistics {
    std::string schema_name;
    std::string parent_schema;
    std::vector<std::string> child_schemas;
    uint32_t hierarchy_level = 0;
    uint64_t table_count = 0;
    uint64_t view_count = 0;
    uint64_t procedure_count = 0;
    uint64_t function_count = 0;
    uint64_t trigger_count = 0;
    uint64_t index_count = 0;
    uint64_t constraint_count = 0;
    uint64_t total_objects = 0;
    uint64_t total_size_bytes = 0;
    std::chrono::steady_clock::time_point created_time;
    std::chrono::steady_clock::time_point last_modified_time;
    std::map<std::string, uint64_t> object_type_counts;
    std::vector<std::string> object_dependencies;
};

} // namespace SBEnhanced

// Output formatting class
class OutputFormatter {
private:
    SBEnhanced::OutputFormat default_format = SBEnhanced::OutputFormat::TABLE;
    std::string custom_delimiter = ",";
    bool show_headers = true;
    bool show_row_numbers = false;
    bool show_statistics = false;
    int max_column_width = 50;
    int page_size = 20;
    std::string null_display = "NULL";
    std::string date_format = "%Y-%m-%d %H:%M:%S";
    
public:
    OutputFormatter();
    ~OutputFormatter() = default;
    
    // Configuration methods
    void setDefaultFormat(SBEnhanced::OutputFormat format);
    void setCustomDelimiter(const std::string& delimiter);
    void setShowHeaders(bool show);
    void setShowRowNumbers(bool show);
    void setShowStatistics(bool show);
    void setMaxColumnWidth(int width);
    void setPageSize(int size);
    void setNullDisplay(const std::string& display);
    void setDateFormat(const std::string& format);
    
    // Table formatting
    std::string formatTable(const SBEnhanced::QueryResults& results, 
                           SBEnhanced::OutputFormat format = SBEnhanced::OutputFormat::TABLE);
    std::string formatTableAsCSV(const SBEnhanced::QueryResults& results);
    std::string formatTableAsJSON(const SBEnhanced::QueryResults& results);
    std::string formatTableAsXML(const SBEnhanced::QueryResults& results);
    std::string formatTableAsHTML(const SBEnhanced::QueryResults& results);
    std::string formatTableAsMarkdown(const SBEnhanced::QueryResults& results);
    std::string formatTableAsSQLInsert(const SBEnhanced::QueryResults& results, 
                                      const std::string& table_name);
    
    // DDL formatting
    std::string formatDDL(const std::string& ddl, SBEnhanced::DDLFormat format);
    std::string formatDDLStandard(const std::string& ddl);
    std::string formatDDLFormatted(const std::string& ddl);
    std::string formatDDLCompact(const std::string& ddl);
    std::string formatDDLCommented(const std::string& ddl);
    std::string formatDDLStructured(const std::string& ddl);
    
    // Statistics formatting
    std::string formatStatistics(const SBEnhanced::DatabaseStatistics& stats, 
                                SBEnhanced::StatFormat format);
    std::string formatStatisticsSummary(const SBEnhanced::DatabaseStatistics& stats);
    std::string formatStatisticsDetailed(const SBEnhanced::DatabaseStatistics& stats);
    std::string formatStatisticsPerformance(const SBEnhanced::DatabaseStatistics& stats);
    std::string formatStatisticsComparison(const SBEnhanced::DatabaseStatistics& stats1,
                                          const SBEnhanced::DatabaseStatistics& stats2);
    
    // Performance metrics formatting
    std::string formatPerformanceMetrics(const SBEnhanced::PerformanceMetrics& metrics);
    std::string formatQueryPlan(const SBEnhanced::QueryPlan& plan);
    std::string formatPerformanceProfile(const SBEnhanced::PerformanceProfile& profile);
    
    // Export methods
    bool exportToFile(const std::string& data, const std::string& filename, 
                     SBEnhanced::ExportFormat format);
    bool exportTableToFile(const SBEnhanced::QueryResults& results, 
                          const std::string& filename, SBEnhanced::ExportFormat format);
    bool exportStatisticsToFile(const SBEnhanced::DatabaseStatistics& stats,
                               const std::string& filename, SBEnhanced::ExportFormat format);
    
    // Utility methods
    std::string escapeCSVField(const std::string& field);
    std::string escapeXMLContent(const std::string& content);
    std::string escapeJSONString(const std::string& str);
    std::string escapeHTMLContent(const std::string& content);
    std::string formatNumber(uint64_t number);
    std::string formatBytes(uint64_t bytes);
    std::string formatDuration(const std::chrono::microseconds& duration);
    std::string formatPercentage(double percentage);
    
private:
    // Internal formatting helpers
    std::vector<int> calculateColumnWidths(const SBEnhanced::QueryResults& results);
    std::string padString(const std::string& str, int width, char pad_char = ' ');
    std::string truncateString(const std::string& str, int max_length);
    std::string createTableBorder(const std::vector<int>& widths);
    std::string createTableRow(const std::vector<std::string>& values, 
                              const std::vector<int>& widths);
    std::string indentDDL(const std::string& ddl, int indent_level = 0);
    std::string addDDLComments(const std::string& ddl);
    std::string beautifySQL(const std::string& sql);
};

// Query analysis class
class QueryAnalyzer {
private:
    std::map<std::string, SBEnhanced::QueryPlan> plan_cache;
    std::map<std::string, SBEnhanced::PerformanceProfile> profile_cache;
    bool cache_enabled = true;
    std::chrono::seconds cache_ttl{3600}; // 1 hour
    
public:
    QueryAnalyzer();
    ~QueryAnalyzer() = default;
    
    // Configuration
    void setCacheEnabled(bool enabled);
    void setCacheTTL(const std::chrono::seconds& ttl);
    void clearCache();
    
    // Query analysis
    SBEnhanced::QueryPlan getExecutionPlan(const std::string& sql);
    std::vector<std::string> getOptimizationHints(const std::string& sql);
    SBEnhanced::PerformanceProfile analyzePerformance(const std::string& sql);
    
    // Query optimization
    std::vector<std::string> suggestIndexes(const std::string& sql);
    std::vector<std::string> suggestQueryRewrite(const std::string& sql);
    std::vector<std::string> identifyPerformanceIssues(const std::string& sql);
    
    // Query classification
    bool isSelectQuery(const std::string& sql);
    bool isUpdateQuery(const std::string& sql);
    bool isInsertQuery(const std::string& sql);
    bool isDeleteQuery(const std::string& sql);
    bool isDDLQuery(const std::string& sql);
    bool isComplexQuery(const std::string& sql);
    
    // Statistics and reporting
    std::map<std::string, uint64_t> getQueryStatistics();
    std::vector<std::string> getSlowQueries(const std::chrono::microseconds& threshold);
    std::vector<std::string> getFrequentQueries(uint64_t min_count = 10);
    
private:
    // Internal analysis methods
    std::string normalizeSQL(const std::string& sql);
    std::string calculateQueryHash(const std::string& sql);
    std::vector<std::string> extractTableNames(const std::string& sql);
    std::vector<std::string> extractColumnNames(const std::string& sql);
    std::vector<std::string> extractJoins(const std::string& sql);
    std::vector<std::string> extractWhereConditions(const std::string& sql);
    std::vector<std::string> extractOrderByColumns(const std::string& sql);
    std::vector<std::string> extractGroupByColumns(const std::string& sql);
    
    // Optimization helpers
    bool hasCartesianProduct(const std::string& sql);
    bool hasUnindexedColumns(const std::string& sql);
    bool hasLikeWithLeadingWildcard(const std::string& sql);
    bool hasSelectStar(const std::string& sql);
    bool hasSubqueries(const std::string& sql);
    bool hasComplexJoins(const std::string& sql);
    
    // Cache management
    bool isCacheEntryValid(const std::string& key);
    void addToPlanCache(const std::string& key, const SBEnhanced::QueryPlan& plan);
    void addToProfileCache(const std::string& key, const SBEnhanced::PerformanceProfile& profile);
};

// Statistics collection class
class StatisticsCollector {
private:
    std::map<std::string, std::chrono::steady_clock::time_point> last_collection_times;
    std::chrono::seconds collection_interval{300}; // 5 minutes
    bool auto_refresh = true;
    
public:
    StatisticsCollector();
    ~StatisticsCollector() = default;
    
    // Configuration
    void setCollectionInterval(const std::chrono::seconds& interval);
    void setAutoRefresh(bool enabled);
    
    // Statistics collection
    bool collectDatabaseStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectTableStats(const std::string& table_name, SBEnhanced::TableStatistics& stats);
    bool collectIndexStats(const std::string& index_name, SBEnhanced::IndexStatistics& stats);
    bool collectSchemaStats(const std::string& schema_name, SBEnhanced::SchemaStatistics& stats);
    
    // Batch collection
    bool collectAllTableStats(std::map<std::string, SBEnhanced::TableStatistics>& stats);
    bool collectAllIndexStats(std::map<std::string, SBEnhanced::IndexStatistics>& stats);
    bool collectAllSchemaStats(std::map<std::string, SBEnhanced::SchemaStatistics>& stats);
    
    // Analysis and recommendations
    std::vector<std::string> analyzeFragmentation();
    std::vector<std::string> analyzeIndexUsage();
    std::vector<std::string> analyzeStatisticsAge();
    std::vector<std::string> generateOptimizationRecommendations();
    
    // Trend analysis
    bool trackStatisticsChanges(const std::string& object_name);
    std::vector<std::string> getStatisticsTrends(const std::string& object_name);
    
private:
    // Internal collection methods
    bool collectSystemTableStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectTransactionStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectConnectionStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectPerformanceCounters(SBEnhanced::DatabaseStatistics& stats);
    bool collectSchemaHierarchyStats(SBEnhanced::DatabaseStatistics& stats);
    
    // Utility methods
    bool isCollectionNeeded(const std::string& key);
    void updateCollectionTime(const std::string& key);
    uint64_t calculateTableSize(const std::string& table_name);
    uint64_t calculateIndexSize(const std::string& index_name);
    double calculateFragmentationRatio(const std::string& object_name);
    
    // Query helpers
    bool executeStatisticsQuery(const std::string& sql, std::map<std::string, std::string>& results);
    bool executeCountQuery(const std::string& sql, uint64_t& count);
    bool executeSizeQuery(const std::string& sql, uint64_t& size);
};