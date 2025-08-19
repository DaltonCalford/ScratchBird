#include "scratchbird/engine/heap_validator.h"

#include "scratchbird/engine/ods.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace scratchbird::engine
{
    HeapValidator::HeapValidator(FileMap& fmap, std::uint32_t page_size)
        : fmap_(fmap), page_size_(page_size)
    {
    }

    ValidationResult HeapValidator::validate_heap_relation(std::uint32_t root_page_no,
                                                           const TupleLayout& tuple_layout,
                                                           const ValidationOptions& options)
    {
        ValidationResult result{};
        auto start_time = std::chrono::steady_clock::now();

        if (options.verbose_output) {
            std::cout << "[HEAP_VAL] Starting validation of heap relation (root page: "
                      << root_page_no << ")" << std::endl;
        }

        try {
            // Step 1: Validate the heap root page
            std::vector<std::uint8_t> root_page_data(page_size_);
            if (!read_page_safe(root_page_no, root_page_data)) {
                add_issue(result, ValidationSeverity::Critical, "page", root_page_no,
                          "Failed to read heap root page", "I/O error or corrupted page");
                result.success = false;
                return result;
            }

            // Validate root page header
            const auto* page_header =
                reinterpret_cast<const ods::PageHeader*>(root_page_data.data());
            validate_page_header(*page_header, root_page_no, result);

            // Check that this is actually a heap root page
            if (static_cast<ods::PageType>(page_header->type) != ods::PageType::HeapRoot) {
                add_issue(result, ValidationSeverity::Error, "page", root_page_no,
                          "Page is not a heap root page",
                          "Expected PageType::HeapRoot, got " + std::to_string(page_header->type));
            }

            // Validate heap root payload
            const auto* heap_root = reinterpret_cast<const ods::HeapRootPayload*>(
                root_page_data.data() + sizeof(ods::PageHeader));

            if (heap_root->version != 1) {
                add_issue(result, ValidationSeverity::Warning, "heap_root", root_page_no,
                          "Unexpected heap root version",
                          "Version " + std::to_string(heap_root->version) + ", expected 1");
            }

            // Step 2: Validate all data pages in the heap chain
            std::unordered_set<std::uint32_t> visited_pages;
            std::uint32_t current_page = heap_root->first_heap_page;

            while (current_page != 0) {
                if (visited_pages.count(current_page)) {
                    add_issue(result, ValidationSeverity::Error, "heap_chain", current_page,
                              "Circular reference in heap page chain",
                              "Page already visited in chain traversal");
                    break;
                }
                visited_pages.insert(current_page);

                auto page_result = validate_heap_page(current_page, tuple_layout, options);

                // Merge results
                result.stats.pages_checked += page_result.stats.pages_checked;
                result.stats.tuples_checked += page_result.stats.tuples_checked;
                result.stats.slots_checked += page_result.stats.slots_checked;
                result.stats.bytes_validated += page_result.stats.bytes_validated;
                result.stats.info_count += page_result.stats.info_count;
                result.stats.warning_count += page_result.stats.warning_count;
                result.stats.error_count += page_result.stats.error_count;
                result.stats.critical_count += page_result.stats.critical_count;

                result.issues.insert(result.issues.end(), page_result.issues.begin(),
                                     page_result.issues.end());

                if (!page_result.success) {
                    result.success = false;
                }

                // Find next page (simplified - would need to read page header for next pointer)
                current_page = 0; // For now, only validate single page

                // Respect max issues limit
                if (options.max_issues > 0 && result.issues.size() >= options.max_issues) {
                    add_issue(result, ValidationSeverity::Warning, "validation", 0,
                              "Maximum issue limit reached",
                              "Stopped validation at " + std::to_string(options.max_issues) +
                                  " issues");
                    break;
                }
            }

            auto end_time = std::chrono::steady_clock::now();
            auto duration_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                    .count();

            if (options.verbose_output) {
                std::cout << "[HEAP_VAL] Validation completed in " << duration_ms << "ms"
                          << std::endl;
            }

            result.summary = format_issue_summary(result);

        } catch (const std::exception& e) {
            add_issue(result, ValidationSeverity::Critical, "validation", 0,
                      "Validation failed with exception", std::string("Exception: ") + e.what());
            result.success = false;
        }

        return result;
    }

    ValidationResult HeapValidator::validate_heap_page(std::uint32_t page_no,
                                                       const TupleLayout& tuple_layout,
                                                       const ValidationOptions& options)
    {
        ValidationResult result{};

        try {
            // Read page data
            std::vector<std::uint8_t> page_data(page_size_);
            if (!read_page_safe(page_no, page_data)) {
                add_issue(result, ValidationSeverity::Critical, "page", page_no,
                          "Failed to read page", "I/O error or corrupted page data");
                result.success = false;
                return result;
            }

            result.stats.pages_checked++;
            result.stats.bytes_validated += page_size_;

            // Validate page header
            const auto* page_header = reinterpret_cast<const ods::PageHeader*>(page_data.data());
            if (options.check_page_headers) {
                validate_page_header(*page_header, page_no, result);
            }

            // Verify this is a heap data page
            if (static_cast<ods::PageType>(page_header->type) != ods::PageType::HeapData) {
                add_issue(result, ValidationSeverity::Error, "page", page_no,
                          "Page is not a heap data page",
                          "Expected PageType::HeapData, got " + std::to_string(page_header->type));
                return result;
            }

            // Validate heap page header
            const auto* heap_header = reinterpret_cast<const ods::HeapPageHeader*>(
                page_data.data() + sizeof(ods::PageHeader));

            if (options.check_slot_directory) {
                validate_heap_page_header(*heap_header, page_no, result);
                validate_slot_directory(page_data, *heap_header, page_no, result);
            }

            // Validate free space tracking
            if (options.check_free_space) {
                validate_free_space_integrity(page_data, *heap_header, page_no, result);
            }

            // Validate individual tuples
            if (options.check_tuple_headers || options.check_tuple_data) {
                std::uint16_t* slot_directory = reinterpret_cast<std::uint16_t*>(
                    page_data.data() + page_size_ -
                    (heap_header->num_slots * sizeof(std::uint16_t)));

                for (std::uint16_t slot = 0; slot < heap_header->num_slots; ++slot) {
                    std::uint16_t slot_offset = slot_directory[slot];
                    result.stats.slots_checked++;

                    if (slot_offset == 0) {
                        // Dead tuple slot - this is normal
                        continue;
                    }

                    if (slot_offset >= page_size_) {
                        add_issue(result, ValidationSeverity::Error, "slot", page_no,
                                  "Slot offset beyond page boundary",
                                  "Slot " + std::to_string(slot) + " offset " +
                                      std::to_string(slot_offset) + " >= page size " +
                                      std::to_string(page_size_),
                                  slot);
                        continue;
                    }

                    validate_tuple(page_data, slot_offset, slot, tuple_layout, page_no, result);
                    result.stats.tuples_checked++;
                }
            }

        } catch (const std::exception& e) {
            add_issue(result, ValidationSeverity::Critical, "page", page_no,
                      "Page validation failed with exception",
                      std::string("Exception: ") + e.what());
            result.success = false;
        }

        return result;
    }

    ValidationResult HeapValidator::validate_all_heaps(const ValidationOptions& options)
    {
        ValidationResult result{};

        if (options.verbose_output) {
            std::cout << "[HEAP_VAL] Starting comprehensive heap validation" << std::endl;
        }

        // This is a simplified implementation
        // Real implementation would scan the catalog to find all heap relations
        add_issue(result, ValidationSeverity::Info, "validation", 0,
                  "Comprehensive heap validation not yet implemented",
                  "Use validate_heap_relation for specific relations");

        result.summary = "Comprehensive validation placeholder - implement catalog scanning";
        return result;
    }

    bool HeapValidator::quick_corruption_check(std::uint32_t root_page_no)
    {
        try {
            std::vector<std::uint8_t> page_data(page_size_);
            if (!read_page_safe(root_page_no, page_data)) {
                return false; // Cannot read page = corruption
            }

            const auto* page_header = reinterpret_cast<const ods::PageHeader*>(page_data.data());

            // Quick checks for obvious corruption
            if (page_header->page_no != root_page_no)
                return false;
            if (page_header->page_size != page_size_)
                return false;
            if (static_cast<ods::PageType>(page_header->type) != ods::PageType::HeapRoot)
                return false;

            // Basic sanity checks passed
            return true;

        } catch (...) {
            return false; // Any exception = corruption
        }
    }

    void HeapValidator::print_validation_report(const ValidationResult& result,
                                                std::ostream& os) const
    {
        os << "ScratchBird Heap Validation Report\n";
        os << "==================================\n\n";

        // Summary statistics
        os << "Validation Summary:\n";
        os << "  Success: " << (result.success ? "YES" : "NO") << "\n";
        os << "  Pages Checked: " << result.stats.pages_checked << "\n";
        os << "  Tuples Checked: " << result.stats.tuples_checked << "\n";
        os << "  Slots Checked: " << result.stats.slots_checked << "\n";
        os << "  Bytes Validated: " << result.stats.bytes_validated << " bytes\n\n";

        // Issue counts by severity
        os << "Issues Found:\n";
        os << "  Critical: " << result.stats.critical_count << "\n";
        os << "  Errors: " << result.stats.error_count << "\n";
        os << "  Warnings: " << result.stats.warning_count << "\n";
        os << "  Info: " << result.stats.info_count << "\n\n";

        // Overall health assessment
        if (result.stats.is_healthy()) {
            os << "🟢 HEAP STATUS: HEALTHY - No corruption detected\n\n";
        } else if (result.stats.has_corruption()) {
            os << "🔴 HEAP STATUS: CORRUPTED - Critical issues found\n\n";
        } else {
            os << "🟡 HEAP STATUS: WARNINGS - Issues require investigation\n\n";
        }

        // Detailed issue listing
        if (!result.issues.empty()) {
            os << "Detailed Issues:\n";
            os << "================\n";

            for (const auto& issue : result.issues) {
                os << "[" << severity_to_string(issue.severity) << "] " << issue.category
                   << " (Page " << issue.page_no;
                if (issue.slot_no > 0) {
                    os << ", Slot " << issue.slot_no;
                }
                os << "): " << issue.description << "\n";

                if (!issue.details.empty()) {
                    os << "    Details: " << issue.details << "\n";
                }

                if (issue.data_offset > 0) {
                    os << "    Offset: 0x" << std::hex << issue.data_offset << std::dec << "\n";
                }
                os << "\n";
            }
        }

        // Summary message
        if (!result.summary.empty()) {
            os << "Summary: " << result.summary << "\n";
        }
    }

    // Private helper methods implementation

    void HeapValidator::validate_page_header(const ods::PageHeader& header, std::uint32_t page_no,
                                             ValidationResult& result) const
    {
        if (header.page_no != page_no) {
            add_issue(result, ValidationSeverity::Error, "page_header", page_no,
                      "Page number mismatch",
                      "Header says " + std::to_string(header.page_no) + ", expected " +
                          std::to_string(page_no));
        }

        if (header.page_size != page_size_) {
            add_issue(result, ValidationSeverity::Error, "page_header", page_no,
                      "Page size mismatch",
                      "Header says " + std::to_string(header.page_size) + ", expected " +
                          std::to_string(page_size_));
        }

        if (header.header_version == 0 || header.header_version > 10) {
            add_issue(result, ValidationSeverity::Warning, "page_header", page_no,
                      "Suspicious header version",
                      "Version " + std::to_string(header.header_version));
        }
    }

    void HeapValidator::validate_heap_page_header(const ods::HeapPageHeader& heap_header,
                                                  std::uint32_t page_no,
                                                  ValidationResult& result) const
    {
        if (heap_header.num_slots > ods::HEAP_MAX_SLOTS_PER_PAGE) {
            add_issue(result, ValidationSeverity::Error, "heap_header", page_no, "Too many slots",
                      "Slots: " + std::to_string(heap_header.num_slots) +
                          ", max: " + std::to_string(ods::HEAP_MAX_SLOTS_PER_PAGE));
        }

        if (heap_header.free_start > page_size_) {
            add_issue(result, ValidationSeverity::Error, "heap_header", page_no,
                      "Free start beyond page boundary",
                      "Free start: " + std::to_string(heap_header.free_start));
        }

        if (heap_header.dir_start > page_size_) {
            add_issue(result, ValidationSeverity::Error, "heap_header", page_no,
                      "Directory start beyond page boundary",
                      "Dir start: " + std::to_string(heap_header.dir_start));
        }

        // Check for slot directory overlap with free space
        std::uint32_t dir_size = heap_header.num_slots * sizeof(std::uint16_t);
        if (heap_header.free_start + dir_size > page_size_) {
            add_issue(result, ValidationSeverity::Error, "heap_header", page_no,
                      "Slot directory overlaps with data",
                      "Free start " + std::to_string(heap_header.free_start) + " + dir size " +
                          std::to_string(dir_size));
        }
    }

    void HeapValidator::validate_slot_directory(const std::vector<std::uint8_t>& page_data,
                                                const ods::HeapPageHeader& heap_header,
                                                std::uint32_t page_no,
                                                ValidationResult& result) const
    {
        if (heap_header.num_slots == 0)
            return;

        const std::uint16_t* slot_directory = reinterpret_cast<const std::uint16_t*>(
            page_data.data() + page_size_ - (heap_header.num_slots * sizeof(std::uint16_t)));

        std::unordered_set<std::uint16_t> used_offsets;

        for (std::uint16_t slot = 0; slot < heap_header.num_slots; ++slot) {
            std::uint16_t offset = slot_directory[slot];

            if (offset == 0)
                continue; // Dead slot

            if (offset >= page_size_) {
                add_issue(result, ValidationSeverity::Error, "slot_directory", page_no,
                          "Slot offset beyond page",
                          "Slot " + std::to_string(slot) + " offset " + std::to_string(offset),
                          slot);
                continue;
            }

            if (used_offsets.count(offset)) {
                add_issue(result, ValidationSeverity::Error, "slot_directory", page_no,
                          "Duplicate slot offset",
                          "Offset " + std::to_string(offset) + " used by multiple slots", slot);
            }
            used_offsets.insert(offset);

            // Check offset is within the data region
            if (offset < sizeof(ods::PageHeader) + sizeof(ods::HeapPageHeader)) {
                add_issue(result, ValidationSeverity::Error, "slot_directory", page_no,
                          "Slot offset in header region",
                          "Slot " + std::to_string(slot) + " offset " + std::to_string(offset),
                          slot);
            }
        }
    }

    void HeapValidator::validate_tuple(const std::vector<std::uint8_t>& page_data,
                                       std::uint16_t slot_offset, std::uint16_t slot_no,
                                       const TupleLayout& tuple_layout, std::uint32_t page_no,
                                       ValidationResult& result) const
    {
        if (slot_offset + sizeof(ods::TupleHeader) > page_size_) {
            add_issue(result, ValidationSeverity::Error, "tuple", page_no,
                      "Tuple header extends beyond page",
                      "Offset " + std::to_string(slot_offset) + " + header size", slot_no);
            return;
        }

        const auto* tuple_header =
            reinterpret_cast<const ods::TupleHeader*>(page_data.data() + slot_offset);

        validate_tuple_header(*tuple_header, page_no, slot_no, result);

        // Validate tuple data size and layout
        std::uint32_t expected_min_size =
            sizeof(ods::TupleHeader) + tuple_header->nullmap_bytes + tuple_header->varlena_bytes;

        if (slot_offset + expected_min_size > page_size_) {
            add_issue(result, ValidationSeverity::Error, "tuple", page_no,
                      "Tuple data extends beyond page",
                      "Size " + std::to_string(expected_min_size) + " at offset " +
                          std::to_string(slot_offset),
                      slot_no);
        }

        // Validate attribute count matches layout
        if (tuple_header->num_attrs != tuple_layout.attrs.size()) {
            add_issue(result, ValidationSeverity::Warning, "tuple", page_no,
                      "Attribute count mismatch",
                      "Tuple has " + std::to_string(tuple_header->num_attrs) + ", layout expects " +
                          std::to_string(tuple_layout.attrs.size()),
                      slot_no);
        }
    }

    void HeapValidator::validate_tuple_header(const ods::TupleHeader& tuple_header,
                                              std::uint32_t page_no, std::uint16_t slot_no,
                                              ValidationResult& result) const
    {
        // Check for reasonable attribute count
        if (tuple_header.num_attrs > 1000) {
            add_issue(result, ValidationSeverity::Warning, "tuple_header", page_no,
                      "Suspicious attribute count",
                      "Attributes: " + std::to_string(tuple_header.num_attrs), slot_no);
        }

        // Check for reasonable nullmap size
        std::uint16_t expected_nullmap = (tuple_header.num_attrs + 7) / 8;
        if (tuple_header.nullmap_bytes > expected_nullmap + 10) {
            add_issue(result, ValidationSeverity::Warning, "tuple_header", page_no,
                      "Suspicious nullmap size",
                      "Nullmap bytes: " + std::to_string(tuple_header.nullmap_bytes) +
                          ", expected ~" + std::to_string(expected_nullmap),
                      slot_no);
        }

        // Check transaction IDs for reasonable values (basic sanity)
        if (tuple_header.created_xid > 0 && tuple_header.deleted_xid > 0 &&
            tuple_header.created_xid >= tuple_header.deleted_xid) {
            add_issue(result, ValidationSeverity::Warning, "tuple_header", page_no,
                      "Invalid transaction ID sequence",
                      "Created XID " + std::to_string(tuple_header.created_xid) +
                          " >= Deleted XID " + std::to_string(tuple_header.deleted_xid),
                      slot_no);
        }
    }

    void HeapValidator::validate_free_space_integrity(const std::vector<std::uint8_t>& page_data,
                                                      const ods::HeapPageHeader& heap_header,
                                                      std::uint32_t page_no,
                                                      ValidationResult& result) const
    {
        (void)page_data; // TODO: Implement free space validation
        (void)heap_header;

        // Placeholder for free space validation
        add_issue(result, ValidationSeverity::Info, "free_space", page_no,
                  "Free space validation not yet implemented",
                  "TODO: Validate free space tracking and fragmentation");
    }

    void HeapValidator::validate_overflow_references(const std::vector<std::uint8_t>& tuple_data,
                                                     const TupleLayout& tuple_layout,
                                                     std::uint32_t page_no, std::uint16_t slot_no,
                                                     ValidationResult& result) const
    {
        (void)tuple_data; // TODO: Implement overflow validation
        (void)tuple_layout;

        // Placeholder for overflow page validation
        add_issue(result, ValidationSeverity::Info, "overflow", page_no,
                  "Overflow page validation not yet implemented",
                  "TODO: Validate overflow page chains and references", slot_no);
    }

    void HeapValidator::add_issue(ValidationResult& result, ValidationSeverity severity,
                                  const std::string& category, std::uint32_t page_no,
                                  const std::string& description, const std::string& details,
                                  std::uint16_t slot_no, std::uint64_t data_offset) const
    {
        ValidationIssue issue{};
        issue.severity = severity;
        issue.category = category;
        issue.page_no = page_no;
        issue.slot_no = slot_no;
        issue.description = description;
        issue.details = details;
        issue.data_offset = data_offset;

        result.issues.push_back(issue);

        // Update counters
        switch (severity) {
        case ValidationSeverity::Info:
            result.stats.info_count++;
            break;
        case ValidationSeverity::Warning:
            result.stats.warning_count++;
            break;
        case ValidationSeverity::Error:
            result.stats.error_count++;
            break;
        case ValidationSeverity::Critical:
            result.stats.critical_count++;
            break;
        }
    }

    bool HeapValidator::verify_page_checksum(const std::vector<std::uint8_t>& page_data) const
    {
        if (page_data.size() < sizeof(ods::PageHeader))
            return false;

        const auto* header = reinterpret_cast<const ods::PageHeader*>(page_data.data());
        std::uint32_t stored_checksum = header->checksum;

        // Calculate checksum with checksum field zeroed
        std::vector<std::uint8_t> temp_data = page_data;
        auto* temp_header = reinterpret_cast<ods::PageHeader*>(temp_data.data());
        temp_header->checksum = 0;

        std::uint32_t calculated_checksum = ods::crc32c(temp_data.data(), temp_data.size());

        return stored_checksum == calculated_checksum;
    }

    std::string HeapValidator::severity_to_string(ValidationSeverity severity) const
    {
        switch (severity) {
        case ValidationSeverity::Info:
            return "INFO";
        case ValidationSeverity::Warning:
            return "WARN";
        case ValidationSeverity::Error:
            return "ERROR";
        case ValidationSeverity::Critical:
            return "CRITICAL";
        default:
            return "UNKNOWN";
        }
    }

    std::string HeapValidator::format_issue_summary(const ValidationResult& result) const
    {
        std::ostringstream oss;

        if (result.stats.has_corruption()) {
            oss << "CORRUPTION DETECTED: " << result.stats.error_count << " errors, "
                << result.stats.critical_count << " critical issues";
        } else if (result.stats.warning_count > 0) {
            oss << "WARNINGS FOUND: " << result.stats.warning_count
                << " issues require investigation";
        } else {
            oss << "VALIDATION PASSED: No corruption detected in " << result.stats.pages_checked
                << " pages";
        }

        return oss.str();
    }

    bool HeapValidator::read_page_safe(std::uint32_t page_no,
                                       std::vector<std::uint8_t>& page_data) const
    {
        try {
            page_data.resize(page_size_);
            fmap_.read_page(page_no, page_data.data());
            return true;
        } catch (...) {
            return false;
        }
    }

} // namespace scratchbird::engine
