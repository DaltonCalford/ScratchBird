#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/composite.h"
#include <cstring>
#include <algorithm>
#include <unordered_set>

namespace scratchbird::core
{
    // On-disk domain record structure
    struct DomainRecord
    {
        ID domain_id;
        ID schema_id;
        char domain_name[128];
        uint8_t domain_type;         // DomainType enum
        uint16_t base_type;          // DataType enum
        uint32_t precision;
        uint32_t scale;
        uint8_t nullable;
        char default_value[256];
        ID parent_domain_id;         // For inheritance
        uint8_t is_valid;           // Soft delete flag
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t constraints_oid;   // TOAST reference for constraints
        uint32_t fields_oid;        // TOAST reference for RECORD fields
        uint32_t enum_values_oid;   // TOAST reference for ENUM values
        uint16_t set_element_type;  // For SET domains
        uint16_t reserved;

        DomainRecord() : domain_type(0), base_type(0), precision(0), scale(0),
                        nullable(1), is_valid(1), created_time(0), last_modified_time(0),
                        constraints_oid(0), fields_oid(0), enum_values_oid(0),
                        set_element_type(0), reserved(0)
        {
            std::memset(domain_name, 0, sizeof(domain_name));
            std::memset(default_value, 0, sizeof(default_value));
        }
    };

    // Helper struct for catalog page
    struct DomainCatalogPage
    {
        PageHeader header;
        uint32_t record_count;
        uint32_t free_offset;
        uint8_t data[];
    };

    DomainManager::DomainManager(Database* db)
        : db_(db), domain_count_(0)
    {
    }

    DomainManager::~DomainManager() = default;

