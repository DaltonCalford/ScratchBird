#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/composite.h"
#include "scratchbird/core/utf8_utils.h"
#include <cstring>
#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace scratchbird::core
{
    namespace {
        bool isZeroUuidLocal(const ID& id) {
            for (auto b : id.bytes) {
                if (b != 0) {
                    return false;
                }
            }
            return true;
        }

        std::string defaultDialectTagForCreate() {
            auto* conn_ctx = ConnectionContext::getCurrent();
            if (conn_ctx && !conn_ctx->dialect_tag().empty()) {
                return conn_ctx->dialect_tag();
            }
            return "scratchbird";
        }

        constexpr uint8_t DOMAIN_SECURITY_VERSION = 1;
        constexpr uint8_t DOMAIN_INTEGRITY_VERSION = 1;
        constexpr uint8_t DOMAIN_VALIDATION_VERSION = 1;
        constexpr uint8_t DOMAIN_QUALITY_VERSION = 1;

        void appendUint8(std::string& out, uint8_t value)
        {
            out.push_back(static_cast<char>(value));
        }

        bool readUint8(const std::string& blob, size_t& offset, uint8_t& value)
        {
            if (offset + 1 > blob.size())
            {
                return false;
            }
            value = static_cast<uint8_t>(blob[offset]);
            offset += 1;
            return true;
        }

        void appendUint32(std::string& out, uint32_t value)
        {
            char buf[4];
            buf[0] = static_cast<char>(value & 0xFF);
            buf[1] = static_cast<char>((value >> 8) & 0xFF);
            buf[2] = static_cast<char>((value >> 16) & 0xFF);
            buf[3] = static_cast<char>((value >> 24) & 0xFF);
            out.append(buf, 4);
        }

        bool readUint32(const std::string& blob, size_t& offset, uint32_t& value)
        {
            if (offset + 4 > blob.size())
            {
                return false;
            }
            const uint8_t* data = reinterpret_cast<const uint8_t*>(blob.data() + offset);
            value = static_cast<uint32_t>(data[0]) |
                    (static_cast<uint32_t>(data[1]) << 8) |
                    (static_cast<uint32_t>(data[2]) << 16) |
                    (static_cast<uint32_t>(data[3]) << 24);
            offset += 4;
            return true;
        }

        void appendString(std::string& out, const std::string& value)
        {
            if (value.size() > UINT32_MAX)
            {
                appendUint32(out, 0);
                return;
            }
            appendUint32(out, static_cast<uint32_t>(value.size()));
            out.append(value);
        }

        bool readString(const std::string& blob, size_t& offset, std::string& value)
        {
            uint32_t len = 0;
            if (!readUint32(blob, offset, len))
            {
                return false;
            }
            if (offset + len > blob.size())
            {
                return false;
            }
            value.assign(blob.data() + offset, len);
            offset += len;
            return true;
        }

        bool isDefaultSecurity(const DomainSecurity& security)
        {
            return security.masking_config.type == MaskingType::NONE &&
                   security.masking_config.pattern.empty() &&
                   security.masking_config.full_mask_char == "*" &&
                   security.required_privilege_for_unmasked.empty() &&
                   !security.encryption_enabled &&
                   !security.audit_enabled &&
                   security.permission_mask == 0;
        }

        bool isDefaultIntegrity(const DomainIntegrity& integrity)
        {
            return !integrity.uniqueness_check &&
                   !integrity.normalization_enabled &&
                   integrity.normalization_function.empty();
        }

        bool isDefaultValidation(const DomainValidation& validation)
        {
            return validation.validation_function.empty() &&
                   validation.error_message.empty();
        }

        bool isDefaultQuality(const DomainQuality& quality)
        {
            return quality.parse_function.empty() &&
                   quality.standardize_function.empty() &&
                   quality.enrich_function.empty();
        }

        std::string serializeDomainSecurity(const DomainSecurity& security)
        {
            if (isDefaultSecurity(security))
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_SECURITY_VERSION);
            appendUint8(out, static_cast<uint8_t>(security.masking_config.type));
            appendUint8(out, security.encryption_enabled ? 1 : 0);
            appendUint8(out, security.audit_enabled ? 1 : 0);
            appendUint32(out, security.permission_mask);
            appendString(out, security.masking_config.pattern);
            appendString(out, security.masking_config.full_mask_char);
            appendString(out, security.required_privilege_for_unmasked);
            return out;
        }

        Status deserializeDomainSecurity(const std::string& blob,
                                         DomainSecurity& security,
                                         ErrorContext* ctx)
        {
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            if (!readUint8(blob, offset, version) || version != DOMAIN_SECURITY_VERSION)
            {
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTION, "Invalid domain security format");
                return Status::CORRUPTION;
            }

            uint8_t type = 0;
            uint8_t encryption = 0;
            uint8_t audit = 0;
            uint32_t permission_mask = 0;
            std::string pattern;
            std::string mask_char;
            std::string privilege;

            if (!readUint8(blob, offset, type) ||
                !readUint8(blob, offset, encryption) ||
                !readUint8(blob, offset, audit) ||
                !readUint32(blob, offset, permission_mask) ||
                !readString(blob, offset, pattern) ||
                !readString(blob, offset, mask_char) ||
                !readString(blob, offset, privilege))
            {
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTION, "Invalid domain security payload");
                return Status::CORRUPTION;
            }

            security.masking_config.type = static_cast<MaskingType>(type);
            security.masking_config.pattern = std::move(pattern);
            security.masking_config.full_mask_char = std::move(mask_char);
            security.required_privilege_for_unmasked = std::move(privilege);
            security.encryption_enabled = (encryption != 0);
            security.audit_enabled = (audit != 0);
            security.permission_mask = permission_mask;

            return Status::OK;
        }

        std::string serializeDomainIntegrity(const DomainIntegrity& integrity)
        {
            if (isDefaultIntegrity(integrity))
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_INTEGRITY_VERSION);
            appendUint8(out, integrity.uniqueness_check ? 1 : 0);
            appendUint8(out, integrity.normalization_enabled ? 1 : 0);
            appendUint8(out, 0);
            appendString(out, integrity.normalization_function);
            return out;
        }

        Status deserializeDomainIntegrity(const std::string& blob,
                                          DomainIntegrity& integrity,
                                          ErrorContext* ctx)
        {
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            uint8_t uniqueness = 0;
            uint8_t normalization = 0;
            uint8_t reserved = 0;
            std::string function;

            if (!readUint8(blob, offset, version) || version != DOMAIN_INTEGRITY_VERSION ||
                !readUint8(blob, offset, uniqueness) ||
                !readUint8(blob, offset, normalization) ||
                !readUint8(blob, offset, reserved) ||
                !readString(blob, offset, function))
            {
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTION, "Invalid domain integrity payload");
                return Status::CORRUPTION;
            }

            integrity.uniqueness_check = (uniqueness != 0);
            integrity.normalization_enabled = (normalization != 0);
            integrity.normalization_function = std::move(function);

            return Status::OK;
        }

        std::string serializeDomainValidation(const DomainValidation& validation)
        {
            if (isDefaultValidation(validation))
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_VALIDATION_VERSION);
            appendString(out, validation.validation_function);
            appendString(out, validation.error_message);
            return out;
        }

        Status deserializeDomainValidation(const std::string& blob,
                                           DomainValidation& validation,
                                           ErrorContext* ctx)
        {
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            std::string function;
            std::string message;

            if (!readUint8(blob, offset, version) || version != DOMAIN_VALIDATION_VERSION ||
                !readString(blob, offset, function) ||
                !readString(blob, offset, message))
            {
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTION, "Invalid domain validation payload");
                return Status::CORRUPTION;
            }

            validation.validation_function = std::move(function);
            validation.error_message = std::move(message);

            return Status::OK;
        }

        std::string serializeDomainQuality(const DomainQuality& quality)
        {
            if (isDefaultQuality(quality))
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_QUALITY_VERSION);
            appendString(out, quality.parse_function);
            appendString(out, quality.standardize_function);
            appendString(out, quality.enrich_function);
            return out;
        }

        Status deserializeDomainQuality(const std::string& blob,
                                        DomainQuality& quality,
                                        ErrorContext* ctx)
        {
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            std::string parse_function;
            std::string standardize_function;
            std::string enrich_function;

            if (!readUint8(blob, offset, version) || version != DOMAIN_QUALITY_VERSION ||
                !readString(blob, offset, parse_function) ||
                !readString(blob, offset, standardize_function) ||
                !readString(blob, offset, enrich_function))
            {
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTION, "Invalid domain quality payload");
                return Status::CORRUPTION;
            }

            quality.parse_function = std::move(parse_function);
            quality.standardize_function = std::move(standardize_function);
            quality.enrich_function = std::move(enrich_function);

            return Status::OK;
        }
    }

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
        uint32_t security_oid;      // TOAST reference for security config
        uint32_t integrity_oid;     // TOAST reference for integrity config
        uint32_t validation_oid;    // TOAST reference for validation config
        uint32_t quality_oid;       // TOAST reference for quality config
        uint16_t set_element_type;  // For SET domains
        char dialect_tag[32];        // Cross-dialect compatibility tag
        char compat_name[128];       // Dialect-specific type name
        uint16_t reserved;

        DomainRecord() : domain_type(0), base_type(0), precision(0), scale(0),
                        nullable(1), is_valid(1), created_time(0), last_modified_time(0),
                        constraints_oid(0), fields_oid(0), enum_values_oid(0),
                        security_oid(0), integrity_oid(0), validation_oid(0), quality_oid(0),
                        set_element_type(0), reserved(0)
        {
            std::memset(domain_name, 0, sizeof(domain_name));
            std::memset(default_value, 0, sizeof(default_value));
            std::memset(dialect_tag, 0, sizeof(dialect_tag));
            std::memset(compat_name, 0, sizeof(compat_name));
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
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

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
            if (!isZeroUuidLocal(schema_id) && domain_info.schema_id != schema_id)
            {
                continue;
            }
            if (IdentifierUtils::namesMatch(domain_name, false /*search_delimited*/,
                                            domain_info.domain_name, false /*stored_delimited*/))
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
            if (!isZeroUuidLocal(schema_id) && info.schema_id != schema_id)
            {
                continue;
            }
            domains.push_back(info);
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

        // Check for dependencies - domain cannot be dropped if used by columns
        // We need to unlock mutex before calling catalog_manager to avoid deadlock
        mutex_.unlock();
        std::vector<std::pair<ID, std::string>> dependent_columns;
        Status status = db_->catalog_manager()->findColumnsByDomain(domain_id, dependent_columns, ctx);
        mutex_.lock();

        if (status == Status::OK && !dependent_columns.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Cannot drop domain: referenced by column(s)");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Delete from catalog
        status = deleteDomainRecord(domain_id, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
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

    auto DomainManager::renameDomain(const ID& domain_id, const std::string& new_name,
                                     ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (new_name.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain name cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        Status validation = UTF8Utils::validateStorageCapacity(
            new_name,
            CatalogConstants::MAX_IDENTIFIER_CHARS,
            CatalogConstants::MAX_IDENTIFIER_STORAGE,
            ctx
        );
        if (validation != Status::OK)
        {
            return validation;
        }

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        DomainInfo old_info = it->second;

        const std::string dialect_tag = IdentifierUtils::toUpper(old_info.dialect_tag);
        for (const auto& [id, info] : domain_cache_)
        {
            if (id == domain_id)
            {
                continue;
            }
            if (IdentifierUtils::namesConflict(new_name, false /*new_is_delimited*/,
                                               info.domain_name, false /*existing_is_delimited*/))
            {
                if (IdentifierUtils::toUpper(info.dialect_tag) == dialect_tag)
                {
                    SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                                      "Domain name already exists for dialect");
                    return Status::FILE_EXISTS;
                }
            }
        }

        DomainInfo& info = it->second;
        info.domain_name = new_name;
        info.last_modified_time = std::chrono::system_clock::now().time_since_epoch().count();

        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            it->second = old_info;
            return status;
        }

        LOG_INFO(CATALOG, "Renamed domain '%s' to '%s'",
                 old_info.domain_name.c_str(), new_name.c_str());

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
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

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

        /* ========================================================================
         * Phase 2 Enhancement: RECORD Field Extraction
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue COMPOSITE support required)
         *
         * DESCRIPTION:
         *   Extracts a named field value from a RECORD/COMPOSITE type value.
         *   This operation is fundamental for working with structured data types.
         *
         * PREREQUISITES:
         *   1. TypedValue extension to hold CompositeValue
         *      - Add DataType::COMPOSITE to TypedValue type system
         *      - Internal storage for field name->value mappings
         *      - Binary serialization format for COMPOSITE types
         *
         *   2. Binary decoding of COMPOSITE from TypedValue storage
         *      - Deserialize binary format into in-memory structure
         *      - Support for nested COMPOSITE types
         *      - Handle NULL field values
         *
         *   3. Field extraction from decoded CompositeValue
         *      - Field name lookup (case-sensitive)
         *      - Type-safe value extraction
         *      - Error handling for missing fields
         *
         * USAGE EXAMPLE:
         *   // Create RECORD domain: employee(name TEXT, age INT, salary DECIMAL)
         *   ID domain_id;
         *   std::vector<RecordField> fields = {
         *       RecordField("name", DataType::TEXT, false),
         *       RecordField("age", DataType::INT32, false),
         *       RecordField("salary", DataType::DECIMAL, true)
         *   };
         *   ASSERT_OK(domain_mgr->createRecordDomain(schema_id, "employee",
         *                                            fields, domain_id, &ctx));
         *
         *   // Create RECORD value: ('Alice Smith', 30, 75000.00)
         *   TypedValue employee_record = TypedValue::createComposite({
         *       {"name", TypedValue::createText("Alice Smith")},
         *       {"age", TypedValue::createInt32(30)},
         *       {"salary", TypedValue::createDecimal(75000, 2)}
         *   });
         *
         *   // Extract field value
         *   TypedValue name_field;
         *   ASSERT_OK(domain_mgr->extractField(employee_record, "name",
         *                                      name_field, &ctx));
         *   EXPECT_EQ(name_field.toString(), "Alice Smith");
         *   EXPECT_EQ(name_field.type(), DataType::TEXT);
         *
         *   // Extract numeric field
         *   TypedValue age_field;
         *   ASSERT_OK(domain_mgr->extractField(employee_record, "age",
         *                                      age_field, &ctx));
         *   EXPECT_EQ(age_field.asInt32(), 30);
         *
         *   // Handle missing field
         *   TypedValue invalid_field;
         *   EXPECT_EQ(domain_mgr->extractField(employee_record, "nonexistent",
         *                                      invalid_field, &ctx),
         *             Status::NOT_FOUND);
         *
         * SQL EQUIVALENT:
         *   SELECT (employee_data).name FROM employees;
         *   SELECT rec.field_name FROM table;
         *
         * RELATED FUNCTIONS:
         *   - createRecordDomain(): Create RECORD domain definition
         *   - getRecordField(): Get field metadata from domain
         *   - TypedValue::createComposite(): Create COMPOSITE value (future)
         *
         * IMPLEMENTATION NOTES:
         *   - Field names are case-sensitive
         *   - NULL field values must be supported
         *   - Nested RECORD types should be handled recursively
         *   - Performance: O(n) field lookup, consider hash map for large records
         *
         * ERROR CASES:
         *   - Status::TYPE_MISMATCH: Value is not COMPOSITE/RECORD type
         *   - Status::NOT_FOUND: Field name does not exist in domain
         *   - Status::INVALID_ARGUMENT: field_name is empty
         *
         * ESTIMATED EFFORT: 6-8 hours
         *   - TypedValue COMPOSITE storage: 3-4 hours
         *   - Binary format design & implementation: 2-3 hours
         *   - Field extraction logic: 1 hour
         *   - Testing (10+ test cases): 1 hour
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "RECORD field extraction requires TypedValue COMPOSITE support (Phase 2)");
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
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

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
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

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

        /* ========================================================================
         * Phase 2 Enhancement: SET Contains Operation
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VECTOR support required)
         *
         * DESCRIPTION:
         *   Checks if a SET/VECTOR value contains a specific element.
         *   Implements PostgreSQL's @> (contains) operator for arrays/sets.
         *
         * PREREQUISITES:
         *   1. TypedValue extension to access VectorValue elements
         *      - Add iterator interface for VECTOR types
         *      - Support element access by index
         *      - Handle heterogeneous element types
         *
         *   2. Element iteration through vector
         *      - Efficient iteration over all elements
         *      - Support for large sets (1000+ elements)
         *
         *   3. Element comparison for membership testing
         *      - Type-safe equality comparison
         *      - Support for all comparable types
         *      - NULL handling in sets
         *
         * USAGE EXAMPLE:
         *   // Create SET domain for tags
         *   ID domain_id;
         *   ASSERT_OK(domain_mgr->createSetDomain(schema_id, "tag_set",
         *                                          DataType::TEXT, domain_id, &ctx));
         *
         *   // Create SET value: {'database', 'nosql', 'distributed'}
         *   TypedValue tag_set = TypedValue::createVector({
         *       TypedValue::createText("database"),
         *       TypedValue::createText("nosql"),
         *       TypedValue::createText("distributed")
         *   });
         *
         *   // Check if set contains 'nosql'
         *   TypedValue search_elem = TypedValue::createText("nosql");
         *   bool contains = false;
         *   ASSERT_OK(domain_mgr->setContains(tag_set, search_elem, contains, &ctx));
         *   EXPECT_TRUE(contains);
         *
         *   // Check for non-existent element
         *   TypedValue missing = TypedValue::createText("mysql");
         *   ASSERT_OK(domain_mgr->setContains(tag_set, missing, contains, &ctx));
         *   EXPECT_FALSE(contains);
         *
         * SQL EQUIVALENT:
         *   SELECT tags @> ARRAY['nosql'] FROM documents;
         *   SELECT 'nosql' = ANY(tags) FROM documents;
         *
         * PERFORMANCE:
         *   - Time complexity: O(n) where n = set size
         *   - Consider hash-based membership for large sets (n > 100)
         *
         * ERROR CASES:
         *   - Status::TYPE_MISMATCH: set_value is not VECTOR type
         *   - Status::TYPE_MISMATCH: element type incompatible with set element type
         *
         * ESTIMATED EFFORT: 3-4 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET contains operation requires TypedValue VECTOR element access (Phase 2)");
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

        /* ========================================================================
         * Phase 2 Enhancement: SET Overlap Operation
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VECTOR support required)
         *
         * DESCRIPTION:
         *   Checks if two SETs have any elements in common.
         *   Implements PostgreSQL's && (overlap) operator for arrays.
         *
         * USAGE EXAMPLE:
         *   TypedValue set1 = TypedValue::createVector({
         *       TypedValue::createInt32(1),
         *       TypedValue::createInt32(2),
         *       TypedValue::createInt32(3)
         *   });
         *
         *   TypedValue set2 = TypedValue::createVector({
         *       TypedValue::createInt32(3),
         *       TypedValue::createInt32(4),
         *       TypedValue::createInt32(5)
         *   });
         *
         *   bool overlaps = false;
         *   ASSERT_OK(domain_mgr->setsOverlap(set1, set2, overlaps, &ctx));
         *   EXPECT_TRUE(overlaps);  // Both contain 3
         *
         * SQL EQUIVALENT:
         *   SELECT tags1 && tags2 FROM table;
         *
         * PERFORMANCE:
         *   - Time complexity: O(n*m) naive, O(n+m) with hash set
         *   - Optimize for small sets (n,m < 10): use nested loops
         *   - Optimize for large sets: use hash set for smaller set, iterate larger
         *
         * ESTIMATED EFFORT: 2-3 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET overlap operation requires TypedValue VECTOR element access (Phase 2)");
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

        /* ========================================================================
         * Phase 2 Enhancement: SET Union Operation
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VECTOR construction required)
         *
         * DESCRIPTION:
         *   Creates a new SET containing all unique elements from both input sets.
         *   Implements set theory union: A ∪ B
         *
         * USAGE EXAMPLE:
         *   TypedValue set1 = TypedValue::createVector({
         *       TypedValue::createText("a"),
         *       TypedValue::createText("b")
         *   });
         *
         *   TypedValue set2 = TypedValue::createVector({
         *       TypedValue::createText("b"),
         *       TypedValue::createText("c")
         *   });
         *
         *   TypedValue result;
         *   ASSERT_OK(domain_mgr->setUnion(set1, set2, result, &ctx));
         *   // result = {'a', 'b', 'c'}  (duplicates removed)
         *
         * SQL EQUIVALENT:
         *   SELECT array_union(tags1, tags2) FROM table;
         *   SELECT DISTINCT unnest(tags1 || tags2) FROM table;
         *
         * IMPLEMENTATION NOTES:
         *   - Use hash set to track unique elements
         *   - Preserve element order from set1, then set2
         *   - Handle NULL elements (include only once if present in either set)
         *
         * PERFORMANCE:
         *   - Time complexity: O(n + m)
         *   - Space complexity: O(n + m) for result set
         *
         * ESTIMATED EFFORT: 3-4 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET union operation requires TypedValue VECTOR construction (Phase 2)");
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

        /* ========================================================================
         * Phase 2 Enhancement: SET Intersection Operation
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VECTOR construction required)
         *
         * DESCRIPTION:
         *   Creates a new SET containing only elements present in both input sets.
         *   Implements set theory intersection: A ∩ B
         *
         * USAGE EXAMPLE:
         *   TypedValue set1 = TypedValue::createVector({
         *       TypedValue::createInt32(1),
         *       TypedValue::createInt32(2),
         *       TypedValue::createInt32(3)
         *   });
         *
         *   TypedValue set2 = TypedValue::createVector({
         *       TypedValue::createInt32(2),
         *       TypedValue::createInt32(3),
         *       TypedValue::createInt32(4)
         *   });
         *
         *   TypedValue result;
         *   ASSERT_OK(domain_mgr->setIntersection(set1, set2, result, &ctx));
         *   // result = {2, 3}
         *
         * SQL EQUIVALENT:
         *   SELECT ARRAY(SELECT unnest(a1) INTERSECT SELECT unnest(a2));
         *
         * IMPLEMENTATION NOTES:
         *   - Build hash set from smaller input set
         *   - Iterate larger set, checking membership
         *   - Result preserves order from first set
         *
         * PERFORMANCE:
         *   - Time complexity: O(min(n,m) + max(n,m))
         *   - Optimize: always build hash set from smaller input
         *
         * ESTIMATED EFFORT: 3-4 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET intersection operation requires TypedValue VECTOR construction (Phase 2)");
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

        /* ========================================================================
         * Phase 2 Enhancement: SET Difference Operation
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VECTOR construction required)
         *
         * DESCRIPTION:
         *   Creates a new SET containing elements in set1 but not in set2.
         *   Implements set theory difference: A \ B (or A - B)
         *
         * USAGE EXAMPLE:
         *   TypedValue set1 = TypedValue::createVector({
         *       TypedValue::createText("apple"),
         *       TypedValue::createText("banana"),
         *       TypedValue::createText("cherry")
         *   });
         *
         *   TypedValue set2 = TypedValue::createVector({
         *       TypedValue::createText("banana"),
         *       TypedValue::createText("durian")
         *   });
         *
         *   TypedValue result;
         *   ASSERT_OK(domain_mgr->setDifference(set1, set2, result, &ctx));
         *   // result = {'apple', 'cherry'}
         *
         * SQL EQUIVALENT:
         *   SELECT ARRAY(SELECT unnest(a1) EXCEPT SELECT unnest(a2));
         *
         * IMPLEMENTATION NOTES:
         *   - Build hash set from set2 for O(1) membership checks
         *   - Iterate set1, excluding elements present in hash set
         *   - Result preserves order from set1
         *   - Note: NOT symmetric (A-B ≠ B-A)
         *
         * PERFORMANCE:
         *   - Time complexity: O(n + m) where n = |set1|, m = |set2|
         *   - Space complexity: O(m) for hash set + O(k) for result where k ≤ n
         *
         * RELATED OPERATIONS:
         *   - Symmetric difference: (A-B) ∪ (B-A) = (A ∪ B) - (A ∩ B)
         *   - Can be implemented using union, intersection, and difference
         *
         * ESTIMATED EFFORT: 3-4 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "SET difference operation requires TypedValue VECTOR construction (Phase 2)");
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
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

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
        /* ========================================================================
         * Phase 2 Enhancement: VARIANT Type Extraction
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VARIANT support required)
         *
         * DESCRIPTION:
         *   Extracts the runtime type tag from a VARIANT value.
         *   Equivalent to SQL: EXTRACT(DATATYPE FROM variant_value)
         *   Similar to TypeScript's typeof operator or C++ std::variant::index()
         *
         * PREREQUISITES:
         *   1. TypedValue extension to hold VariantValue with runtime type tag
         *      - Add DataType::VARIANT to TypedValue type system
         *      - Internal discriminated union structure
         *      - Type tag storage (1 byte overhead)
         *
         *   2. Runtime type extraction from VariantValue
         *      - Efficient type tag access (O(1))
         *      - Support for all allowed types in domain
         *
         * USAGE EXAMPLE:
         *   // Create VARIANT domain allowing INT32, TEXT, DECIMAL
         *   ID domain_id;
         *   std::vector<DataType> allowed = {
         *       DataType::INT32,
         *       DataType::TEXT,
         *       DataType::DECIMAL
         *   };
         *   ASSERT_OK(domain_mgr->createVariantDomain(schema_id, "flexible_value",
         *                                              allowed, domain_id, &ctx));
         *
         *   // Create VARIANT value containing INT32
         *   TypedValue variant = TypedValue::createVariant(
         *       DataType::INT32,
         *       TypedValue::createInt32(42)
         *   );
         *
         *   // Extract runtime type
         *   DataType runtime_type;
         *   ASSERT_OK(domain_mgr->extractDataType(variant, runtime_type, &ctx));
         *   EXPECT_EQ(runtime_type, DataType::INT32);
         *
         *   // Create VARIANT containing TEXT
         *   TypedValue text_variant = TypedValue::createVariant(
         *       DataType::TEXT,
         *       TypedValue::createText("hello")
         *   );
         *   ASSERT_OK(domain_mgr->extractDataType(text_variant, runtime_type, &ctx));
         *   EXPECT_EQ(runtime_type, DataType::TEXT);
         *
         * SQL EQUIVALENT:
         *   SELECT EXTRACT(DATATYPE FROM json_value) FROM table;
         *   -- PostgreSQL JSON: SELECT jsonb_typeof(col) FROM table;
         *
         * USE CASES:
         *   - Polymorphic data handling
         *   - JSON-like flexible schemas
         *   - Union types in type systems
         *   - Dynamic dispatch based on runtime type
         *
         * IMPLEMENTATION NOTES:
         *   - Type tag should be first field for cache locality
         *   - Validation: extracted type must be in domain's allowed_types
         *   - Consider type ID instead of enum for extensibility
         *
         * ERROR CASES:
         *   - Status::TYPE_MISMATCH: Value is not VARIANT type
         *   - Status::CORRUPTION: Type tag is invalid/corrupted
         *
         * ESTIMATED EFFORT: 4-5 hours
         *   - TypedValue VARIANT storage design: 2-3 hours
         *   - Type extraction implementation: 1 hour
         *   - Testing: 1 hour
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "VARIANT type extraction requires TypedValue VARIANT support (Phase 2)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::isOfType(const TypedValue& variant_value,
                                DataType expected_type,
                                bool& result,
                                ErrorContext* ctx) -> Status
    {
        /* ========================================================================
         * Phase 2 Enhancement: VARIANT Type Checking
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VARIANT support required)
         *
         * DESCRIPTION:
         *   Checks if a VARIANT value currently holds a specific type.
         *   Equivalent to SQL: value IS OF (TYPE expected_type)
         *   Similar to TypeScript's instanceof or C++ std::holds_alternative<T>()
         *
         * USAGE EXAMPLE:
         *   // Create VARIANT allowing multiple numeric types
         *   TypedValue variant = TypedValue::createVariant(
         *       DataType::INT32,
         *       TypedValue::createInt32(100)
         *   );
         *
         *   // Check if variant holds INT32
         *   bool is_int32 = false;
         *   ASSERT_OK(domain_mgr->isOfType(variant, DataType::INT32, is_int32, &ctx));
         *   EXPECT_TRUE(is_int32);
         *
         *   // Check if variant holds TEXT (should be false)
         *   bool is_text = false;
         *   ASSERT_OK(domain_mgr->isOfType(variant, DataType::TEXT, is_text, &ctx));
         *   EXPECT_FALSE(is_text);
         *
         *   // Use in conditional logic
         *   if (is_int32) {
         *       // Safe to extract as INT32
         *       TypedValue int_value;
         *       ASSERT_OK(domain_mgr->variantCast(variant, DataType::INT32,
         *                                          int_value, &ctx));
         *       int32_t num = int_value.asInt32();
         *   }
         *
         * SQL EQUIVALENT:
         *   SELECT CASE WHEN col IS OF (TYPE INTEGER) THEN 'int'
         *               WHEN col IS OF (TYPE TEXT) THEN 'text'
         *               ELSE 'unknown' END
         *   FROM table;
         *
         * USE CASES:
         *   - Type-safe variant access
         *   - Conditional processing based on runtime type
         *   - Type guards for safe casting
         *   - Pattern matching on variant types
         *
         * IMPLEMENTATION NOTES:
         *   - Simple type tag comparison (O(1))
         *   - Can be inlined for performance
         *   - Validate expected_type is in domain's allowed_types
         *
         * PERFORMANCE:
         *   - Time complexity: O(1)
         *   - Space complexity: O(1)
         *   - Should be highly optimized (hot path for variant access)
         *
         * ERROR CASES:
         *   - Status::TYPE_MISMATCH: variant_value is not VARIANT type
         *   - Status::INVALID_ARGUMENT: expected_type is DataType::UNKNOWN
         *
         * ESTIMATED EFFORT: 2-3 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "VARIANT type checking requires TypedValue VARIANT support (Phase 2)");
        return Status::NOT_IMPLEMENTED;
    }

    auto DomainManager::variantCast(const TypedValue& variant_value,
                                   DataType target_type,
                                   TypedValue& result,
                                   ErrorContext* ctx) -> Status
    {
        /* ========================================================================
         * Phase 2 Enhancement: VARIANT Type-Safe Casting
         * ========================================================================
         *
         * STATUS: Deferred to Phase 2 (TypedValue VARIANT support required)
         *
         * DESCRIPTION:
         *   Extracts the underlying value from a VARIANT with type checking.
         *   Returns error if variant doesn't currently hold target_type.
         *   Similar to C++ std::get<T>(variant) or Rust's match on enum.
         *
         * PREREQUISITES:
         *   1. TypedValue extension to hold VariantValue
         *      - Discriminated union storage
         *      - Type-safe value extraction
         *
         *   2. Runtime type extraction
         *      - Get current type tag
         *      - Compare with target_type
         *
         *   3. Type-safe value extraction and casting
         *      - Extract value only if types match
         *      - Optional implicit casting (e.g., INT32 → INT64)
         *
         *   4. Validation that cast is to allowed type
         *      - Check target_type is in domain's allowed_types
         *      - Prevent unsafe casts
         *
         * USAGE EXAMPLE:
         *   // Create VARIANT domain
         *   ID domain_id;
         *   std::vector<DataType> allowed = {
         *       DataType::INT32,
         *       DataType::TEXT,
         *       DataType::DECIMAL
         *   };
         *   ASSERT_OK(domain_mgr->createVariantDomain(schema_id, "flexible_col",
         *                                              allowed, domain_id, &ctx));
         *
         *   // Create VARIANT holding INT32
         *   TypedValue variant = TypedValue::createVariant(
         *       DataType::INT32,
         *       TypedValue::createInt32(42)
         *   );
         *
         *   // Safe cast to INT32 (should succeed)
         *   TypedValue int_result;
         *   ASSERT_OK(domain_mgr->variantCast(variant, DataType::INT32,
         *                                      int_result, &ctx));
         *   EXPECT_EQ(int_result.asInt32(), 42);
         *   EXPECT_EQ(int_result.type(), DataType::INT32);
         *
         *   // Attempt to cast to TEXT (should fail - type mismatch)
         *   TypedValue text_result;
         *   EXPECT_EQ(domain_mgr->variantCast(variant, DataType::TEXT,
         *                                      text_result, &ctx),
         *             Status::TYPE_MISMATCH);
         *
         *   // Pattern matching style usage
         *   TypedValue result;
         *   if (domain_mgr->variantCast(variant, DataType::INT32, result, &ctx) == Status::OK) {
         *       processInteger(result.asInt32());
         *   } else if (domain_mgr->variantCast(variant, DataType::TEXT, result, &ctx) == Status::OK) {
         *       processText(result.asText());
         *   } else {
         *       handleUnknownType();
         *   }
         *
         * SQL EQUIVALENT:
         *   -- PostgreSQL JSONB casting
         *   SELECT (col::jsonb->>'key')::INTEGER FROM table;
         *
         *   -- SQL/MM VARIANT casting
         *   SELECT CAST(variant_col AS INTEGER) FROM table;
         *
         * ADVANCED USAGE: Implicit Casting
         *   // VARIANT holds INT32, cast to INT64 (widening cast)
         *   TypedValue int32_variant = TypedValue::createVariant(
         *       DataType::INT32,
         *       TypedValue::createInt32(100)
         *   );
         *
         *   TypedValue int64_result;
         *   // Option 1: Strict casting (fails if types don't match exactly)
         *   EXPECT_EQ(domain_mgr->variantCast(int32_variant, DataType::INT64,
         *                                      int64_result, &ctx),
         *             Status::TYPE_MISMATCH);
         *
         *   // Option 2: With implicit casting (future enhancement)
         *   // EXPECT_OK(domain_mgr->variantCastWithConversion(...));
         *
         * IMPLEMENTATION NOTES:
         *   - Phase 2.0: Strict type matching only (no implicit casts)
         *   - Phase 2.1: Add implicit widening casts (INT32→INT64, FLOAT→DOUBLE)
         *   - Phase 2.2: Add explicit conversion functions
         *   - Always validate target_type is in domain's allowed_types
         *   - Consider returning std::optional<TypedValue> instead of Status
         *
         * PERFORMANCE:
         *   - Type check: O(1) - single tag comparison
         *   - Value extraction: O(1) - direct memory access
         *   - No dynamic allocation for small types (SBO - Small Buffer Optimization)
         *
         * ERROR CASES:
         *   - Status::TYPE_MISMATCH: variant doesn't hold target_type
         *   - Status::INVALID_ARGUMENT: target_type not in domain's allowed_types
         *   - Status::CORRUPTION: variant type tag is invalid
         *
         * RELATED FUNCTIONS:
         *   - extractDataType(): Get current type without extracting value
         *   - isOfType(): Check type before casting
         *
         * ESTIMATED EFFORT: 5-6 hours
         *   - Type-safe extraction: 2 hours
         *   - Domain validation: 1 hour
         *   - Error handling: 1 hour
         *   - Testing (15+ test cases): 2 hours
         * ======================================================================== */

        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
            "VARIANT casting requires TypedValue VARIANT support (Phase 2)");
        return Status::NOT_IMPLEMENTED;
    }

    // ====================
    // Phase 6: Advanced Features
    // ====================

    auto DomainManager::setSecurityOptions(const ID& domain_id,
                                          const DomainSecurity& security,
                                          ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        // Update security options
        it->second.security = security;
        it->second.last_modified_time = std::time(nullptr);

        // Update catalog
        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Set security options for domain %s",
                domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::setIntegrityOptions(const ID& domain_id,
                                           const DomainIntegrity& integrity,
                                           ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        // Update integrity options
        it->second.integrity = integrity;
        it->second.last_modified_time = std::time(nullptr);

        // Update catalog
        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Set integrity options for domain %s",
                domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::setValidationOptions(const ID& domain_id,
                                            const DomainValidation& validation,
                                            ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        // Update validation options
        it->second.validation = validation;
        it->second.last_modified_time = std::time(nullptr);

        // Update catalog
        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Set validation options for domain %s",
                domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::setQualityOptions(const ID& domain_id,
                                         const DomainQuality& quality,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        // Update quality options
        it->second.quality = quality;
        it->second.last_modified_time = std::time(nullptr);

        // Update catalog
        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Set quality options for domain %s",
                domain_id.toString().c_str());

        return Status::OK;
    }

    namespace
    {
        std::string normalizePrivilegeName(const std::string& name)
        {
            std::string normalized;
            normalized.reserve(name.size());
            for (char ch : name)
            {
                if (std::isspace(static_cast<unsigned char>(ch)) || ch == '-')
                {
                    normalized.push_back('_');
                }
                else
                {
                    normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                }
            }
            return normalized;
        }

        bool parsePrivilegeName(const std::string& name, CatalogManager::Privilege& privilege_out)
        {
            std::string normalized = normalizePrivilegeName(name);
            if (normalized.empty())
            {
                return false;
            }
            if (normalized == "SELECT")
            {
                privilege_out = CatalogManager::Privilege::SELECT;
                return true;
            }
            if (normalized == "INSERT")
            {
                privilege_out = CatalogManager::Privilege::INSERT;
                return true;
            }
            if (normalized == "UPDATE")
            {
                privilege_out = CatalogManager::Privilege::UPDATE;
                return true;
            }
            if (normalized == "DELETE")
            {
                privilege_out = CatalogManager::Privilege::DELETE;
                return true;
            }
            if (normalized == "TRUNCATE")
            {
                privilege_out = CatalogManager::Privilege::TRUNCATE;
                return true;
            }
            if (normalized == "REFERENCES")
            {
                privilege_out = CatalogManager::Privilege::REFERENCES;
                return true;
            }
            if (normalized == "TRIGGER")
            {
                privilege_out = CatalogManager::Privilege::TRIGGER;
                return true;
            }
            if (normalized == "CREATE")
            {
                privilege_out = CatalogManager::Privilege::CREATE;
                return true;
            }
            if (normalized == "USAGE")
            {
                privilege_out = CatalogManager::Privilege::USAGE;
                return true;
            }
            if (normalized == "SEQUENCE_USAGE")
            {
                privilege_out = CatalogManager::Privilege::SEQUENCE_USAGE;
                return true;
            }
            if (normalized == "SEQUENCE_UPDATE")
            {
                privilege_out = CatalogManager::Privilege::SEQUENCE_UPDATE;
                return true;
            }
            if (normalized == "EXECUTE")
            {
                privilege_out = CatalogManager::Privilege::EXECUTE;
                return true;
            }
            if (normalized == "CONNECT")
            {
                privilege_out = CatalogManager::Privilege::CONNECT;
                return true;
            }
            if (normalized == "TEMPORARY")
            {
                privilege_out = CatalogManager::Privilege::TEMPORARY;
                return true;
            }
            if (normalized == "ALL")
            {
                privilege_out = CatalogManager::Privilege::ALL;
                return true;
            }
            return false;
        }

        Status checkPermissionMask(CatalogManager* catalog,
                                   const ID& domain_id,
                                   const ID& user_id,
                                   uint32_t permission_mask,
                                   bool& has_privilege_out,
                                   ErrorContext* ctx)
        {
            has_privilege_out = false;

            constexpr std::array<CatalogManager::Privilege, 14> kPrivileges = {
                CatalogManager::Privilege::SELECT,
                CatalogManager::Privilege::INSERT,
                CatalogManager::Privilege::UPDATE,
                CatalogManager::Privilege::DELETE,
                CatalogManager::Privilege::TRUNCATE,
                CatalogManager::Privilege::REFERENCES,
                CatalogManager::Privilege::TRIGGER,
                CatalogManager::Privilege::CREATE,
                CatalogManager::Privilege::USAGE,
                CatalogManager::Privilege::SEQUENCE_USAGE,
                CatalogManager::Privilege::SEQUENCE_UPDATE,
                CatalogManager::Privilege::EXECUTE,
                CatalogManager::Privilege::CONNECT,
                CatalogManager::Privilege::TEMPORARY
            };

            for (auto privilege : kPrivileges)
            {
                uint32_t bit = static_cast<uint32_t>(privilege);
                if ((permission_mask & bit) == 0)
                {
                    continue;
                }
                bool has_permission = false;
                Status status = catalog->hasPermission(
                    user_id,
                    domain_id,
                    CatalogManager::PermissionObjectType::DOMAIN,
                    privilege,
                    has_permission,
                    ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                if (has_permission)
                {
                    has_privilege_out = true;
                    return Status::OK;
                }
            }

            return Status::OK;
        }

        Status checkMaskingPrivilegeInternal(Database* db,
                                             const DomainSecurity& security,
                                             const ID& domain_id,
                                             const ID& user_id,
                                             bool& has_privilege_out,
                                             ErrorContext* ctx)
        {
            has_privilege_out = false;

            if (isZeroUuidLocal(user_id))
            {
                return Status::OK;
            }

            CatalogManager* catalog = db->catalog_manager();
            if (!catalog)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
                return Status::INVALID_ARGUMENT;
            }

            if (security.permission_mask != 0)
            {
                return checkPermissionMask(catalog, domain_id, user_id,
                                           security.permission_mask, has_privilege_out, ctx);
            }

            if (security.required_privilege_for_unmasked.empty())
            {
                return Status::OK;
            }

            std::string normalized = normalizePrivilegeName(security.required_privilege_for_unmasked);
            if (normalized == "NONE")
            {
                return Status::OK;
            }

            CatalogManager::Privilege privilege;
            if (!parsePrivilegeName(security.required_privilege_for_unmasked, privilege))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown masking privilege");
                return Status::INVALID_ARGUMENT;
            }

            bool has_permission = false;
            Status status = catalog->hasPermission(
                user_id,
                domain_id,
                CatalogManager::PermissionObjectType::DOMAIN,
                privilege,
                has_permission,
                ctx);
            if (status != Status::OK)
            {
                return status;
            }
            has_privilege_out = has_permission;
            return Status::OK;
        }
    }

    auto DomainManager::applyMasking(const ID& domain_id,
                                    const ID& user_id,
                                    const TypedValue& value,
                                    TypedValue& masked_value,
                                    ErrorContext* ctx) -> Status
    {
        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (value.isNull() || domain.security.masking_config.type == MaskingType::NONE)
        {
            masked_value = value;
            return Status::OK;
        }

        bool has_privilege = false;
        status = checkMaskingPrivilegeInternal(db_, domain.security, domain_id,
                                               user_id, has_privilege, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::string raw_value;
        switch (value.type())
        {
        case DataType::VARCHAR:
            raw_value = value.getVarchar();
            break;
        case DataType::TEXT:
            raw_value = value.getText();
            break;
        case DataType::CHAR:
            raw_value = value.getChar();
            break;
        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Masking only supported for string domain values");
            return Status::INVALID_ARGUMENT;
        }

        std::string masked;
        status = DataMasking::applyMasking(raw_value, domain.security.masking_config,
                                           has_privilege, masked, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        switch (value.type())
        {
        case DataType::VARCHAR:
            masked_value = TypedValue::makeVarchar(masked);
            break;
        case DataType::TEXT:
            masked_value = TypedValue::makeText(masked);
            break;
        case DataType::CHAR:
            masked_value = TypedValue::makeChar(masked);
            break;
        default:
            masked_value = value;
            break;
        }

        return Status::OK;
    }

    auto DomainManager::checkMaskingPrivilege(const ID& domain_id,
                                              const ID& user_id,
                                              bool& has_privilege_out,
                                              ErrorContext* ctx) -> Status
    {
        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return checkMaskingPrivilegeInternal(db_, domain.security, domain_id,
                                             user_id, has_privilege_out, ctx);
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

        // Check if domain already exists (for updates)
        DomainRecord* existing_record = nullptr;
        for (uint32_t i = 0; i < catalog_page->record_count; i++)
        {
            auto* record = reinterpret_cast<DomainRecord*>(
                catalog_page->data + (i * sizeof(DomainRecord)));

            if (record->domain_id == domain.domain_id)
            {
                existing_record = record;
                break;
            }
        }

        DomainRecord* record;
        if (existing_record)
        {
            // Update existing record
            record = existing_record;
        }
        else
        {
            // Add new record
            if (catalog_page->record_count >= (db_->page_size() - sizeof(DomainCatalogPage)) / sizeof(DomainRecord))
            {
                bp->unpinPage(domains_table_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Domain catalog page full");
                return Status::IO_ERROR;
            }

            record = reinterpret_cast<DomainRecord*>(
                catalog_page->data + (catalog_page->record_count * sizeof(DomainRecord)));
            catalog_page->record_count++;
        }

        // Populate record from DomainInfo
        record->domain_id = domain.domain_id;
        record->schema_id = domain.schema_id;

        std::strncpy(record->domain_name, domain.domain_name.c_str(), sizeof(record->domain_name) - 1);
        record->domain_name[sizeof(record->domain_name) - 1] = '\0';

        record->domain_type = static_cast<uint8_t>(domain.domain_type);
        record->base_type = static_cast<uint16_t>(domain.base_type);
        record->precision = domain.precision;
        record->scale = domain.scale;
        record->nullable = domain.nullable ? 1 : 0;

        std::strncpy(record->default_value, domain.default_value.c_str(), sizeof(record->default_value) - 1);
        record->default_value[sizeof(record->default_value) - 1] = '\0';

        record->parent_domain_id = domain.parent_domain_id;
        record->is_valid = 1;
        record->created_time = domain.created_time;
        record->last_modified_time = domain.last_modified_time;
        record->set_element_type = static_cast<uint16_t>(domain.set_element_type);

        std::strncpy(record->dialect_tag, domain.dialect_tag.c_str(), sizeof(record->dialect_tag) - 1);
        record->dialect_tag[sizeof(record->dialect_tag) - 1] = '\0';

        std::strncpy(record->compat_name, domain.compat_name.c_str(), sizeof(record->compat_name) - 1);
        record->compat_name[sizeof(record->compat_name) - 1] = '\0';

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            bp->unpinPage(domains_table_page_, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        uint64_t xmin = ConnectionContext::getCurrentTransactionId();

        record->security_oid = 0;
        std::string security_blob = serializeDomainSecurity(domain.security);
        if (!security_blob.empty())
        {
            status = catalog->storeStringInToast(security_blob, xmin, record->security_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(domains_table_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain security in TOAST");
                return status;
            }
        }

        record->integrity_oid = 0;
        std::string integrity_blob = serializeDomainIntegrity(domain.integrity);
        if (!integrity_blob.empty())
        {
            status = catalog->storeStringInToast(integrity_blob, xmin, record->integrity_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(domains_table_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain integrity in TOAST");
                return status;
            }
        }

        record->validation_oid = 0;
        std::string validation_blob = serializeDomainValidation(domain.validation);
        if (!validation_blob.empty())
        {
            status = catalog->storeStringInToast(validation_blob, xmin, record->validation_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(domains_table_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain validation in TOAST");
                return status;
            }
        }

        record->quality_oid = 0;
        std::string quality_blob = serializeDomainQuality(domain.quality);
        if (!quality_blob.empty())
        {
            status = catalog->storeStringInToast(quality_blob, xmin, record->quality_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(domains_table_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain quality in TOAST");
                return status;
            }
        }

        // TODO: Persist constraints, fields, enum_values to TOAST
        // For now, constraints_oid, fields_oid, enum_values_oid remain 0

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
                info.dialect_tag = record->dialect_tag;
                if (info.dialect_tag.empty())
                {
                    info.dialect_tag = "scratchbird";
                }
                info.compat_name = record->compat_name;

                CatalogManager* catalog = db_->catalog_manager();
                if (!catalog)
                {
                    bp->unpinPage(domains_table_page_, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
                    return Status::INVALID_ARGUMENT;
                }

                uint64_t xmin = ConnectionContext::getCurrentTransactionId();
                std::string blob;

                if (record->security_oid != 0)
                {
                    Status load_status = catalog->loadStringFromToast(record->security_oid,
                                                                      xmin, blob, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    load_status = deserializeDomainSecurity(blob, info.security, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    blob.clear();
                }

                if (record->integrity_oid != 0)
                {
                    Status load_status = catalog->loadStringFromToast(record->integrity_oid,
                                                                      xmin, blob, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    load_status = deserializeDomainIntegrity(blob, info.integrity, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    blob.clear();
                }

                if (record->validation_oid != 0)
                {
                    Status load_status = catalog->loadStringFromToast(record->validation_oid,
                                                                      xmin, blob, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    load_status = deserializeDomainValidation(blob, info.validation, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    blob.clear();
                }

                if (record->quality_oid != 0)
                {
                    Status load_status = catalog->loadStringFromToast(record->quality_oid,
                                                                      xmin, blob, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    load_status = deserializeDomainQuality(blob, info.quality, ctx);
                    if (load_status != Status::OK)
                    {
                        bp->unpinPage(domains_table_page_, false, ctx);
                        return load_status;
                    }
                    blob.clear();
                }

                // Phase 3 Enhancement: Load constraints, fields, enum_values from TOAST
                // Currently domain constraints are stored inline; TOAST support for large constraint lists

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

    // Helper function for SQL LIKE pattern matching
    // Supports % (zero or more chars) and _ (exactly one char)
    static bool matchLikePattern(const std::string& str, const std::string& pattern)
    {
        size_t str_idx = 0;
        size_t pat_idx = 0;
        size_t str_len = str.length();
        size_t pat_len = pattern.length();

        // Last positions where we saw % and the corresponding string position
        size_t star_idx = std::string::npos;
        size_t str_match_idx = 0;

        while (str_idx < str_len) {
            if (pat_idx < pat_len && (pattern[pat_idx] == str[str_idx] || pattern[pat_idx] == '_')) {
                // Character match or single-char wildcard
                str_idx++;
                pat_idx++;
            } else if (pat_idx < pat_len && pattern[pat_idx] == '%') {
                // Multi-char wildcard - save position
                star_idx = pat_idx;
                str_match_idx = str_idx;
                pat_idx++;
            } else if (star_idx != std::string::npos) {
                // No match, but we have a previous %, backtrack
                pat_idx = star_idx + 1;
                str_match_idx++;
                str_idx = str_match_idx;
            } else {
                // No match and no % to backtrack
                return false;
            }
        }

        // Consume trailing % in pattern
        while (pat_idx < pat_len && pattern[pat_idx] == '%') {
            pat_idx++;
        }

        // Match only if we consumed entire pattern
        return pat_idx == pat_len;
    }

    auto DomainManager::validateCheckConstraint(const DomainInfo& domain,
                                                const TypedValue& value,
                                                const DomainConstraint& constraint,
                                                ErrorContext* ctx) -> Status
    {
        // Parse the constraint expression if needed
        // The expression should use "VALUE" to refer to the domain value
        // Example: "VALUE > 0" or "VALUE LIKE '%@%.%'"

        // For now, implement basic constraint checking
        // Full expression evaluation would require parser integration

        const std::string& expr = constraint.expression;

        // Handle common constraint patterns
        // Pattern 1: VALUE > number
        // Pattern 2: VALUE >= number
        // Pattern 3: VALUE < number
        // Pattern 4: VALUE <= number
        // Pattern 5: VALUE = number
        // Pattern 6: VALUE != number
        // Pattern 7: VALUE LIKE 'pattern'
        // Pattern 8: VALUE BETWEEN min AND max

        // For alpha phase, implement basic numeric comparison
        // Phase 3 Enhancement: Full expression parser integration for complex constraint expressions

        // Skip empty expressions
        if (expr.empty()) {
            return Status::OK;
        }

        // Simple pattern matching for basic constraints
        // This handles the most common cases: VALUE op literal

        size_t value_pos = expr.find("VALUE");
        if (value_pos == std::string::npos) {
            // No VALUE reference - malformed constraint
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                            "CHECK constraint must reference VALUE");
            return Status::INVALID_ARGUMENT;
        }

        // Extract operator and compare value
        std::string remaining = expr.substr(value_pos + 5); // Skip "VALUE"

        // Trim whitespace
        size_t first = remaining.find_first_not_of(" \t");
        if (first == std::string::npos) {
            // Just "VALUE" - always true
            return Status::OK;
        }
        remaining = remaining.substr(first);

        // Check for common operators
        bool result = true; // Default to true

        if (remaining.substr(0, 2) == ">=") {
            // VALUE >= number
            std::string num_str = remaining.substr(2);
            num_str.erase(0, num_str.find_first_not_of(" \t"));
            double threshold = std::stod(num_str);

            if (value.type() == DataType::INT32) {
                result = (value.getInt32() >= threshold);
            } else if (value.type() == DataType::INT64) {
                result = (value.getInt64() >= threshold);
            } else if (value.type() == DataType::FLOAT32) {
                result = (value.getFloat32() >= threshold);
            } else if (value.type() == DataType::FLOAT64) {
                result = (value.getFloat64() >= threshold);
            }
        } else if (remaining.substr(0, 2) == "<=") {
            // VALUE <= number
            std::string num_str = remaining.substr(2);
            num_str.erase(0, num_str.find_first_not_of(" \t"));
            double threshold = std::stod(num_str);

            if (value.type() == DataType::INT32) {
                result = (value.getInt32() <= threshold);
            } else if (value.type() == DataType::INT64) {
                result = (value.getInt64() <= threshold);
            } else if (value.type() == DataType::FLOAT32) {
                result = (value.getFloat32() <= threshold);
            } else if (value.type() == DataType::FLOAT64) {
                result = (value.getFloat64() <= threshold);
            }
        } else if (remaining[0] == '>') {
            // VALUE > number
            std::string num_str = remaining.substr(1);
            num_str.erase(0, num_str.find_first_not_of(" \t"));
            double threshold = std::stod(num_str);

            if (value.type() == DataType::INT32) {
                result = (value.getInt32() > threshold);
            } else if (value.type() == DataType::INT64) {
                result = (value.getInt64() > threshold);
            } else if (value.type() == DataType::FLOAT32) {
                result = (value.getFloat32() > threshold);
            } else if (value.type() == DataType::FLOAT64) {
                result = (value.getFloat64() > threshold);
            }
        } else if (remaining[0] == '<') {
            // VALUE < number
            std::string num_str = remaining.substr(1);
            num_str.erase(0, num_str.find_first_not_of(" \t"));
            double threshold = std::stod(num_str);

            if (value.type() == DataType::INT32) {
                result = (value.getInt32() < threshold);
            } else if (value.type() == DataType::INT64) {
                result = (value.getInt64() < threshold);
            } else if (value.type() == DataType::FLOAT32) {
                result = (value.getFloat32() < threshold);
            } else if (value.type() == DataType::FLOAT64) {
                result = (value.getFloat64() < threshold);
            }
        } else if (remaining[0] == '=') {
            // VALUE = number
            std::string num_str = remaining.substr(1);
            num_str.erase(0, num_str.find_first_not_of(" \t"));
            double threshold = std::stod(num_str);

            if (value.type() == DataType::INT32) {
                result = (value.getInt32() == threshold);
            } else if (value.type() == DataType::INT64) {
                result = (value.getInt64() == threshold);
            } else if (value.type() == DataType::FLOAT32) {
                result = (std::abs(value.getFloat32() - threshold) < 1e-6);
            } else if (value.type() == DataType::FLOAT64) {
                result = (std::abs(value.getFloat64() - threshold) < 1e-9);
            }
        } else if (remaining.find("LIKE") != std::string::npos) {
            // VALUE LIKE 'pattern'
            // Extract pattern (between single quotes)
            size_t like_pos = remaining.find("LIKE");
            std::string after_like = remaining.substr(like_pos + 4);
            after_like.erase(0, after_like.find_first_not_of(" \t"));

            if (!after_like.empty() && after_like[0] == '\'') {
                size_t end_quote = after_like.find('\'', 1);
                if (end_quote != std::string::npos) {
                    std::string pattern = after_like.substr(1, end_quote - 1);

                    // Get string value
                    std::string str_value;
                    if (value.type() == DataType::VARCHAR) {
                        str_value = value.getVarchar();
                    } else if (value.type() == DataType::TEXT) {
                        str_value = value.getText();
                    } else if (value.type() == DataType::CHAR) {
                        str_value = value.getChar();
                    } else {
                        // Type mismatch - LIKE only works on strings
                        result = false;
                    }

                    if (result) {
                        // Perform SQL LIKE pattern matching
                        // % matches zero or more characters
                        // _ matches exactly one character
                        result = matchLikePattern(str_value, pattern);
                    }
                }
            }
        } else {
            // Unsupported constraint pattern for now
            // Log and allow (fail-open for unknown patterns)
            LOG_DEBUG(CATALOG, "Unsupported CHECK constraint pattern: {}", expr);
            return Status::OK;
        }

        if (!result) {
            std::string error_msg = "CHECK constraint violated: " + constraint.name + " (" + expr + ")";
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
            return Status::CONSTRAINT_VIOLATION;
        }

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
