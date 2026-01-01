#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/data_masking.h"

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class BufferPool;

    using ID = UuidV7Bytes;

    /**
     * DomainType - Type of domain
     */
    enum class DomainType : uint8_t
    {
        BASIC = 0,    // Basic domain (wraps a base type with constraints)
        RECORD = 1,   // RECORD/ROW type with named fields
        ENUM = 2,     // ENUM type with ordered values
        SET = 3,      // SET type with unique unordered values
        VARIANT = 4   // VARIANT type (runtime polymorphic)
    };

    /**
     * Domain constraint types
     */
    enum class ConstraintType : uint8_t
    {
        CHECK = 0,      // CHECK constraint (expression)
        NOT_NULL = 1,   // NOT NULL constraint
        UNIQUE = 2,     // UNIQUE constraint
        DEFAULT = 3     // DEFAULT value
    };

    /**
     * Domain constraint information
     */
    struct DomainConstraint
    {
        ConstraintType type;
        std::string expression;  // SQL expression for CHECK, value for DEFAULT
        std::string name;        // Optional constraint name

        DomainConstraint() : type(ConstraintType::CHECK) {}
        DomainConstraint(ConstraintType t, const std::string& expr, const std::string& n = "")
            : type(t), expression(expr), name(n) {}
    };

    /**
     * RECORD field definition
     */
    struct RecordField
    {
        std::string name;
        DataType type;
        uint32_t precision;
        uint32_t scale;
        bool nullable;
        ID domain_id;  // If field uses a domain instead of base type

        RecordField() : type(DataType::UNKNOWN), precision(0), scale(0), nullable(true) {}
        RecordField(const std::string& n, DataType t, bool null = true)
            : name(n), type(t), precision(0), scale(0), nullable(null) {}
    };

    /**
     * ENUM value definition
     */
    struct EnumValue
    {
        std::string label;
        int32_t position;  // Order/position in enum (for ordering)

        EnumValue() : position(0) {}
        EnumValue(const std::string& l, int32_t p) : label(l), position(p) {}
    };

    /**
     * Domain security options (Phase 6)
     */
    struct DomainSecurity
    {
        MaskingConfig masking_config;       // Data masking
        std::string required_privilege_for_unmasked; // Privilege to bypass masking
        bool encryption_enabled;    // Encryption at rest
        EncryptionAlgorithm encryption_algorithm; // Encryption algorithm
        ID encryption_key_id;       // Active encryption key ID
        bool audit_enabled;         // Audit trail
        uint32_t permission_mask;   // Permission requirements

        DomainSecurity()
            : encryption_enabled(false),
              encryption_algorithm(EncryptionAlgorithm::NONE),
              encryption_key_id(),
              audit_enabled(false), permission_mask(0) {}
    };

    /**
     * Domain integrity options (Phase 6)
     */
    struct DomainIntegrity
    {
        bool uniqueness_check;      // Enforce uniqueness
        bool normalization_enabled; // Auto-normalization
        std::string normalization_function;

        DomainIntegrity() : uniqueness_check(false), normalization_enabled(false) {}
    };

    /**
     * Domain validation options (Phase 6)
     */
    struct DomainValidation
    {
        std::string validation_function;  // Custom validation function name
        std::string error_message;        // Custom error message

        DomainValidation() = default;
    };

    /**
     * Domain quality options (Phase 6)
     */
    struct DomainQuality
    {
        std::string parse_function;       // Parsing function
        std::string standardize_function; // Standardization function
        std::string enrich_function;      // Enrichment function

        DomainQuality() = default;
    };

    /**
     * Domain information
     */
    struct DomainInfo
    {
        ID domain_id;
        ID schema_id;
        std::string domain_name;
        DomainType domain_type;
        DataType base_type;           // For BASIC domains
        uint32_t precision;           // For DECIMAL, VARCHAR, etc.
        uint32_t scale;               // For DECIMAL
        bool nullable;
        std::string default_value;

        // Inheritance (INHERITS clause)
        ID parent_domain_id;          // If inherits from another domain

        // For RECORD domains
        std::vector<RecordField> fields;

        // For ENUM domains
        std::vector<EnumValue> enum_values;

        // For SET domains
        DataType set_element_type;    // Element type for SET

        // For VARIANT domains
        std::vector<DataType> variant_allowed_types;  // Allowed types for VARIANT

        // Constraints
        std::vector<DomainConstraint> constraints;

        // Advanced features (Phase 6)
        DomainSecurity security;
        DomainIntegrity integrity;
        DomainValidation validation;
        DomainQuality quality;

        // Cross-dialect compatibility (Plan 04)
        std::string dialect_tag;      // e.g., "firebird", "postgresql", "mysql"
        std::string compat_name;      // Dialect-specific type name for mapping

        uint64_t created_time;
        uint64_t last_modified_time;

        DomainInfo()
            : domain_type(DomainType::BASIC), base_type(DataType::UNKNOWN),
              precision(0), scale(0), nullable(true), set_element_type(DataType::UNKNOWN),
              created_time(0), last_modified_time(0) {}
    };

    /**
     * DomainManager - Manages user-defined domains
     *
     * Provides:
     * - Domain creation and management
     * - Constraint validation
     * - Domain inheritance
     * - RECORD, ENUM, SET, VARIANT type support
     * - Security, integrity, validation, quality features
     */
    class DomainManager
    {
    public:
        DomainManager(Database* db);
        ~DomainManager();

        // Initialize domain catalog
        auto initialize(ErrorContext* ctx = nullptr) -> Status;

        // Load domains from catalog
        auto load(ErrorContext* ctx = nullptr) -> Status;

        // ====================
        // Phase 1: Basic Domains
        // ====================

        // Create basic domain
        auto createBasicDomain(const ID& schema_id,
                              const std::string& domain_name,
                              DataType base_type,
                              uint32_t precision,
                              uint32_t scale,
                              bool nullable,
                              const std::string& default_value,
                              const std::vector<DomainConstraint>& constraints,
                              ID& domain_id,
                              ErrorContext* ctx = nullptr) -> Status;

        // Get domain by ID
        auto getDomain(const ID& domain_id,
                      DomainInfo& info,
                      ErrorContext* ctx = nullptr) -> Status;

        // Get domain by name
        auto getDomain(const ID& schema_id,
                      const std::string& domain_name,
                      DomainInfo& info,
                      ErrorContext* ctx = nullptr) -> Status;

        // List domains in schema
        auto listDomains(const ID& schema_id,
                        std::vector<DomainInfo>& domains,
                        ErrorContext* ctx = nullptr) -> Status;

        // Drop domain
        auto dropDomain(const ID& domain_id,
                        ErrorContext* ctx = nullptr) -> Status;
        auto renameDomain(const ID& domain_id, const std::string& new_name,
                          ErrorContext* ctx = nullptr) -> Status;

        // Validate value against domain constraints
        auto validateValue(const ID& domain_id,
                          const TypedValue& value,
                          ErrorContext* ctx = nullptr) -> Status;

        // Domain inheritance
        auto setParentDomain(const ID& domain_id,
                           const ID& parent_domain_id,
                           ErrorContext* ctx = nullptr) -> Status;

        // ====================
        // Phase 2: RECORD Domains
        // ====================

        // Create RECORD domain
        auto createRecordDomain(const ID& schema_id,
                               const std::string& domain_name,
                               const std::vector<RecordField>& fields,
                               ID& domain_id,
                               ErrorContext* ctx = nullptr) -> Status;

        // Get field from RECORD domain
        auto getRecordField(const ID& domain_id,
                           const std::string& field_name,
                           RecordField& field,
                           ErrorContext* ctx = nullptr) -> Status;

        // Extract field value from RECORD
        auto extractField(const TypedValue& record_value,
                         const std::string& field_name,
                         TypedValue& field_value,
                         ErrorContext* ctx = nullptr) -> Status;

        // ====================
        // Phase 3: ENUM Domains
        // ====================

        // Create ENUM domain
        auto createEnumDomain(const ID& schema_id,
                             const std::string& domain_name,
                             const std::vector<EnumValue>& values,
                             ID& domain_id,
                             ErrorContext* ctx = nullptr) -> Status;

        // Set next value in ENUM (SET NEXT VALUE)
        auto setNextEnumValue(const ID& domain_id,
                             const std::string& current_label,
                             std::string& next_label,
                             ErrorContext* ctx = nullptr) -> Status;

        // Get value for position (GET VALUE FOR)
        auto getEnumValueForPosition(const ID& domain_id,
                                    int32_t position,
                                    std::string& label,
                                    ErrorContext* ctx = nullptr) -> Status;

        // Get position for value (GET POSITION FOR)
        auto getPositionForEnumValue(const ID& domain_id,
                                    const std::string& label,
                                    int32_t& position,
                                    ErrorContext* ctx = nullptr) -> Status;

        // Compare ENUM values
        auto compareEnumValues(const ID& domain_id,
                              const std::string& label1,
                              const std::string& label2,
                              int& result,
                              ErrorContext* ctx = nullptr) -> Status;

        // ====================
        // Phase 4: SET Domains
        // ====================

        // Create SET domain
        auto createSetDomain(const ID& schema_id,
                            const std::string& domain_name,
                            DataType element_type,
                            ID& domain_id,
                            ErrorContext* ctx = nullptr) -> Status;

        // Check if set contains element (@> operator)
        auto setContains(const TypedValue& set_value,
                        const TypedValue& element,
                        bool& result,
                        ErrorContext* ctx = nullptr) -> Status;

        // Check if sets overlap (&& operator)
        auto setsOverlap(const TypedValue& set1,
                        const TypedValue& set2,
                        bool& result,
                        ErrorContext* ctx = nullptr) -> Status;

        // Set union
        auto setUnion(const TypedValue& set1,
                     const TypedValue& set2,
                     TypedValue& result,
                     ErrorContext* ctx = nullptr) -> Status;

        // Set intersection
        auto setIntersection(const TypedValue& set1,
                            const TypedValue& set2,
                            TypedValue& result,
                            ErrorContext* ctx = nullptr) -> Status;

        // Set difference
        auto setDifference(const TypedValue& set1,
                          const TypedValue& set2,
                          TypedValue& result,
                          ErrorContext* ctx = nullptr) -> Status;

        // ====================
        // Phase 5: VARIANT Type
        // ====================

        // Create VARIANT domain
        auto createVariantDomain(const ID& schema_id,
                                const std::string& domain_name,
                                const std::vector<DataType>& allowed_types,
                                ID& domain_id,
                                ErrorContext* ctx = nullptr) -> Status;

        // Extract datatype from variant (EXTRACT(DATATYPE FROM value))
        auto extractDataType(const TypedValue& variant_value,
                            DataType& type,
                            ErrorContext* ctx = nullptr) -> Status;

        // Check if value is of type (IS OF TYPE check)
        auto isOfType(const TypedValue& variant_value,
                     DataType expected_type,
                     bool& result,
                     ErrorContext* ctx = nullptr) -> Status;

        // Type-safe cast for VARIANT
        auto variantCast(const TypedValue& variant_value,
                        DataType target_type,
                        TypedValue& result,
                        ErrorContext* ctx = nullptr) -> Status;

        // ====================
        // Phase 6: Advanced Features
        // ====================

        // Set security options
        auto setSecurityOptions(const ID& domain_id,
                               const DomainSecurity& security,
                               ErrorContext* ctx = nullptr) -> Status;

        // Set integrity options
        auto setIntegrityOptions(const ID& domain_id,
                                const DomainIntegrity& integrity,
                                ErrorContext* ctx = nullptr) -> Status;

        // Set validation options
        auto setValidationOptions(const ID& domain_id,
                                 const DomainValidation& validation,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Set quality options
        auto setQualityOptions(const ID& domain_id,
                              const DomainQuality& quality,
                              ErrorContext* ctx = nullptr) -> Status;

        // Apply data masking
        auto applyMasking(const ID& domain_id,
                         const ID& user_id,
                         const TypedValue& value,
                         TypedValue& masked_value,
                         ErrorContext* ctx = nullptr) -> Status;

        auto checkMaskingPrivilege(const ID& domain_id,
                                   const ID& user_id,
                                   bool& has_privilege_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        // Encryption support
        auto encryptValue(const ID& domain_id,
                          TypedValue& value,
                          ErrorContext* ctx = nullptr) -> Status;
        auto decryptValue(const ID& domain_id,
                          TypedValue& value,
                          ErrorContext* ctx = nullptr) -> Status;

        // Statistics
        auto domainCount() const -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return domain_count_;
        }

    private:
        Database* db_;
        mutable std::mutex mutex_;

        // Catalog page
        static constexpr uint32_t DOMAINS_TABLE_PAGE = 10;  // After TABLESPACE_FILES_TABLE_PAGE (page 9)
        uint32_t domains_table_page_ = DOMAINS_TABLE_PAGE;

        // In-memory cache
        std::unordered_map<ID, DomainInfo> domain_cache_;
        uint32_t domain_count_ = 0;

        // Internal helpers
        auto writeDomainRecord(const DomainInfo& domain, ErrorContext* ctx) -> Status;
        auto readDomainRecords(ErrorContext* ctx) -> Status;
        auto deleteDomainRecord(const ID& domain_id, ErrorContext* ctx) -> Status;

        // Constraint validation helpers
        auto validateCheckConstraint(const DomainInfo& domain,
                                    const TypedValue& value,
                                    const DomainConstraint& constraint,
                                    ErrorContext* ctx) -> Status;

        auto validateNotNullConstraint(const TypedValue& value,
                                       ErrorContext* ctx) -> Status;

        // Inheritance helpers
        auto resolveInheritedConstraints(const ID& domain_id,
                                        std::vector<DomainConstraint>& all_constraints,
                                        ErrorContext* ctx) -> Status;
    };

} // namespace scratchbird::core