    auto DomainManager::initialize(ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        LOG_INFO(CATALOG, "Initializing domain manager");

        // Allocate domains catalog page
        PageManager* pm = db_->page_manager();
        if (!pm)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "PageManager not available");
            return Status::IO_ERROR;
        }

        Status status = pm->allocatePage(domains_table_page_, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate domains table page");
            return status;
        }

        // Initialize the domains catalog page
        BufferPool* bp = db_->buffer_pool();
        void* page_buffer;
        status = bp->pinPage(domains_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
            return status;
        }

        auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);
        catalog_page->header.page_id = domains_table_page_;
        catalog_page->header.page_type = PAGE_TYPE_HEAP;
        catalog_page->record_count = 0;
        catalog_page->free_offset = sizeof(DomainCatalogPage);

        status = bp->unpinPage(domains_table_page_, true, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to unpin domains catalog page");
            return status;
        }

        LOG_INFO(CATALOG, "Domain manager initialized successfully");
        return Status::OK;
    }

    auto DomainManager::load(ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        LOG_INFO(CATALOG, "Loading domains from catalog");

        Status status = readDomainRecords(ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to load domains from catalog");
            return status;
        }

        LOG_INFO(CATALOG, "Loaded %u domains", domain_count_);
        return Status::OK;
    }

    // ====================
    // Phase 1: Basic Domains
    // ====================

    auto DomainManager::createBasicDomain(const ID& schema_id,
                                         const std::string& domain_name,
                                         DataType base_type,
                                         uint32_t precision,
                                         uint32_t scale,
                                         bool nullable,
                                         const std::string& default_value,
                                         const std::vector<DomainConstraint>& constraints,
                                         ID& domain_id,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Generate new domain ID
        domain_id = generateUuidV7();

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::BASIC;
        info.base_type = base_type;
        info.precision = precision;
        info.scale = scale;
        info.nullable = nullable;
        info.default_value = default_value;
        info.constraints = constraints;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;

        // Write to catalog
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write domain record");
            return status;
        }

        // Add to cache
        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created basic domain '%s' with ID %s",
                domain_name.c_str(), domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::getDomain(const ID& domain_id,
                                 DomainInfo& info,
                                 ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it != domain_cache_.end())
        {
            info = it->second;
            return Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
        return Status::NOT_FOUND;
    }

    auto DomainManager::getDomain(const ID& schema_id,
                                 const std::string& domain_name,
                                 DomainInfo& info,
                                 ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [id, domain_info] : domain_cache_)
        {
            if (domain_info.schema_id == schema_id && domain_info.domain_name == domain_name)
            {
                info = domain_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
        return Status::NOT_FOUND;
    }

    auto DomainManager::listDomains(const ID& schema_id,
                                   std::vector<DomainInfo>& domains,
                                   ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        domains.clear();
        for (const auto& [id, info] : domain_cache_)
        {
            if (info.schema_id == schema_id)
            {
                domains.push_back(info);
            }
        }

        return Status::OK;
    }

    auto DomainManager::dropDomain(const ID& domain_id,
                                  ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        // Delete from catalog
        Status status = deleteDomainRecord(domain_id, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to delete domain record");
            return status;
        }

        // Remove from cache
        domain_cache_.erase(it);
        domain_count_--;

        LOG_INFO(CATALOG, "Dropped domain with ID %s",
                domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::validateValue(const ID& domain_id,
                                      const TypedValue& value,
                                      ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        const DomainInfo& domain = it->second;

        // Check NULL constraint
        if (!domain.nullable && value.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, "NULL value not allowed");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check type compatibility
        if (!value.isNull() && value.type() != domain.base_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value type does not match domain type");
            return Status::TYPE_MISMATCH;
        }

        // Validate constraints
        for (const auto& constraint : domain.constraints)
        {
            Status status = Status::OK;

            switch (constraint.type)
            {
                case ConstraintType::NOT_NULL:
                    status = validateNotNullConstraint(value, ctx);
                    break;

                case ConstraintType::CHECK:
                    status = validateCheckConstraint(domain, value, constraint, ctx);
                    break;

                case ConstraintType::DEFAULT:
                case ConstraintType::UNIQUE:
                    // These are handled elsewhere
                    break;
            }

            if (status != Status::OK)
            {
                return status;
            }
        }

        // Check inherited constraints
        if (domain.parent_domain_id != ID{})
        {
            std::vector<DomainConstraint> inherited;
            Status status = resolveInheritedConstraints(domain_id, inherited, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            for (const auto& constraint : inherited)
            {
                if (constraint.type == ConstraintType::CHECK)
                {
                    status = validateCheckConstraint(domain, value, constraint, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }
            }
        }

        return Status::OK;
    }

    auto DomainManager::setParentDomain(const ID& domain_id,
                                       const ID& parent_domain_id,
                                       ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto parent_it = domain_cache_.find(parent_domain_id);
        if (parent_it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Parent domain not found");
            return Status::NOT_FOUND;
        }

        // Update domain info
        it->second.parent_domain_id = parent_domain_id;
        it->second.last_modified_time = std::time(nullptr);

        // Update catalog
        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Set parent domain for %s to %s",
                domain_id.toString().c_str(),
                parent_domain_id.toString().c_str());

        return Status::OK;
    }

    // ====================
    // Phase 2: RECORD Domains (Stubs)
    // ====================

    auto DomainManager::createRecordDomain(const ID& schema_id,
                                          const std::string& domain_name,
                                          const std::vector<RecordField>& fields,
                                          ID& domain_id,
                                          ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate fields
        if (fields.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "RECORD domain must have at least one field");
            return Status::INVALID_ARGUMENT;
        }

        // Check for duplicate field names
        std::unordered_set<std::string> field_names;
        for (const auto& field : fields)
        {
            if (field_names.count(field.name) > 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Duplicate field name in RECORD domain");
                return Status::INVALID_ARGUMENT;
            }
            field_names.insert(field.name);
        }

        // Generate new domain ID
        domain_id = generateUuidV7();

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::RECORD;
        info.base_type = DataType::COMPOSITE;
        info.fields = fields;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;

        // Write to catalog
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write RECORD domain record");
            return status;
        }

        // Add to cache
        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created RECORD domain '%s' with %zu fields",
                domain_name.c_str(), fields.size());

        return Status::OK;
    }

    auto DomainManager::getRecordField(const ID& domain_id,
                                      const std::string& field_name,
                                      RecordField& field,
                                      ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        const DomainInfo& domain = it->second;
        if (domain.domain_type != DomainType::RECORD)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not a RECORD type");
            return Status::INVALID_ARGUMENT;
        }

        // Find field by name
        for (const auto& f : domain.fields)
        {
            if (f.name == field_name)
            {
                field = f;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Field not found in RECORD domain");
        return Status::NOT_FOUND;
    }

    auto DomainManager::extractField(const TypedValue& record_value,
                                    const std::string& field_name,
                                    TypedValue& field_value,
                                    ErrorContext* ctx) -> Status
    {
        // Check if value is COMPOSITE type
        if (record_value.type() != DataType::COMPOSITE)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not a COMPOSITE/RECORD type");
            return Status::TYPE_MISMATCH;
        }

        // TODO: This requires TypedValue to support COMPOSITE values directly
        // For now, this is a placeholder that demonstrates the API
        // Full implementation requires:
        // 1. TypedValue extension to hold CompositeValue
        // 2. Binary decoding of COMPOSITE from TypedValue storage
        // 3. Field extraction from decoded CompositeValue

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "RECORD field extraction requires TypedValue COMPOSITE support (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    // ====================
    // Phase 3: ENUM Domains
    // ====================

    auto DomainManager::createEnumDomain(const ID& schema_id,
                                        const std::string& domain_name,
                                        const std::vector<EnumValue>& values,
                                        ID& domain_id,
                                        ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate enum values
        if (values.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ENUM domain must have at least one value");
            return Status::INVALID_ARGUMENT;
        }

        // Check for duplicate values
        std::unordered_set<std::string> value_set;
        for (const auto& enum_val : values)
        {
            if (value_set.count(enum_val.label) > 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Duplicate value in ENUM domain");
                return Status::INVALID_ARGUMENT;
            }
            value_set.insert(enum_val.label);
        }

        // Validate positions are sequential starting from 0
        for (size_t i = 0; i < values.size(); i++)
        {
            if (values[i].position != static_cast<uint32_t>(i))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                 "ENUM positions must be sequential starting from 0");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Generate new domain ID
        domain_id = generateUuidV7();

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::ENUM;
        info.base_type = DataType::VARCHAR;  // ENUMs stored as VARCHAR
        info.enum_values = values;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;

        // Write to catalog
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write ENUM domain record");
            return status;
        }

        // Add to cache
        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created ENUM domain '%s' with %zu values",
                domain_name.c_str(), values.size());

        return Status::OK;
    }

    auto DomainManager::setNextEnumValue(const ID& domain_id,
                                        const std::string& current_label,
                                        std::string& next_label,
                                        ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        DomainInfo& domain = it->second;
        if (domain.domain_type != DomainType::ENUM)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not an ENUM type");
            return Status::INVALID_ARGUMENT;
        }

        // Find current value position
        int32_t current_position = -1;
        for (const auto& enum_val : domain.enum_values)
        {
            if (enum_val.label == current_label)
            {
                current_position = static_cast<int32_t>(enum_val.position);
                break;
            }
        }

        if (current_position == -1)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Current value not found in ENUM");
            return Status::NOT_FOUND;
        }

        // Check if there's a next value
        int32_t next_position = current_position + 1;
        if (next_position >= static_cast<int32_t>(domain.enum_values.size()))
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "No next value (already at last position)");
            return Status::OUT_OF_RANGE;
        }

        // Return next value
        next_label = domain.enum_values[next_position].label;
        return Status::OK;
    }

    auto DomainManager::getEnumValueForPosition(const ID& domain_id,
                                               int32_t position,
                                               std::string& label,
                                               ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        const DomainInfo& domain = it->second;
        if (domain.domain_type != DomainType::ENUM)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not an ENUM type");
            return Status::INVALID_ARGUMENT;
        }

        // Validate position
        if (position < 0 || position >= static_cast<int32_t>(domain.enum_values.size()))
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Position out of range");
            return Status::OUT_OF_RANGE;
        }

        // Return value at position
        label = domain.enum_values[position].label;
        return Status::OK;
    }

    auto DomainManager::getPositionForEnumValue(const ID& domain_id,
                                               const std::string& label,
                                               int32_t& position,
                                               ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        const DomainInfo& domain = it->second;
        if (domain.domain_type != DomainType::ENUM)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not an ENUM type");
            return Status::INVALID_ARGUMENT;
        }

        // Find value and return position
        for (const auto& enum_val : domain.enum_values)
        {
            if (enum_val.label == label)
            {
                position = static_cast<int32_t>(enum_val.position);
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Value not found in ENUM");
        return Status::NOT_FOUND;
    }

    auto DomainManager::compareEnumValues(const ID& domain_id,
                                         const std::string& label1,
                                         const std::string& label2,
                                         int& result,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        const DomainInfo& domain = it->second;
        if (domain.domain_type != DomainType::ENUM)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not an ENUM type");
            return Status::INVALID_ARGUMENT;
        }

        // Find positions for both values
        int32_t pos1 = -1;
        int32_t pos2 = -1;

        for (const auto& enum_val : domain.enum_values)
        {
            if (enum_val.label == label1)
            {
                pos1 = static_cast<int32_t>(enum_val.position);
            }
            if (enum_val.label == label2)
            {
                pos2 = static_cast<int32_t>(enum_val.position);
            }
        }

        if (pos1 == -1)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "First value not found in ENUM");
            return Status::NOT_FOUND;
        }

        if (pos2 == -1)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Second value not found in ENUM");
            return Status::NOT_FOUND;
        }

        // Compare by position
        if (pos1 < pos2)
        {
            result = -1;
        }
        else if (pos1 > pos2)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }

        return Status::OK;
    }

    // ====================
    // Phase 4: SET Domains
    // ====================

    auto DomainManager::createSetDomain(const ID& schema_id,
                                       const std::string& domain_name,
                                       DataType element_type,
                                       ID& domain_id,
                                       ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate element type
        if (element_type == DataType::UNKNOWN)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "SET element type cannot be UNKNOWN");
            return Status::INVALID_ARGUMENT;
        }

        // Generate new domain ID
        domain_id = generateUuidV7();

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::SET;
        info.base_type = DataType::VECTOR;  // SETs stored as VECTOR with unique elements
        info.set_element_type = element_type;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;

        // Write to catalog
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write SET domain record");
            return status;
        }

        // Add to cache
        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created SET domain '%s' with element type %d",
                domain_name.c_str(), static_cast<int>(element_type));

        return Status::OK;
    }

    auto DomainManager::setContains(const TypedValue& set_value,
                                   const TypedValue& element,
                                   bool& result,
                                   ErrorContext* ctx) -> Status
    {
        // Check if set_value is VECTOR type
        if (set_value.type() != DataType::VECTOR)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not a VECTOR/SET type");
            return Status::TYPE_MISMATCH;
        }

        // TODO: This requires TypedValue VECTOR support for element access
        // For now, this is a placeholder that demonstrates the API
        // Full implementation requires:
        // 1. TypedValue extension to access VectorValue elements
        // 2. Element iteration through vector
        // 3. Element comparison for membership testing

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET contains operation requires TypedValue VECTOR element access (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setsOverlap(const TypedValue& set1,
                                   const TypedValue& set2,
                                   bool& result,
                                   ErrorContext* ctx) -> Status
    {
        // Check if both values are VECTOR type
        if (set1.type() != DataType::VECTOR || set2.type() != DataType::VECTOR)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Values are not VECTOR/SET types");
            return Status::TYPE_MISMATCH;
        }

        // TODO: This requires TypedValue VECTOR support for element access
        // Full implementation requires:
        // 1. TypedValue extension to access VectorValue elements
        // 2. Iterate through both sets looking for common elements
        // 3. Return true if any element appears in both sets

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET overlap operation requires TypedValue VECTOR element access (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setUnion(const TypedValue& set1,
                                const TypedValue& set2,
                                TypedValue& result,
                                ErrorContext* ctx) -> Status
    {
        // Check if both values are VECTOR type
        if (set1.type() != DataType::VECTOR || set2.type() != DataType::VECTOR)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Values are not VECTOR/SET types");
            return Status::TYPE_MISMATCH;
        }

        // TODO: This requires TypedValue VECTOR support for construction and element access
        // Full implementation requires:
        // 1. TypedValue extension to access and create VectorValue
        // 2. Collect all unique elements from both sets
        // 3. Create new VECTOR with union of elements

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET union operation requires TypedValue VECTOR construction (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setIntersection(const TypedValue& set1,
                                       const TypedValue& set2,
                                       TypedValue& result,
                                       ErrorContext* ctx) -> Status
    {
        // Check if both values are VECTOR type
        if (set1.type() != DataType::VECTOR || set2.type() != DataType::VECTOR)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Values are not VECTOR/SET types");
            return Status::TYPE_MISMATCH;
        }

        // TODO: This requires TypedValue VECTOR support for construction and element access
        // Full implementation requires:
        // 1. TypedValue extension to access and create VectorValue
        // 2. Collect elements that appear in both sets
        // 3. Create new VECTOR with intersection of elements

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET intersection operation requires TypedValue VECTOR construction (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setDifference(const TypedValue& set1,
                                     const TypedValue& set2,
                                     TypedValue& result,
                                     ErrorContext* ctx) -> Status
    {
        // Check if both values are VECTOR type
        if (set1.type() != DataType::VECTOR || set2.type() != DataType::VECTOR)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Values are not VECTOR/SET types");
            return Status::TYPE_MISMATCH;
        }

        // TODO: This requires TypedValue VECTOR support for construction and element access
        // Full implementation requires:
        // 1. TypedValue extension to access and create VectorValue
        // 2. Collect elements from set1 that don't appear in set2
        // 3. Create new VECTOR with difference of elements

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET difference operation requires TypedValue VECTOR construction (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    // ====================
    // Phase 5: VARIANT Type
    // ====================

    auto DomainManager::createVariantDomain(const ID& schema_id,
                                           const std::string& domain_name,
                                           const std::vector<DataType>& allowed_types,
                                           ID& domain_id,
                                           ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate allowed types
        if (allowed_types.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "VARIANT domain must allow at least one type");
            return Status::INVALID_ARGUMENT;
        }

        // Check for UNKNOWN type
        for (const auto& type : allowed_types)
        {
            if (type == DataType::UNKNOWN)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "VARIANT cannot allow UNKNOWN type");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Check for duplicate types
        std::unordered_set<DataType> type_set;
        for (const auto& type : allowed_types)
        {
            if (type_set.count(type) > 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Duplicate type in VARIANT allowed types");
                return Status::INVALID_ARGUMENT;
            }
            type_set.insert(type);
        }

        // Generate new domain ID
        domain_id = generateUuidV7();

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::VARIANT;
        info.base_type = DataType::UNKNOWN;  // VARIANT doesn't have fixed base type
        info.variant_allowed_types = allowed_types;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;

        // Write to catalog
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write VARIANT domain record");
            return status;
        }

        // Add to cache
        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created VARIANT domain '%s' with %zu allowed types",
                domain_name.c_str(), allowed_types.size());

        return Status::OK;
    }

    auto DomainManager::extractDataType(const TypedValue& variant_value,
                                       DataType& type,
                                       ErrorContext* ctx) -> Status
    {
        // TODO: This requires TypedValue VARIANT support for runtime type access
        // For now, this is a placeholder that demonstrates the API
        // Full implementation requires:
        // 1. TypedValue extension to hold VariantValue with runtime type tag
        // 2. Runtime type extraction from VariantValue

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "VARIANT type extraction requires TypedValue VARIANT support (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::isOfType(const TypedValue& variant_value,
                                DataType expected_type,
                                bool& result,
                                ErrorContext* ctx) -> Status
    {
        // TODO: This requires TypedValue VARIANT support for runtime type checking
        // Full implementation requires:
        // 1. TypedValue extension to hold VariantValue with runtime type tag
        // 2. Runtime type comparison

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "VARIANT type checking requires TypedValue VARIANT support (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::variantCast(const TypedValue& variant_value,
                                   DataType target_type,
                                   TypedValue& result,
                                   ErrorContext* ctx) -> Status
    {
        // TODO: This requires TypedValue VARIANT support for type-safe casting
        // Full implementation requires:
        // 1. TypedValue extension to hold VariantValue
        // 2. Runtime type extraction
        // 3. Type-safe value extraction and casting
        // 4. Validation that cast is to allowed type

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "VARIANT casting requires TypedValue VARIANT support (future enhancement)");
        return Status::NOT_IMPLEMENTED;
    }

    // ====================
    // Phase 6: Advanced Features (Stubs)
    // ====================

    auto DomainManager::setSecurityOptions(const ID& domain_id,
                                          const DomainSecurity& security,
                                          ErrorContext* ctx) -> Status
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Security features not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setIntegrityOptions(const ID& domain_id,
                                           const DomainIntegrity& integrity,
                                           ErrorContext* ctx) -> Status
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Integrity features not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setValidationOptions(const ID& domain_id,
                                            const DomainValidation& validation,
                                            ErrorContext* ctx) -> Status
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Validation features not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::setQualityOptions(const ID& domain_id,
                                         const DomainQuality& quality,
                                         ErrorContext* ctx) -> Status
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Quality features not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::applyMasking(const ID& domain_id,
                                    const TypedValue& value,
                                    TypedValue& masked_value,
                                    ErrorContext* ctx) -> Status
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Masking not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    // ====================
    // Private Helper Methods
    // ====================

    auto DomainManager::writeDomainRecord(const DomainInfo& domain, ErrorContext* ctx) -> Status
    {
        BufferPool* bp = db_->buffer_pool();
        void* page_buffer;

        Status status = bp->pinPage(domains_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
            return status;
        }

        auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);

        // Create domain record
        DomainRecord record;
        record.domain_id = domain.domain_id;
        record.schema_id = domain.schema_id;
        std::strncpy(record.domain_name, domain.domain_name.c_str(), sizeof(record.domain_name) - 1);
        record.domain_type = static_cast<uint8_t>(domain.domain_type);
        record.base_type = static_cast<uint16_t>(domain.base_type);
        record.precision = domain.precision;
        record.scale = domain.scale;
        record.nullable = domain.nullable ? 1 : 0;
        std::strncpy(record.default_value, domain.default_value.c_str(), sizeof(record.default_value) - 1);
        record.parent_domain_id = domain.parent_domain_id;
        record.is_valid = 1;
        record.created_time = domain.created_time;
        record.last_modified_time = domain.last_modified_time;
        record.set_element_type = static_cast<uint16_t>(domain.set_element_type);

        // TODO: Serialize constraints, fields, enum_values to TOAST

        // Write record
        uint8_t* write_pos = catalog_page->data + (catalog_page->record_count * sizeof(DomainRecord));
        std::memcpy(write_pos, &record, sizeof(DomainRecord));
        catalog_page->record_count++;
        catalog_page->free_offset += sizeof(DomainRecord);

        return bp->unpinPage(domains_table_page_, true, ctx);
    }

    auto DomainManager::readDomainRecords(ErrorContext* ctx) -> Status
    {
        BufferPool* bp = db_->buffer_pool();
        void* page_buffer;

        Status status = bp->pinPage(domains_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
            return status;
        }

        auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);
        domain_cache_.clear();
        domain_count_ = 0;

        for (uint32_t i = 0; i < catalog_page->record_count; i++)
        {
            auto* record = reinterpret_cast<DomainRecord*>(
                catalog_page->data + (i * sizeof(DomainRecord)));

            if (record->is_valid)
            {
                DomainInfo info;
                info.domain_id = record->domain_id;
                info.schema_id = record->schema_id;
                info.domain_name = record->domain_name;
                info.domain_type = static_cast<DomainType>(record->domain_type);
                info.base_type = static_cast<DataType>(record->base_type);
                info.precision = record->precision;
                info.scale = record->scale;
                info.nullable = record->nullable != 0;
                info.default_value = record->default_value;
                info.parent_domain_id = record->parent_domain_id;
                info.created_time = record->created_time;
                info.last_modified_time = record->last_modified_time;
                info.set_element_type = static_cast<DataType>(record->set_element_type);

                // TODO: Load constraints, fields, enum_values from TOAST

                domain_cache_[info.domain_id] = info;
                domain_count_++;
            }
        }

        return bp->unpinPage(domains_table_page_, false, ctx);
    }

    auto DomainManager::deleteDomainRecord(const ID& domain_id, ErrorContext* ctx) -> Status
    {
        BufferPool* bp = db_->buffer_pool();
        void* page_buffer;

        Status status = bp->pinPage(domains_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
            return status;
        }

        auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);
        bool found = false;

        for (uint32_t i = 0; i < catalog_page->record_count; i++)
        {
            auto* record = reinterpret_cast<DomainRecord*>(
                catalog_page->data + (i * sizeof(DomainRecord)));

            if (record->domain_id == domain_id && record->is_valid)
            {
                record->is_valid = 0;  // Soft delete
                found = true;
                break;
            }
        }

        if (!found)
        {
            bp->unpinPage(domains_table_page_, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain record not found");
            return Status::NOT_FOUND;
        }

        return bp->unpinPage(domains_table_page_, true, ctx);
    }

    auto DomainManager::validateCheckConstraint(const DomainInfo& domain,
                                                const TypedValue& value,
                                                const DomainConstraint& constraint,
                                                ErrorContext* ctx) -> Status
    {
        // TODO: Implement CHECK constraint evaluation
        // This requires integration with the expression evaluator
        // For now, just log and return OK
        LOG_DEBUG(CATALOG, "CHECK constraint validation not yet implemented");
        return Status::OK;
    }

    auto DomainManager::validateNotNullConstraint(const TypedValue& value,
                                                  ErrorContext* ctx) -> Status
    {
        if (value.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, "NULL value not allowed");
            return Status::CONSTRAINT_VIOLATION;
        }
        return Status::OK;
    }

    auto DomainManager::resolveInheritedConstraints(const ID& domain_id,
                                                    std::vector<DomainConstraint>& all_constraints,
                                                    ErrorContext* ctx) -> Status
    {
        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            return Status::NOT_FOUND;
        }

        const DomainInfo& domain = it->second;

        // Add this domain's constraints
        all_constraints.insert(all_constraints.end(),
                             domain.constraints.begin(),
                             domain.constraints.end());

        // Recursively get parent constraints
        if (domain.parent_domain_id != ID{})
        {
            return resolveInheritedConstraints(domain.parent_domain_id, all_constraints, ctx);
        }

        return Status::OK;
    }

} // namespace scratchbird::core
