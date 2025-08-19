#ifndef SCRATCHBIRD_ENGINE_HEAP_VALIDATOR_H
#define SCRATCHBIRD_ENGINE_HEAP_VALIDATOR_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/ods.h"

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{
    /**
     * Heap validation error severity levels
     */
    enum class ValidationSeverity : std::uint8_t {
        Info = 0,    // Informational notices (e.g., statistics)
        Warning = 1, // Non-critical issues that should be investigated
        Error = 2,   // Serious corruption that affects data integrity
        Critical = 3 // Severe corruption that makes database unusable
    };

    /**
     * Individual validation error or finding
     */
    struct ValidationIssue {
        ValidationSeverity severity;
        std::string category;          // "page", "tuple", "slot", "overflow", etc.
        std::uint32_t page_no = 0;     // Page number where issue was found
        std::uint16_t slot_no = 0;     // Slot number (if applicable)
        std::string description;       // Human-readable description
        std::string details;           // Technical details and context
        std::uint64_t data_offset = 0; // Byte offset where corruption occurred
    };

    /**
     * Validation statistics and summary
     */
    struct ValidationStats {
        std::uint64_t pages_checked = 0;
        std::uint64_t tuples_checked = 0;
        std::uint64_t slots_checked = 0;
        std::uint64_t bytes_validated = 0;

        std::uint32_t info_count = 0;
        std::uint32_t warning_count = 0;
        std::uint32_t error_count = 0;
        std::uint32_t critical_count = 0;

        bool is_healthy() const
        {
            return error_count == 0 && critical_count == 0;
        }

        bool has_corruption() const
        {
            return error_count > 0 || critical_count > 0;
        }
    };

    /**
     * Complete validation result
     */
    struct ValidationResult {
        ValidationStats stats;
        std::vector<ValidationIssue> issues;
        bool success = true; // True if validation completed without errors
        std::string summary; // Human-readable summary
    };

    /**
     * Heap validation options
     */
    struct ValidationOptions {
        bool check_page_headers = true;     // Validate page header structure
        bool check_checksums = true;        // Verify page checksums
        bool check_slot_directory = true;   // Validate slot directory integrity
        bool check_tuple_headers = true;    // Validate tuple header structure
        bool check_tuple_data = true;       // Validate tuple data consistency
        bool check_overflow_pages = true;   // Validate overflow page chains
        bool check_free_space = true;       // Validate free space tracking
        bool check_cross_references = true; // Check cross-page references
        bool verbose_output = false;        // Include detailed progress output
        std::uint32_t max_issues = 1000;    // Maximum issues to collect (0 = unlimited)
    };

    /**
     * Comprehensive heap validation and corruption detection system
     *
     * Provides extensive validation of heap page structures, tuple integrity,
     * and cross-page consistency. Detects various forms of corruption and
     * provides detailed diagnostic information.
     */
    class HeapValidator
    {
      public:
        /**
         * Constructor
         * @param fmap FileMap for database access
         * @param page_size Database page size
         */
        explicit HeapValidator(FileMap& fmap, std::uint32_t page_size = 4096);

        /**
         * Validate a specific heap relation
         * @param root_page_no Root page number for the heap relation
         * @param tuple_layout Expected tuple layout for validation
         * @param options Validation options
         * @return Validation result with detailed findings
         */
        ValidationResult
        validate_heap_relation(std::uint32_t root_page_no, const TupleLayout& tuple_layout,
                               const ValidationOptions& options = ValidationOptions{});

        /**
         * Validate a single heap page
         * @param page_no Page number to validate
         * @param tuple_layout Expected tuple layout
         * @param options Validation options
         * @return Validation result for this page
         */
        ValidationResult validate_heap_page(std::uint32_t page_no, const TupleLayout& tuple_layout,
                                            const ValidationOptions& options = ValidationOptions{});

        /**
         * Validate all heap pages in a database
         * @param options Validation options
         * @return Comprehensive validation result
         */
        ValidationResult validate_all_heaps(const ValidationOptions& options = ValidationOptions{});

        /**
         * Quick corruption check (faster, less comprehensive)
         * @param root_page_no Root page number for the heap relation
         * @return True if no obvious corruption detected
         */
        bool quick_corruption_check(std::uint32_t root_page_no);

        /**
         * Print detailed validation report
         * @param result Validation result to format
         * @param os Output stream
         */
        void print_validation_report(const ValidationResult& result,
                                     std::ostream& os = std::cout) const;

      private:
        FileMap& fmap_;
        std::uint32_t page_size_;

        // Internal validation methods
        void validate_page_header(const ods::PageHeader& header, std::uint32_t page_no,
                                  ValidationResult& result) const;

        void validate_heap_page_header(const ods::HeapPageHeader& heap_header,
                                       std::uint32_t page_no, ValidationResult& result) const;

        void validate_slot_directory(const std::vector<std::uint8_t>& page_data,
                                     const ods::HeapPageHeader& heap_header, std::uint32_t page_no,
                                     ValidationResult& result) const;

        void validate_tuple(const std::vector<std::uint8_t>& page_data, std::uint16_t slot_offset,
                            std::uint16_t slot_no, const TupleLayout& tuple_layout,
                            std::uint32_t page_no, ValidationResult& result) const;

        void validate_tuple_header(const ods::TupleHeader& tuple_header, std::uint32_t page_no,
                                   std::uint16_t slot_no, ValidationResult& result) const;

        void validate_free_space_integrity(const std::vector<std::uint8_t>& page_data,
                                           const ods::HeapPageHeader& heap_header,
                                           std::uint32_t page_no, ValidationResult& result) const;

        void validate_overflow_references(const std::vector<std::uint8_t>& tuple_data,
                                          const TupleLayout& tuple_layout, std::uint32_t page_no,
                                          std::uint16_t slot_no, ValidationResult& result) const;

        // Helper methods
        void add_issue(ValidationResult& result, ValidationSeverity severity,
                       const std::string& category, std::uint32_t page_no,
                       const std::string& description, const std::string& details = "",
                       std::uint16_t slot_no = 0, std::uint64_t data_offset = 0) const;

        bool verify_page_checksum(const std::vector<std::uint8_t>& page_data) const;

        std::string severity_to_string(ValidationSeverity severity) const;

        std::string format_issue_summary(const ValidationResult& result) const;

        // Read page data safely with error handling
        bool read_page_safe(std::uint32_t page_no, std::vector<std::uint8_t>& page_data) const;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_HEAP_VALIDATOR_H
