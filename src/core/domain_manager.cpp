/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/global_uniqueness_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/composite.h"
#include "scratchbird/core/utf8_utils.h"
#include "scratchbird/core/normalization.h"
#include "scratchbird/core/domain_validation.h"
#include "scratchbird/core/quality_pipeline.h"
#include <cstring>
#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <openssl/sha.h>

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

        struct SystemDomainDef {
            const char* name;
            DataType base_type;
            uint32_t precision;
            uint32_t scale;
            const char* param_string = "";
            const char* fixed_uuid_literal = "";
        };

        const SystemDomainDef kLegacySystemDomains[] = {
            {"[sb_dom]UUID_V7", DataType::UUID, 0, 0},
            {"[sb_dom]NAME", DataType::VARCHAR, 128, 0},
            {"[sb_dom]NAME_64", DataType::VARCHAR, 64, 0},
            {"[sb_dom]NAME_256", DataType::VARCHAR, 256, 0},
            {"[sb_dom]NAME_512", DataType::VARCHAR, 512, 0},
            {"[sb_dom]NAME_1024", DataType::VARCHAR, 1024, 0},
            {"[sb_dom]BOOL", DataType::BOOLEAN, 0, 0},
            {"[sb_dom]BIT", DataType::BIT, 0, 0},
            {"[sb_dom]U8", DataType::UINT8, 0, 0},
            {"[sb_dom]U16", DataType::UINT16, 0, 0},
            {"[sb_dom]U32", DataType::UINT32, 0, 0},
            {"[sb_dom]U64", DataType::UINT64, 0, 0},
            {"[sb_dom]U128", DataType::UINT128, 0, 0},
            {"[sb_dom]I8", DataType::INT8, 0, 0},
            {"[sb_dom]I16", DataType::INT16, 0, 0},
            {"[sb_dom]I32", DataType::INT32, 0, 0},
            {"[sb_dom]I64", DataType::INT64, 0, 0},
            {"[sb_dom]I128", DataType::INT128, 0, 0},
            {"[sb_dom]F32", DataType::FLOAT32, 0, 0},
            {"[sb_dom]F64", DataType::FLOAT64, 0, 0},
            {"[sb_dom]DECIMAL", DataType::DECIMAL, 0, 0},
            {"[sb_dom]MONEY", DataType::MONEY, 0, 0},
            {"[sb_dom]DECFLOAT16", DataType::DECFLOAT16, 0, 0},
            {"[sb_dom]DECFLOAT34", DataType::DECFLOAT34, 0, 0},
            {"[sb_dom]TIME_US", DataType::UINT64, 0, 0},
            {"[sb_dom]DATE", DataType::DATE, 0, 0},
            {"[sb_dom]TIME", DataType::TIME, 0, 0},
            {"[sb_dom]TIMESTAMP", DataType::TIMESTAMP, 0, 0},
            {"[sb_dom]TIMESTAMPTZ", DataType::TIMESTAMP_WITH_ZONE, 0, 0},
            {"[sb_dom]TIME_TZ", DataType::TIME_WITH_ZONE, 0, 0},
            {"[sb_dom]INTERVAL", DataType::INTERVAL, 0, 0},
            {"[sb_dom]YEAR", DataType::YEAR, 0, 0},
            {"[sb_dom]SQLSTATE", DataType::CHAR, 5, 0},
            {"[sb_dom]HASH256", DataType::BINARY, 32, 0},
            {"[sb_dom]BINARY", DataType::BINARY, 0, 0},
            {"[sb_dom]VARBINARY", DataType::VARBINARY, 0, 0},
            {"[sb_dom]BLOB", DataType::BLOB, 0, 0},
            {"[sb_dom]BYTEA", DataType::BYTEA, 0, 0},
            {"[sb_dom]TEXT", DataType::TEXT, 0, 0},
            {"[sb_dom]JSON", DataType::JSON, 0, 0},
            {"[sb_dom]JSONB", DataType::JSONB, 0, 0},
            {"[sb_dom]XML", DataType::XML, 0, 0},
            {"[sb_dom]VECTOR", DataType::VECTOR, 0, 0},
            {"[sb_dom]POINT", DataType::POINT, 0, 0},
            {"[sb_dom]LINESTRING", DataType::LINESTRING, 0, 0},
            {"[sb_dom]POLYGON", DataType::POLYGON, 0, 0},
            {"[sb_dom]MULTIPOINT", DataType::MULTIPOINT, 0, 0},
            {"[sb_dom]MULTILINESTRING", DataType::MULTILINESTRING, 0, 0},
            {"[sb_dom]MULTIPOLYGON", DataType::MULTIPOLYGON, 0, 0},
            {"[sb_dom]GEOMETRYCOLLECTION", DataType::GEOMETRYCOLLECTION, 0, 0},
            {"[sb_dom]GEOMETRY", DataType::GEOMETRY, 0, 0},
            {"[sb_dom]INET", DataType::INET, 0, 0},
            {"[sb_dom]CIDR", DataType::CIDR, 0, 0},
            {"[sb_dom]MACADDR", DataType::MACADDR, 0, 0},
            {"[sb_dom]MACADDR8", DataType::MACADDR8, 0, 0},
            {"[sb_dom]TSVECTOR", DataType::TSVECTOR, 0, 0},
            {"[sb_dom]TSQUERY", DataType::TSQUERY, 0, 0},
            {"[sb_dom]RANGE_INT4", DataType::INT4RANGE, 0, 0},
            {"[sb_dom]RANGE_INT8", DataType::INT8RANGE, 0, 0},
            {"[sb_dom]RANGE_NUM", DataType::NUMRANGE, 0, 0},
            {"[sb_dom]RANGE_TS", DataType::TSRANGE, 0, 0},
            {"[sb_dom]RANGE_TSTZ", DataType::TSTZRANGE, 0, 0},
            {"[sb_dom]RANGE_DATE", DataType::DATERANGE, 0, 0},
            {"[sb_dom]ARRAY", DataType::ARRAY, 0, 0},
            {"[sb_dom]COMPOSITE", DataType::COMPOSITE, 0, 0},
            {"[sb_dom]DOMAIN", DataType::DOMAIN, 0, 0},
            {"[sb_dom]ROW", DataType::ROW, 0, 0},
            {"[sb_dom]ENUM", DataType::ENUM, 0, 0},
            {"[sb_dom]SET", DataType::SET, 0, 0},
            {"[sb_dom]VARIANT", DataType::VARIANT, 0, 0},
            {"[sb_dom]PAGE_ID", DataType::UINT32, 0, 0},
            {"[sb_dom]LOB_REF", DataType::UUID, 0, 0},
            {"[sb_dom]OBJTYPE", DataType::UINT8, 0, 0},
            {"[sb_dom]SCHEMA_TYPE", DataType::UINT8, 0, 0},
            {"[sb_dom]INDEX_TYPE", DataType::UINT8, 0, 0},
            {"[sb_dom]TABLE_TYPE", DataType::UINT8, 0, 0},
            {"[sb_dom]POLICY_TYPE", DataType::UINT8, 0, 0},
            {"[sb_dom]SECURITY_FLAGS", DataType::UINT32, 0, 0},
            {"[sb_dom]PERMISSIONS_MASK", DataType::UINT32, 0, 0}
        };

        // Authoritative fixed registry imported from:
        // local_work/docs/specifications/15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md
        const SystemDomainDef kCanonicalSystemDomains[] = {
#include "scratchbird/core/system_domain_registry_rows.inc"
        };

        const char* const kSystemKeyDomains[] = {
            "[sb_dom]KEY_SCHEMA",
            "[sb_dom]KEY_TABLE",
            "[sb_dom]KEY_COLUMN",
            "[sb_dom]KEY_INDEX",
            "[sb_dom]KEY_CONSTRAINT",
            "[sb_dom]KEY_SEQUENCE",
            "[sb_dom]KEY_VIEW",
            "[sb_dom]KEY_TRIGGER",
            "[sb_dom]KEY_PERMISSION",
            "[sb_dom]KEY_OBJECT",
            "[sb_dom]KEY_STATISTICS",
            "[sb_dom]KEY_STATISTIC",
            "[sb_dom]KEY_COLLATION",
            "[sb_dom]KEY_TIMEZONE",
            "[sb_dom]KEY_CHARSET",
            "[sb_dom]KEY_COLLATION_DEF",
            "[sb_dom]KEY_DEPENDENCY",
            "[sb_dom]KEY_COMMENT",
            "[sb_dom]KEY_OBJECT_DEF",
            "[sb_dom]KEY_ATTACHMENT",
            "[sb_dom]KEY_JOB",
            "[sb_dom]KEY_JOB_RUN",
            "[sb_dom]KEY_JOB_DEPENDENCY",
            "[sb_dom]KEY_JOB_SECRET",
            "[sb_dom]KEY_NODE",
            "[sb_dom]KEY_SHARD",
            "[sb_dom]KEY_USER",
            "[sb_dom]KEY_PRINCIPAL",
            "[sb_dom]KEY_ROLE",
            "[sb_dom]KEY_GROUP",
            "[sb_dom]KEY_ROLE_MEMBER",
            "[sb_dom]KEY_GROUP_MEMBER",
            "[sb_dom]KEY_GROUP_MAPPING",
            "[sb_dom]KEY_MEMBERSHIP",
            "[sb_dom]KEY_MAPPING",
            "[sb_dom]KEY_PROCEDURE",
            "[sb_dom]KEY_PROC_PARAM",
            "[sb_dom]KEY_DOMAIN",
            "[sb_dom]KEY_UDR",
            "[sb_dom]KEY_EXCEPTION",
            "[sb_dom]KEY_PACKAGE",
            "[sb_dom]KEY_EMULATION_TYPE",
            "[sb_dom]KEY_EMULATION_SERVER",
            "[sb_dom]KEY_EMULATED_DB",
            "[sb_dom]KEY_TABLESPACE",
            "[sb_dom]KEY_TABLESPACE_FILE",
            "[sb_dom]KEY_EXTENSION",
            "[sb_dom]KEY_FOREIGN_KEY",
            "[sb_dom]KEY_SYNONYM",
            "[sb_dom]KEY_FOREIGN_SERVER",
            "[sb_dom]KEY_FOREIGN_TABLE",
            "[sb_dom]KEY_USER_MAPPING",
            "[sb_dom]KEY_SERVER",
            "[sb_dom]KEY_SERVER_INSTANCE",
            "[sb_dom]KEY_SERVER_REGISTRY",
            "[sb_dom]KEY_UDR_ENGINE",
            "[sb_dom]KEY_UDR_MODULE",
            "[sb_dom]KEY_MIGRATION",
            "[sb_dom]KEY_MIGRATION_HISTORY",
            "[sb_dom]KEY_DORMANT_TXN",
            "[sb_dom]KEY_PREPARED_TXN",
            "[sb_dom]KEY_TXN",
            "[sb_dom]KEY_STATEMENT",
            "[sb_dom]KEY_LOCK",
            "[sb_dom]KEY_DATABASE",
            "[sb_dom]KEY_RELATION",
            "[sb_dom]KEY_ENCRYPTION_KEY",
            "[sb_dom]KEY_AUTHKEY",
            "[sb_dom]KEY_SESSION",
            "[sb_dom]KEY_AUDIT_LOG",
            "[sb_dom]KEY_SECURITY_POLICY_EPOCH",
            "[sb_dom]KEY_POLICY",
            "[sb_dom]KEY_COLUMN_PERMISSION",
            "[sb_dom]KEY_OBJECT_PERMISSION"
        };

        auto hexNibble(char c) -> int
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f')
            {
                return 10 + (c - 'a');
            }
            if (c >= 'A' && c <= 'F')
            {
                return 10 + (c - 'A');
            }
            return -1;
        }

        auto parseUuidLiteral(const std::string& literal, ID& out) -> bool
        {
            if (literal.size() != 36)
            {
                return false;
            }
            static constexpr std::array<size_t, 4> kDashPos{8, 13, 18, 23};
            for (size_t dash : kDashPos)
            {
                if (literal[dash] != '-')
                {
                    return false;
                }
            }

            size_t out_idx = 0;
            for (size_t i = 0; i < literal.size();)
            {
                if (literal[i] == '-')
                {
                    ++i;
                    continue;
                }
                if (i + 1 >= literal.size() || out_idx >= out.bytes.size())
                {
                    return false;
                }
                const int hi = hexNibble(literal[i]);
                const int lo = hexNibble(literal[i + 1]);
                if (hi < 0 || lo < 0)
                {
                    return false;
                }
                out.bytes[out_idx++] = static_cast<uint8_t>((hi << 4) | lo);
                i += 2;
            }
            return out_idx == out.bytes.size();
        }

        auto systemDomainNamespaceUuid() -> const ID&
        {
            static const ID kNamespace = [] {
                ID id{};
                const bool ok = parseUuidLiteral("e82f373b-99fa-4e85-a135-dfee52b58edb", id);
                if (!ok)
                {
                    return ID{};
                }
                return id;
            }();
            return kNamespace;
        }

        auto uuidV5(const ID& ns_uuid, const std::string& name) -> ID
        {
            unsigned char hash[SHA_DIGEST_LENGTH];
            SHA_CTX sha_ctx;
            SHA1_Init(&sha_ctx);
            SHA1_Update(&sha_ctx, ns_uuid.bytes.data(), ns_uuid.bytes.size());
            SHA1_Update(&sha_ctx, reinterpret_cast<const unsigned char*>(name.data()), name.size());
            SHA1_Final(hash, &sha_ctx);

            ID out{};
            for (size_t i = 0; i < out.bytes.size(); ++i)
            {
                out.bytes[i] = hash[i];
            }

            // RFC 4122 version/variant bits.
            out.bytes[6] = static_cast<uint8_t>((out.bytes[6] & 0x0F) | 0x50);
            out.bytes[8] = static_cast<uint8_t>((out.bytes[8] & 0x3F) | 0x80);
            return out;
        }

        auto systemDomainOriginForName(const std::string& domain_name) -> const char*
        {
            const std::string upper = IdentifierUtils::toUpper(domain_name);
            if (upper.rfind("[SB_PG_DOM]", 0) == 0)
            {
                return "postgresql";
            }
            if (upper.rfind("[SB_MY_DOM]", 0) == 0)
            {
                return "mysql";
            }
            if (upper.rfind("[SB_FB_DOM]", 0) == 0)
            {
                return "firebird";
            }
            if (upper.rfind("[SB_CAS_DOM]", 0) == 0)
            {
                return "cassandra";
            }
            if (upper.rfind("[SB_MIL_DOM]", 0) == 0)
            {
                return "milvus";
            }
            if (upper.rfind("[SB_MONGO_DOM]", 0) == 0)
            {
                return "mongodb";
            }
            if (upper.rfind("[SB_NEO4J_DOM]", 0) == 0)
            {
                return "neo4j";
            }
            if (upper.rfind("[SB_REDIS_DOM]", 0) == 0)
            {
                return "redis";
            }
            return "native";
        }

        auto splitDomainParams(const char* param_string)
            -> std::vector<std::pair<std::string, std::string>>
        {
            std::vector<std::pair<std::string, std::string>> params;
            if (!param_string || param_string[0] == '\0')
            {
                return params;
            }

            std::string input(param_string);
            size_t start = 0;
            while (start < input.size())
            {
                const size_t end = input.find(';', start);
                const std::string token = input.substr(start, end == std::string::npos
                                                                  ? std::string::npos
                                                                  : end - start);
                const size_t eq = token.find('=');
                if (eq != std::string::npos && eq > 0 && eq + 1 < token.size())
                {
                    params.emplace_back(token.substr(0, eq), token.substr(eq + 1));
                }
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return params;
        }

        auto parseUint32Param(const std::string& value, uint32_t& out) -> bool
        {
            if (value.empty())
            {
                return false;
            }
            for (char c : value)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                {
                    return false;
                }
            }
            try
            {
                const unsigned long long parsed = std::stoull(value);
                if (parsed > std::numeric_limits<uint32_t>::max())
                {
                    return false;
                }
                out = static_cast<uint32_t>(parsed);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        auto resolveDomainPrecisionScale(const SystemDomainDef& def,
                                         uint32_t& precision,
                                         uint32_t& scale) -> void
        {
            precision = def.precision;
            scale = def.scale;

            const auto params = splitDomainParams(def.param_string);
            for (const auto& [key, value] : params)
            {
                uint32_t parsed = 0;
                if (key == "length_chars" || key == "length_bytes" ||
                    key == "bit_length" || key == "precision")
                {
                    if (parseUint32Param(value, parsed))
                    {
                        precision = parsed;
                    }
                    continue;
                }
                if (key == "scale" && parseUint32Param(value, parsed))
                {
                    scale = parsed;
                }
            }
        }

        auto canonicalSystemDomainKey(const std::string& domain_name,
                                      DataType base_type,
                                      const char* param_string,
                                      uint32_t precision,
                                      uint32_t scale) -> std::string
        {
            std::ostringstream oss;
            oss << "domain|" << systemDomainOriginForName(domain_name) << "|"
                << domain_name << "|base_type=" << TypeSystem::getTypeName(base_type);

            auto params = splitDomainParams(param_string);
            if (params.empty())
            {
                if (precision != 0)
                {
                    params.emplace_back("precision", std::to_string(precision));
                }
                if (scale != 0)
                {
                    params.emplace_back("scale", std::to_string(scale));
                }
            }

            std::sort(params.begin(), params.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
            for (const auto& [key, value] : params)
            {
                oss << ';' << key << '=' << value;
            }
            return oss.str();
        }

        auto deterministicSystemDomainId(const std::string& domain_name,
                                         DataType base_type,
                                         uint32_t precision,
                                         uint32_t scale) -> ID
        {
            return uuidV5(systemDomainNamespaceUuid(),
                          canonicalSystemDomainKey(domain_name, base_type, "", precision, scale));
        }

        auto deterministicSystemDomainId(const SystemDomainDef& def,
                                         ErrorContext* ctx) -> ID
        {
            if (def.fixed_uuid_literal && def.fixed_uuid_literal[0] != '\0')
            {
                ID parsed{};
                if (!parseUuidLiteral(def.fixed_uuid_literal, parsed))
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid fixed system domain UUID literal");
                    return ID{};
                }
                return parsed;
            }

            uint32_t precision = 0;
            uint32_t scale = 0;
            resolveDomainPrecisionScale(def, precision, scale);
            return uuidV5(systemDomainNamespaceUuid(),
                          canonicalSystemDomainKey(def.name, def.base_type, def.param_string,
                                                   precision, scale));
        }

        std::string defaultDialectTagForCreate() {
            auto* conn_ctx = ConnectionContext::getCurrent();
            if (conn_ctx && !conn_ctx->dialect_tag().empty()) {
                return conn_ctx->dialect_tag();
            }
            return "scratchbird";
        }

        auto normalizeEngineTag(std::string tag) -> std::string
        {
            tag = IdentifierUtils::toUpper(tag);
            if (tag.empty() || tag == "SCRATCHBIRD" || tag == "NATIVE")
            {
                return "NATIVE";
            }
            if (tag == "POSTGRES")
            {
                return "POSTGRESQL";
            }
            if (tag == "MONGO")
            {
                return "MONGODB";
            }
            return tag;
        }

        auto isSystemDomainName(const std::string& domain_name) -> bool
        {
            const std::string upper = IdentifierUtils::toUpper(domain_name);
            return upper.rfind("[SB_", 0) == 0;
        }

        auto validateDomainCreateName(const std::unordered_map<ID, DomainInfo>& domain_cache,
                                      const ID& schema_id,
                                      const std::string& domain_name,
                                      bool allow_system_reserved_name,
                                      ErrorContext* ctx) -> Status
        {
            if (domain_name.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain name cannot be empty");
                return Status::INVALID_ARGUMENT;
            }

            if (!allow_system_reserved_name && isSystemDomainName(domain_name))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Reserved system domain name requires system installation context");
                return Status::INVALID_ARGUMENT;
            }

            for (const auto& [existing_id, existing] : domain_cache)
            {
                (void)existing_id;
                if (existing.schema_id != schema_id)
                {
                    continue;
                }
                if (IdentifierUtils::namesMatch(domain_name, false,
                                                existing.domain_name, false))
                {
                    SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Domain name already exists");
                    return Status::FILE_EXISTS;
                }
            }

            return Status::OK;
        }

        auto resolveDomainCreateId(const std::unordered_map<ID, DomainInfo>& domain_cache,
                                   const std::optional<ID>& fixed_domain_id,
                                   ID& domain_id_out,
                                   ErrorContext* ctx) -> Status
        {
            if (fixed_domain_id.has_value())
            {
                domain_id_out = fixed_domain_id.value();
            }
            else
            {
                domain_id_out = generateUuidV7();
            }

            if (isZeroUuidLocal(domain_id_out))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain ID cannot be zero UUID");
                return Status::INVALID_ARGUMENT;
            }

            if (domain_cache.find(domain_id_out) != domain_cache.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Domain ID already exists");
                return Status::FILE_EXISTS;
            }

            return Status::OK;
        }

        auto systemDomainOriginTag(const std::string& domain_name) -> std::string
        {
            return normalizeEngineTag(systemDomainOriginForName(domain_name));
        }

        bool isDomainTypeCompatible(DataType value_type, DataType domain_type)
        {
            if (value_type == domain_type)
            {
                return true;
            }

            switch (domain_type)
            {
                case DataType::INT32:
                case DataType::INT64:
                    return value_type == DataType::INT32 || value_type == DataType::INT64;
                case DataType::FLOAT32:
                case DataType::FLOAT64:
                case DataType::DECIMAL:
                case DataType::DECFLOAT16:
                case DataType::DECFLOAT34:
                    return value_type == DataType::FLOAT32 || value_type == DataType::FLOAT64 ||
                           value_type == DataType::INT32 || value_type == DataType::INT64 ||
                           value_type == DataType::DECIMAL || value_type == DataType::DECFLOAT16 ||
                           value_type == DataType::DECFLOAT34;
                case DataType::VARCHAR:
                case DataType::TEXT:
                case DataType::CHAR:
                    return value_type == DataType::VARCHAR || value_type == DataType::TEXT ||
                           value_type == DataType::CHAR;
                case DataType::DATE:
                    return value_type == DataType::DATE || value_type == DataType::INT32 ||
                           value_type == DataType::VARCHAR;
                case DataType::TIME:
                    return value_type == DataType::TIME || value_type == DataType::INT32 ||
                           value_type == DataType::VARCHAR;
                case DataType::TIMESTAMP:
                    return value_type == DataType::TIMESTAMP || value_type == DataType::INT64 ||
                           value_type == DataType::VARCHAR;
                case DataType::BLOB:
                case DataType::BINARY:
                case DataType::VARBINARY:
                    return value_type == DataType::BLOB || value_type == DataType::BINARY ||
                           value_type == DataType::VARBINARY || value_type == DataType::VARCHAR;
                default:
                    return false;
            }
        }

        struct SetElementKey
        {
            DataType type{DataType::UNKNOWN};
            std::vector<uint8_t> payload;

            bool operator<(const SetElementKey& other) const
            {
                if (type != other.type)
                {
                    return static_cast<uint32_t>(type) < static_cast<uint32_t>(other.type);
                }
                return payload < other.payload;
            }

            bool operator==(const SetElementKey& other) const
            {
                return type == other.type && payload == other.payload;
            }
        };

        struct NormalizedSet
        {
            DataType element_type{DataType::UNKNOWN};
            std::vector<SetElementKey> keys;
            std::vector<TypedValue> values;
        };

        Status buildSetElementKey(const TypedValue& value, SetElementKey& key, ErrorContext* ctx)
        {
            if (value.isNull())
            {
                SET_ERROR_CONTEXT(ctx, Status::NULL_VALUE_NOT_ALLOWED, "SET elements cannot be NULL");
                return Status::NULL_VALUE_NOT_ALLOWED;
            }

            key.type = value.type();
            std::vector<uint8_t> payload;
            Status status = value.serializePlainValue(payload, ctx);
            if (status != Status::OK)
            {
                if (ctx && ctx->message.empty())
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to serialize SET element");
                }
                return status;
            }
            key.payload = std::move(payload);
            return Status::OK;
        }

        Status normalizeSetValue(const TypedValue& set_value, NormalizedSet& out, ErrorContext* ctx)
        {
            out.keys.clear();
            out.values.clear();
            out.element_type = DataType::UNKNOWN;

            if (set_value.isNull())
            {
                SET_ERROR_CONTEXT(ctx, Status::NULL_VALUE_NOT_ALLOWED, "SET value cannot be NULL");
                return Status::NULL_VALUE_NOT_ALLOWED;
            }

            if (set_value.type() != DataType::ARRAY)
            {
                SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not an ARRAY/SET type");
                return Status::TYPE_MISMATCH;
            }

            const auto& elements = set_value.getArray();
            if (elements.empty())
            {
                return Status::OK;
            }

            std::vector<std::pair<SetElementKey, TypedValue>> items;
            items.reserve(elements.size());

            DataType element_type = DataType::UNKNOWN;
            for (const auto& element : elements)
            {
                if (element.isNull())
                {
                    SET_ERROR_CONTEXT(ctx, Status::NULL_VALUE_NOT_ALLOWED,
                                      "SET elements cannot be NULL");
                    return Status::NULL_VALUE_NOT_ALLOWED;
                }

                if (element_type == DataType::UNKNOWN)
                {
                    element_type = element.type();
                }
                else if (element.type() != element_type)
                {
                    SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                                      "SET elements must share the same type");
                    return Status::TYPE_MISMATCH;
                }

                SetElementKey key;
                Status status = buildSetElementKey(element, key, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                items.emplace_back(std::move(key), element);
            }

            std::sort(items.begin(), items.end(),
                      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

            for (const auto& item : items)
            {
                if (out.keys.empty() || !(item.first == out.keys.back()))
                {
                    out.keys.push_back(item.first);
                    out.values.push_back(item.second);
                }
            }

            out.element_type = element_type;
            return Status::OK;
        }

        constexpr uint8_t DOMAIN_SECURITY_VERSION = 2;
        constexpr uint8_t DOMAIN_INTEGRITY_VERSION = 1;
        constexpr uint8_t DOMAIN_VALIDATION_VERSION = 1;
        constexpr uint8_t DOMAIN_QUALITY_VERSION = 1;
        constexpr uint8_t DOMAIN_CONSTRAINTS_VERSION = 2;
        constexpr uint8_t DOMAIN_FIELDS_VERSION = 2;
        constexpr uint8_t DOMAIN_ENUM_VALUES_VERSION = 1;
        constexpr uint8_t DOMAIN_RANGE_VERSION = 1;
        constexpr uint8_t DOMAIN_BASE_VERSION = 1;

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

        void appendId(std::string& out, const ID& id)
        {
            out.append(reinterpret_cast<const char*>(id.bytes.data()),
                       id.bytes.size());
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

        bool readId(const std::string& blob, size_t& offset, ID& id)
        {
            if (offset + id.bytes.size() > blob.size())
            {
                return false;
            }
            std::memcpy(id.bytes.data(), blob.data() + offset, id.bytes.size());
            offset += id.bytes.size();
            return true;
        }

        void appendTypeRef(std::string& out, const DomainTypeRef& ref)
        {
            if (!isZeroUuidLocal(ref.domain_id))
            {
                appendUint8(out, 1);
                appendId(out, ref.domain_id);
                return;
            }
            appendUint8(out, 0);
            appendUint32(out, static_cast<uint32_t>(ref.type));
            appendUint32(out, ref.precision);
            appendUint32(out, ref.scale);
            appendUint8(out, ref.with_time_zone ? 1 : 0);
        }

        bool readTypeRef(const std::string& blob, size_t& offset, DomainTypeRef& out)
        {
            uint8_t kind = 0;
            if (!readUint8(blob, offset, kind))
            {
                return false;
            }
            if (kind == 1)
            {
                out = DomainTypeRef{};
                return readId(blob, offset, out.domain_id);
            }

            uint32_t type = 0;
            uint32_t precision = 0;
            uint32_t scale = 0;
            uint8_t with_tz = 0;
            if (!readUint32(blob, offset, type) ||
                !readUint32(blob, offset, precision) ||
                !readUint32(blob, offset, scale) ||
                !readUint8(blob, offset, with_tz))
            {
                return false;
            }
            out.type = static_cast<DataType>(type);
            out.precision = precision;
            out.scale = scale;
            out.with_time_zone = (with_tz != 0);
            out.domain_id = ID{};
            return true;
        }

        bool isDefaultSecurity(const DomainSecurity& security)
        {
            return security.masking_config.type == MaskingType::NONE &&
                   security.masking_config.pattern.empty() &&
                   security.masking_config.full_mask_char == "*" &&
                   security.required_privilege_for_unmasked.empty() &&
                   !security.encryption_enabled &&
                   security.encryption_algorithm == EncryptionAlgorithm::NONE &&
                   isZeroUuidLocal(security.encryption_key_id) &&
                   !security.audit_enabled &&
                   security.permission_mask == 0;
        }

        bool isDefaultIntegrity(const DomainIntegrity& integrity)
        {
            return !integrity.uniqueness_check &&
                   !integrity.normalization_enabled &&
                   integrity.normalization_function.empty();
        }

        bool isDefaultValidation(const DomainValidationConfig& validation)
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
            appendUint8(out, static_cast<uint8_t>(security.encryption_algorithm));
            appendUint32(out, security.permission_mask);
            appendString(out, security.masking_config.pattern);
            appendString(out, security.masking_config.full_mask_char);
            appendString(out, security.required_privilege_for_unmasked);
            appendId(out, security.encryption_key_id);
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
            if (!readUint8(blob, offset, version))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain security format");
                return Status::DATA_CORRUPTED;
            }

            uint8_t type = 0;
            uint8_t encryption = 0;
            uint8_t audit = 0;
            uint8_t algorithm = 0;
            uint32_t permission_mask = 0;
            std::string pattern;
            std::string mask_char;
            std::string privilege;
            ID key_id;

            if (version == 1)
            {
                if (!readUint8(blob, offset, type) ||
                    !readUint8(blob, offset, encryption) ||
                    !readUint8(blob, offset, audit) ||
                    !readUint32(blob, offset, permission_mask) ||
                    !readString(blob, offset, pattern) ||
                    !readString(blob, offset, mask_char) ||
                    !readString(blob, offset, privilege))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain security payload");
                    return Status::DATA_CORRUPTED;
                }

                algorithm = static_cast<uint8_t>(EncryptionAlgorithm::NONE);
                key_id = ID{};
            }
            else if (version == DOMAIN_SECURITY_VERSION)
            {
                if (!readUint8(blob, offset, type) ||
                    !readUint8(blob, offset, encryption) ||
                    !readUint8(blob, offset, audit) ||
                    !readUint8(blob, offset, algorithm) ||
                    !readUint32(blob, offset, permission_mask) ||
                    !readString(blob, offset, pattern) ||
                    !readString(blob, offset, mask_char) ||
                    !readString(blob, offset, privilege) ||
                    !readId(blob, offset, key_id))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain security payload");
                    return Status::DATA_CORRUPTED;
                }
            }
            else
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain security format");
                return Status::DATA_CORRUPTED;
            }

            security.masking_config.type = static_cast<MaskingType>(type);
            security.masking_config.pattern = std::move(pattern);
            security.masking_config.full_mask_char = std::move(mask_char);
            security.required_privilege_for_unmasked = std::move(privilege);
            security.encryption_enabled = (encryption != 0);
            security.audit_enabled = (audit != 0);
            security.permission_mask = permission_mask;
            security.encryption_algorithm = static_cast<EncryptionAlgorithm>(algorithm);
            security.encryption_key_id = key_id;

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
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain integrity payload");
                return Status::DATA_CORRUPTED;
            }

            integrity.uniqueness_check = (uniqueness != 0);
            integrity.normalization_enabled = (normalization != 0);
            integrity.normalization_function = std::move(function);

            return Status::OK;
        }

        std::string serializeDomainValidation(const DomainValidationConfig& validation)
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
                                           DomainValidationConfig& validation,
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
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain validation payload");
                return Status::DATA_CORRUPTED;
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
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain quality payload");
                return Status::DATA_CORRUPTED;
            }

            quality.parse_function = std::move(parse_function);
            quality.standardize_function = std::move(standardize_function);
            quality.enrich_function = std::move(enrich_function);

            return Status::OK;
        }

        std::string serializeDomainConstraints(const DomainInfo& domain)
        {
            bool has_set_element = domain.set_element_type.type != DataType::UNKNOWN ||
                                   !isZeroUuidLocal(domain.set_element_type.domain_id);
            if (domain.constraints.empty() &&
                domain.variant_allowed_types.empty() &&
                domain.collation_name.empty() &&
                !domain.enum_wrap &&
                !has_set_element)
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_CONSTRAINTS_VERSION);
            appendUint32(out, static_cast<uint32_t>(domain.constraints.size()));
            for (const auto& constraint : domain.constraints)
            {
                appendUint8(out, static_cast<uint8_t>(constraint.type));
                appendString(out, constraint.expression);
                appendString(out, constraint.name);
            }

            appendString(out, domain.collation_name);
            appendUint8(out, domain.enum_wrap ? 1 : 0);

            appendUint8(out, has_set_element ? 1 : 0);
            if (has_set_element)
            {
                appendTypeRef(out, domain.set_element_type);
            }

            appendUint32(out, static_cast<uint32_t>(domain.variant_allowed_types.size()));
            for (const auto& type : domain.variant_allowed_types)
            {
                appendTypeRef(out, type);
            }
            return out;
        }

        Status deserializeDomainConstraints(const std::string& blob,
                                            std::vector<DomainConstraint>& constraints_out,
                                            std::vector<DomainTypeRef>& variant_types_out,
                                            std::string& collation_out,
                                            bool& enum_wrap_out,
                                            DomainTypeRef& set_element_out,
                                            ErrorContext* ctx)
        {
            constraints_out.clear();
            variant_types_out.clear();
            collation_out.clear();
            enum_wrap_out = false;
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            if (!readUint8(blob, offset, version))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }

            if (version != DOMAIN_CONSTRAINTS_VERSION && version != 1)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }

            uint32_t count = 0;
            if (!readUint32(blob, offset, count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }

            constraints_out.reserve(count);
            for (uint32_t i = 0; i < count; i++)
            {
                uint8_t type = 0;
                std::string expression;
                std::string name;
                if (!readUint8(blob, offset, type) ||
                    !readString(blob, offset, expression) ||
                    !readString(blob, offset, name))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                    return Status::DATA_CORRUPTED;
                }
                constraints_out.emplace_back(static_cast<ConstraintType>(type), expression, name);
            }

            if (version == 1)
            {
                if (offset >= blob.size())
                {
                    return Status::OK;
                }

                uint32_t type_count = 0;
                if (!readUint32(blob, offset, type_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                    return Status::DATA_CORRUPTED;
                }

                variant_types_out.reserve(type_count);
                for (uint32_t i = 0; i < type_count; i++)
                {
                    uint32_t type = 0;
                    if (!readUint32(blob, offset, type))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                        return Status::DATA_CORRUPTED;
                    }
                    DomainTypeRef ref;
                    ref.type = static_cast<DataType>(type);
                    variant_types_out.push_back(std::move(ref));
                }

                return Status::OK;
            }

            if (!readString(blob, offset, collation_out))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }

            uint8_t enum_wrap = 0;
            if (!readUint8(blob, offset, enum_wrap))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }
            enum_wrap_out = enum_wrap != 0;

            uint8_t has_set = 0;
            if (!readUint8(blob, offset, has_set))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }
            if (has_set)
            {
                if (!readTypeRef(blob, offset, set_element_out))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                    return Status::DATA_CORRUPTED;
                }
            }

            uint32_t type_count = 0;
            if (!readUint32(blob, offset, type_count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                return Status::DATA_CORRUPTED;
            }

            variant_types_out.reserve(type_count);
            for (uint32_t i = 0; i < type_count; i++)
            {
                DomainTypeRef ref;
                if (!readTypeRef(blob, offset, ref))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain constraints payload");
                    return Status::DATA_CORRUPTED;
                }
                variant_types_out.push_back(std::move(ref));
            }

            return Status::OK;
        }

        std::string serializeDomainFields(const std::vector<RecordField>& fields)
        {
            if (fields.empty())
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_FIELDS_VERSION);
            appendUint32(out, static_cast<uint32_t>(fields.size()));
            for (const auto& field : fields)
            {
                appendString(out, field.name);
                appendUint32(out, static_cast<uint32_t>(field.type));
                appendUint32(out, field.precision);
                appendUint32(out, field.scale);
                appendUint8(out, field.nullable ? 1 : 0);
                appendUint8(out, field.has_default ? 1 : 0);
                appendString(out, field.default_value);
                appendId(out, field.domain_id);
            }
            return out;
        }

        Status deserializeDomainFields(const std::string& blob,
                                       std::vector<RecordField>& fields_out,
                                       ErrorContext* ctx)
        {
            fields_out.clear();
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            if (!readUint8(blob, offset, version) ||
                (version != DOMAIN_FIELDS_VERSION && version != 1))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain fields payload");
                return Status::DATA_CORRUPTED;
            }

            uint32_t count = 0;
            if (!readUint32(blob, offset, count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain fields payload");
                return Status::DATA_CORRUPTED;
            }

            fields_out.reserve(count);
            for (uint32_t i = 0; i < count; i++)
            {
                std::string name;
                uint32_t type = 0;
                uint32_t precision = 0;
                uint32_t scale = 0;
                uint8_t nullable = 0;
                ID domain_id;
                if (!readString(blob, offset, name) ||
                    !readUint32(blob, offset, type) ||
                    !readUint32(blob, offset, precision) ||
                    !readUint32(blob, offset, scale) ||
                    !readUint8(blob, offset, nullable))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain fields payload");
                    return Status::DATA_CORRUPTED;
                }
                bool has_default = false;
                std::string default_value;
                if (version == 1)
                {
                    if (!readId(blob, offset, domain_id))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain fields payload");
                        return Status::DATA_CORRUPTED;
                    }
                }
                else
                {
                    uint8_t has_default_flag = 0;
                    if (!readUint8(blob, offset, has_default_flag) ||
                        !readString(blob, offset, default_value) ||
                        !readId(blob, offset, domain_id))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain fields payload");
                        return Status::DATA_CORRUPTED;
                    }
                    has_default = has_default_flag != 0;
                }
                RecordField field;
                field.name = std::move(name);
                field.type = static_cast<DataType>(type);
                field.precision = precision;
                field.scale = scale;
                field.nullable = nullable != 0;
                field.has_default = has_default;
                field.default_value = std::move(default_value);
                field.domain_id = domain_id;
                fields_out.push_back(std::move(field));
            }

            return Status::OK;
        }

        std::string serializeDomainRangeInfo(const RangeTypeInfo& info)
        {
            std::string out;
            appendUint8(out, DOMAIN_RANGE_VERSION);
            appendUint8(out, 1);
            appendTypeRef(out, info.subtype);

            appendUint8(out, info.subtype_collation.empty() ? 0 : 1);
            if (!info.subtype_collation.empty())
            {
                appendString(out, info.subtype_collation);
            }

            appendUint8(out, info.subtype_opclass.empty() ? 0 : 1);
            if (!info.subtype_opclass.empty())
            {
                appendString(out, info.subtype_opclass);
            }

            appendUint8(out, info.canonical_function.empty() ? 0 : 1);
            if (!info.canonical_function.empty())
            {
                appendString(out, info.canonical_function);
            }

            appendUint8(out, info.subtype_diff_function.empty() ? 0 : 1);
            if (!info.subtype_diff_function.empty())
            {
                appendString(out, info.subtype_diff_function);
            }

            appendUint8(out, info.multirange ? 1 : 0);
            return out;
        }

        Status deserializeDomainRangeInfo(const std::string& blob,
                                          RangeTypeInfo& info_out,
                                          ErrorContext* ctx)
        {
            info_out = RangeTypeInfo{};
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            if (!readUint8(blob, offset, version) || version != DOMAIN_RANGE_VERSION)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain range payload");
                return Status::DATA_CORRUPTED;
            }

            uint8_t has_subtype = 0;
            if (!readUint8(blob, offset, has_subtype))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain range payload");
                return Status::DATA_CORRUPTED;
            }
            if (has_subtype)
            {
                if (!readTypeRef(blob, offset, info_out.subtype))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain range payload");
                    return Status::DATA_CORRUPTED;
                }
            }

            auto read_optional_string = [&](std::string& out) -> Status {
                uint8_t has_value = 0;
                if (!readUint8(blob, offset, has_value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain range payload");
                    return Status::DATA_CORRUPTED;
                }
                if (has_value)
                {
                    if (!readString(blob, offset, out))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain range payload");
                        return Status::DATA_CORRUPTED;
                    }
                }
                return Status::OK;
            };

            Status status = read_optional_string(info_out.subtype_collation);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.subtype_opclass);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.canonical_function);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.subtype_diff_function);
            if (status != Status::OK) return status;

            uint8_t multirange = 0;
            if (!readUint8(blob, offset, multirange))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain range payload");
                return Status::DATA_CORRUPTED;
            }
            info_out.multirange = multirange != 0;

            return Status::OK;
        }

        std::string serializeDomainBaseInfo(const BaseTypeInfo& info)
        {
            std::string out;
            appendUint8(out, DOMAIN_BASE_VERSION);
            appendUint8(out, 1);
            appendTypeRef(out, info.storage);

            appendString(out, info.input_function);
            appendString(out, info.output_function);

            appendUint8(out, info.receive_function.empty() ? 0 : 1);
            if (!info.receive_function.empty())
            {
                appendString(out, info.receive_function);
            }

            appendUint8(out, info.send_function.empty() ? 0 : 1);
            if (!info.send_function.empty())
            {
                appendString(out, info.send_function);
            }

            appendUint8(out, info.typmod_in_function.empty() ? 0 : 1);
            if (!info.typmod_in_function.empty())
            {
                appendString(out, info.typmod_in_function);
            }

            appendUint8(out, info.typmod_out_function.empty() ? 0 : 1);
            if (!info.typmod_out_function.empty())
            {
                appendString(out, info.typmod_out_function);
            }

            appendUint8(out, info.analyze_function.empty() ? 0 : 1);
            if (!info.analyze_function.empty())
            {
                appendString(out, info.analyze_function);
            }

            appendUint8(out, info.alignment.empty() ? 0 : 1);
            if (!info.alignment.empty())
            {
                appendString(out, info.alignment);
            }

            appendUint8(out, info.storage_mode.empty() ? 0 : 1);
            if (!info.storage_mode.empty())
            {
                appendString(out, info.storage_mode);
            }

            appendUint8(out, info.category == '\0' ? 0 : 1);
            if (info.category != '\0')
            {
                appendUint8(out, static_cast<uint8_t>(info.category));
            }

            appendUint8(out, info.has_preferred ? 1 : 0);
            if (info.has_preferred)
            {
                appendUint8(out, info.preferred ? 1 : 0);
            }

            return out;
        }

        Status deserializeDomainBaseInfo(const std::string& blob,
                                         BaseTypeInfo& info_out,
                                         ErrorContext* ctx)
        {
            info_out = BaseTypeInfo{};
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            if (!readUint8(blob, offset, version) || version != DOMAIN_BASE_VERSION)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                return Status::DATA_CORRUPTED;
            }

            uint8_t has_storage = 0;
            if (!readUint8(blob, offset, has_storage))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                return Status::DATA_CORRUPTED;
            }
            if (has_storage)
            {
                if (!readTypeRef(blob, offset, info_out.storage))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                    return Status::DATA_CORRUPTED;
                }
            }

            if (!readString(blob, offset, info_out.input_function) ||
                !readString(blob, offset, info_out.output_function))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                return Status::DATA_CORRUPTED;
            }

            auto read_optional_string = [&](std::string& out) -> Status {
                uint8_t has_value = 0;
                if (!readUint8(blob, offset, has_value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                    return Status::DATA_CORRUPTED;
                }
                if (has_value)
                {
                    if (!readString(blob, offset, out))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                        return Status::DATA_CORRUPTED;
                    }
                }
                return Status::OK;
            };

            Status status = read_optional_string(info_out.receive_function);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.send_function);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.typmod_in_function);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.typmod_out_function);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.analyze_function);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.alignment);
            if (status != Status::OK) return status;
            status = read_optional_string(info_out.storage_mode);
            if (status != Status::OK) return status;

            uint8_t has_category = 0;
            if (!readUint8(blob, offset, has_category))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                return Status::DATA_CORRUPTED;
            }
            if (has_category)
            {
                uint8_t category = 0;
                if (!readUint8(blob, offset, category))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                    return Status::DATA_CORRUPTED;
                }
                info_out.category = static_cast<char>(category);
            }

            uint8_t has_preferred = 0;
            if (!readUint8(blob, offset, has_preferred))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                return Status::DATA_CORRUPTED;
            }
            if (has_preferred)
            {
                uint8_t preferred = 0;
                if (!readUint8(blob, offset, preferred))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain base payload");
                    return Status::DATA_CORRUPTED;
                }
                info_out.preferred = preferred != 0;
                info_out.has_preferred = true;
            }

            return Status::OK;
        }

        std::string serializeDomainEnumValues(const std::vector<EnumValue>& values)
        {
            if (values.empty())
            {
                return {};
            }

            std::string out;
            appendUint8(out, DOMAIN_ENUM_VALUES_VERSION);
            appendUint32(out, static_cast<uint32_t>(values.size()));
            for (const auto& value : values)
            {
                appendString(out, value.label);
                appendUint32(out, static_cast<uint32_t>(value.position));
            }
            return out;
        }

        Status deserializeDomainEnumValues(const std::string& blob,
                                           std::vector<EnumValue>& values_out,
                                           ErrorContext* ctx)
        {
            values_out.clear();
            if (blob.empty())
            {
                return Status::OK;
            }

            size_t offset = 0;
            uint8_t version = 0;
            if (!readUint8(blob, offset, version) || version != DOMAIN_ENUM_VALUES_VERSION)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain enum values payload");
                return Status::DATA_CORRUPTED;
            }

            uint32_t count = 0;
            if (!readUint32(blob, offset, count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain enum values payload");
                return Status::DATA_CORRUPTED;
            }

            values_out.reserve(count);
            for (uint32_t i = 0; i < count; i++)
            {
                std::string label;
                uint32_t position = 0;
                if (!readString(blob, offset, label) ||
                    !readUint32(blob, offset, position))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid domain enum values payload");
                    return Status::DATA_CORRUPTED;
                }
                values_out.emplace_back(label, static_cast<int32_t>(position));
            }

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
        ID constraints_oid;   // TOAST reference for constraints
        ID fields_oid;        // TOAST reference for RECORD fields
        ID enum_values_oid;   // TOAST reference for ENUM values
        ID security_oid;      // TOAST reference for security config
        ID integrity_oid;     // TOAST reference for integrity config
        ID validation_oid;    // TOAST reference for validation config
        ID quality_oid;       // TOAST reference for quality config
        uint16_t set_element_type;  // For SET domains
        char dialect_tag[32];        // Cross-dialect compatibility tag
        char compat_name[128];       // Dialect-specific type name
        uint16_t reserved;

        DomainRecord() : domain_type(0), base_type(0), precision(0), scale(0),
                        nullable(1), is_valid(1), created_time(0), last_modified_time(0),
                        constraints_oid(ID{}), fields_oid(ID{}), enum_values_oid(ID{}),
                        security_oid(ID{}), integrity_oid(ID{}), validation_oid(ID{}), quality_oid(ID{}),
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
        uint32_t next_page;
        uint32_t reserved;
        uint8_t data[];
    };

    DomainManager::DomainManager(Database* db)
        : db_(db),
          domain_count_(0),
          uniqueness_index_(std::make_unique<GlobalUniquenessIndex>(db))
    {
    }

    DomainManager::~DomainManager() = default;

    auto DomainManager::initialize(ErrorContext* ctx) -> Status
    {
        std::unique_lock<std::mutex> lock(mutex_);

        LOG_INFO(CATALOG, "Initializing domain manager");

        CatalogManager* catalog = db_ ? db_->catalog_manager() : nullptr;
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        Status status = catalog->ensureDomainsTablePage(ctx);
        if (status != Status::OK)
        {
            return status;
        }
        domains_table_page_ = catalog->domainsTablePage();
        if (domains_table_page_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Domains table page not initialized");
            return Status::PAGE_CORRUPT;
        }

        // Initialize the domains catalog page if needed.
        BufferPool* bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }
        void* page_buffer = nullptr;
        status = bp->pinPage(domains_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
            return status;
        }

        auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);
        bool needs_init = catalog_page->header.magic != K_MAGIC_SBRD ||
                          catalog_page->header.page_type != PAGE_TYPE_HEAP ||
                          catalog_page->header.page_id != domains_table_page_;

        if (needs_init)
        {
            std::memset(page_buffer, 0, db_->page_size());
            catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);

            catalog_page->header.magic = K_MAGIC_SBRD;
            catalog_page->header.version = 1;
            catalog_page->header.page_type = PAGE_TYPE_HEAP;
            catalog_page->header.page_size = db_->page_size();
            catalog_page->header.page_id = domains_table_page_;
            catalog_page->header.generation = 1;
            catalog_page->header.checksum = 0;
            catalog_page->header.flags = 0;
            catalog_page->header.lsn = 0;
            pageSetLower(catalog_page->header, sizeof(DomainCatalogPage));
            pageSetUpper(catalog_page->header, db_->page_size());
            pageSetSpecial(catalog_page->header, db_->page_size());

            catalog_page->record_count = 0;
            catalog_page->free_offset = sizeof(DomainCatalogPage);
            catalog_page->next_page = 0;
            catalog_page->reserved = 0;
        }

        status = bp->unpinPage(domains_table_page_, needs_init, ctx);
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

        if (uniqueness_index_)
        {
            for (const auto& [id, info] : domain_cache_)
            {
                if (!info.enforce_global_uniqueness && !info.integrity.uniqueness_check)
                {
                    continue;
                }
                status = uniqueness_index_->enableUniqueness(id, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to enable domain uniqueness");
                    return status;
                }
            }
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
                                         const DomainCreateOptions& options,
                                         ID& domain_id,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::BASIC;
        info.base_type = base_type;
        info.precision = precision;
        info.scale = scale;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        // Write to catalog
        status = writeDomainRecord(info, ctx);
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
        DomainCreateOptions options;
        options.nullable = nullable;
        options.default_value = default_value;
        options.constraints = constraints;
        return createBasicDomain(schema_id, domain_name, base_type, precision, scale,
                                 options, domain_id, ctx);
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

    auto DomainManager::listDomainsVisible(const ID& schema_id,
                                           const DomainListOptions& options,
                                           std::vector<DomainInfo>& domains,
                                           ErrorContext* ctx) -> Status
    {
        std::unordered_set<std::string> enabled_origins;
        enabled_origins.insert("NATIVE");

        if (options.enforce_emulation_profiles)
        {
            CatalogManager* catalog = db_ ? db_->catalog_manager() : nullptr;
            if (catalog)
            {
                std::vector<CatalogManager::EmulationTypeInfo> emulation_types;
                Status status = catalog->listEmulationTypes(emulation_types, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    return status;
                }
                if (status == Status::OK)
                {
                    for (const auto& emulation : emulation_types)
                    {
                        enabled_origins.insert(normalizeEngineTag(emulation.emulation_name));
                    }
                }
            }
        }

        std::string active_dialect = options.dialect_tag;
        if (active_dialect.empty())
        {
            if (const auto* conn_ctx = ConnectionContext::getCurrent())
            {
                active_dialect = conn_ctx->dialect_tag();
            }
        }
        active_dialect = normalizeEngineTag(active_dialect);

        std::lock_guard<std::mutex> lock(mutex_);

        domains.clear();
        for (const auto& [id, info] : domain_cache_)
        {
            (void)id;
            if (!isZeroUuidLocal(schema_id) && info.schema_id != schema_id)
            {
                continue;
            }

            const bool system_domain = isSystemDomainName(info.domain_name);
            if (!system_domain)
            {
                domains.push_back(info);
                continue;
            }

            if (!options.include_system)
            {
                continue;
            }

            const std::string origin = systemDomainOriginTag(info.domain_name);
            if (options.enforce_emulation_profiles &&
                origin != "NATIVE" &&
                enabled_origins.find(origin) == enabled_origins.end())
            {
                continue;
            }

            if (active_dialect != "NATIVE" &&
                origin != "NATIVE" &&
                origin != active_dialect)
            {
                continue;
            }

            domains.push_back(info);
        }

        return Status::OK;
    }

    auto DomainManager::ensureSystemDomains(ErrorContext* ctx) -> Status
    {
        CatalogManager* catalog = db_ ? db_->catalog_manager() : nullptr;
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        ID sys_schema_id{};
        for (const auto& schema : schemas)
        {
            if (IdentifierUtils::namesMatch("sys", false,
                                            schema.schema_name, schema.name_is_delimited))
            {
                sys_schema_id = schema.schema_id;
                break;
            }
        }

        if (isZeroUuidLocal(sys_schema_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "System schema not found");
            return Status::NOT_FOUND;
        }

        auto ensure_domain = [&](const SystemDomainDef& def,
                                 bool strict_uuid_match) -> Status {
            ErrorContext local_ctx;
            const ID expected_id = deterministicSystemDomainId(def, &local_ctx);
            if (isZeroUuidLocal(expected_id))
            {
                if (ctx && local_ctx.code != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, local_ctx.code, local_ctx.message.c_str());
                }
                return local_ctx.code == Status::OK ? Status::INVALID_ARGUMENT : local_ctx.code;
            }

            uint32_t precision = 0;
            uint32_t scale = 0;
            resolveDomainPrecisionScale(def, precision, scale);
            DomainInfo existing;
            if (getDomain(sys_schema_id, def.name, existing, nullptr) == Status::OK)
            {
                if (existing.domain_id != expected_id)
                {
                    if (strict_uuid_match)
                    {
                        std::ostringstream msg;
                        msg << "Canonical system domain UUID mismatch for " << def.name
                            << ": found " << existing.domain_id.toString()
                            << ", expected " << expected_id.toString();
                        const std::string msg_str = msg.str();
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, msg_str.c_str());
                        return Status::DATA_CORRUPTED;
                    }
                    LOG_WARNING(CATALOG,
                                "Legacy system domain '%s' has non-canonical UUID %s (expected %s)",
                                def.name,
                                existing.domain_id.toString().c_str(),
                                expected_id.toString().c_str());
                }
                return Status::OK;
            }
            DomainCreateOptions options;
            options.nullable = true;
            options.dialect_tag = systemDomainOriginForName(def.name);
            options.fixed_domain_id = expected_id;
            options.allow_system_reserved_name = true;
            ID domain_id;
            return createBasicDomain(sys_schema_id, def.name,
                                     def.base_type, precision, scale,
                                     options, domain_id, ctx);
        };

        for (const auto& def : kCanonicalSystemDomains)
        {
            status = ensure_domain(def, true);
            if (status != Status::OK)
            {
                return status;
            }
        }

        for (const auto& def : kLegacySystemDomains)
        {
            status = ensure_domain(def, false);
            if (status != Status::OK)
            {
                return status;
            }
        }

        DomainInfo uuid_domain;
        status = getDomain(sys_schema_id, "[sb_dom]uuid_v7_internal", uuid_domain, nullptr);
        if (status != Status::OK)
        {
            status = getDomain(sys_schema_id, "[sb_dom]UUID_V7", uuid_domain, ctx);
        }
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status,
                              "Canonical UUID root system domain not found during bootstrap");
            return status;
        }

        for (const char* name : kSystemKeyDomains)
        {
            const ID expected_id =
                deterministicSystemDomainId(name, DataType::UUID, 0, 0);
            DomainInfo existing;
            if (getDomain(sys_schema_id, name, existing, nullptr) == Status::OK)
            {
                if (existing.domain_id != expected_id)
                {
                    LOG_WARNING(CATALOG,
                                "System key domain '%s' has non-canonical UUID %s (expected %s); "
                                "keeping existing ID for compatibility",
                                name,
                                existing.domain_id.toString().c_str(),
                                expected_id.toString().c_str());
                }
                if (isZeroUuidLocal(existing.parent_domain_id))
                {
                    auto parent_status = setParentDomain(existing.domain_id,
                                                         uuid_domain.domain_id, ctx);
                    if (parent_status != Status::OK)
                    {
                        return parent_status;
                    }
                }
                continue;
            }

            DomainCreateOptions options;
            options.nullable = true;
            options.dialect_tag = "scratchbird";
            options.fixed_domain_id = expected_id;
            options.allow_system_reserved_name = true;
            ID domain_id;
            status = createBasicDomain(sys_schema_id, name,
                                       DataType::UUID, 0, 0,
                                       options, domain_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            status = setParentDomain(domain_id, uuid_domain.domain_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        return Status::OK;
    }

    auto DomainManager::dropDomain(const ID& domain_id,
                                  ErrorContext* ctx) -> Status
    {
        std::unique_lock<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        // Check for dependencies - domain cannot be dropped if used by columns
        // We need to unlock mutex before calling catalog_manager to avoid deadlock
        lock.unlock();
        std::vector<std::pair<ID, std::string>> dependent_columns;
        Status status = db_->catalog_manager()->findColumnsByDomain(domain_id, dependent_columns, ctx);
        lock.lock();

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

        if (isSystemDomainName(new_name))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Reserved system domain names are not valid rename targets");
            return Status::INVALID_ARGUMENT;
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

    auto DomainManager::setDefaultValue(const ID& domain_id,
                                        const std::string& default_value,
                                        ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        it->second.default_value = default_value;
        it->second.last_modified_time = std::time(nullptr);

        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Updated default value for domain %s",
                 domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::addCheckConstraint(const ID& domain_id,
                                           const std::string& name,
                                           const std::string& expression,
                                           ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (expression.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "CHECK constraint expression cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        if (!name.empty())
        {
            for (const auto& existing : it->second.constraints)
            {
                if (!existing.name.empty() &&
                    IdentifierUtils::namesMatch(existing.name, false, name, false))
                {
                    SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                                      "Constraint name already exists");
                    return Status::FILE_EXISTS;
                }
            }
        }

        it->second.constraints.emplace_back(ConstraintType::CHECK, expression, name);
        it->second.last_modified_time = std::time(nullptr);

        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Added CHECK constraint for domain %s",
                 domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::dropConstraint(const ID& domain_id,
                                       const std::string& name,
                                       ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (name.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Constraint name is required");
            return Status::INVALID_ARGUMENT;
        }

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto& constraints = it->second.constraints;
        auto match = std::find_if(constraints.begin(), constraints.end(),
                                  [&](const DomainConstraint& constraint) {
                                      return !constraint.name.empty() &&
                                             IdentifierUtils::namesMatch(constraint.name, false, name, false);
                                  });
        if (match == constraints.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Constraint not found");
            return Status::NOT_FOUND;
        }

        constraints.erase(match);
        it->second.last_modified_time = std::time(nullptr);

        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Dropped constraint for domain %s",
                 domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::setCompatName(const ID& domain_id,
                                      const std::string& compat_name,
                                      ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        it->second.compat_name = compat_name;
        it->second.last_modified_time = std::time(nullptr);

        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        LOG_INFO(CATALOG, "Updated compat name for domain %s",
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

        std::vector<DomainConstraint> inherited;
        bool has_inherited = false;
        if (domain.parent_domain_id != ID{})
        {
            Status status = resolveInheritedConstraints(domain_id, inherited, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            has_inherited = true;
        }

        auto validate_scalar = [&](const TypedValue& scalar) -> Status
        {
            // Check NULL constraint
            if (!domain.nullable && scalar.isNull())
            {
                SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, "NULL value not allowed");
                return Status::CONSTRAINT_VIOLATION;
            }

            // Check type compatibility
            if (!scalar.isNull() && !isDomainTypeCompatible(scalar.type(), domain.base_type))
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
                        status = validateNotNullConstraint(scalar, ctx);
                        break;

                    case ConstraintType::CHECK:
                        status = validateCheckConstraint(domain, scalar, constraint, ctx);
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
            if (has_inherited)
            {
                for (const auto& constraint : inherited)
                {
                    if (constraint.type == ConstraintType::CHECK)
                    {
                        Status status = validateCheckConstraint(domain, scalar, constraint, ctx);
                        if (status != Status::OK)
                        {
                            return status;
                        }
                    }
                }
            }

            return Status::OK;
        };

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                return validate_scalar(value);
            }

            for (const auto& element : value.getArray())
            {
                Status status = validate_scalar(element);
                if (status != Status::OK)
                {
                    return status;
                }
            }
            return Status::OK;
        }

        return validate_scalar(value);
    }

    auto DomainManager::validateCollectionMutation(const ID& domain_id,
                                                   CollectionMutationKind mutation,
                                                   ErrorContext* ctx) -> Status
    {
        if (domain_id == ID{})
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return DomainValidation::validateCollectionMutation(domain, mutation, ctx);
    }

    auto DomainManager::applyNormalization(const ID& domain_id,
                                           TypedValue& value,
                                           FunctionInvoker* invoker,
                                           ErrorContext* ctx) -> Status
    {
        if (domain_id == ID{})
        {
            return Status::OK;
        }

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                return Status::OK;
            }

            std::vector<TypedValue> elements = value.getArray();
            for (auto& element : elements)
            {
                Status status = applyNormalization(domain_id, element, invoker, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            value = TypedValue::makeArray(elements);
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!domain.integrity.normalization_enabled ||
            domain.integrity.normalization_function.empty())
        {
            return Status::OK;
        }

        NormalizationConfig config =
            Normalization::resolveConfig(domain.integrity.normalization_function);
        TypedValue normalized;
        status = Normalization::applyNormalization(value, config, invoker, normalized, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Domain normalization failed");
            }
            return status;
        }

        value = normalized;
        return Status::OK;
    }

    auto DomainManager::validateValue(const ID& domain_id,
                                      const TypedValue& value,
                                      FunctionInvoker* invoker,
                                      bool& is_valid_out,
                                      ErrorContext* ctx) -> Status
    {
        is_valid_out = true;
        if (domain_id == ID{})
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (domain.validation.validation_function.empty())
        {
            return Status::OK;
        }

        ValidationConfig config;
        config.function_name = domain.validation.validation_function;
        config.error_message = domain.validation.error_message;

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                is_valid_out = true;
                return Status::OK;
            }

            bool all_valid = true;
            for (const auto& element : value.getArray())
            {
                bool element_valid = true;
                status = DomainValidation::validateValue(element, config, invoker,
                                                        element_valid, ctx);
                if (status != Status::OK)
                {
                    if (ctx && ctx->message.empty())
                    {
                        SET_ERROR_CONTEXT(ctx, status, "Domain validation failed");
                    }
                    return status;
                }
                if (!element_valid)
                {
                    all_valid = false;
                    break;
                }
            }

            is_valid_out = all_valid;
            return Status::OK;
        }

        status = DomainValidation::validateValue(value, config, invoker, is_valid_out, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Domain validation failed");
            }
            return status;
        }

        return Status::OK;
    }

    auto DomainManager::executeQualityPipeline(const ID& domain_id,
                                               TypedValue& value,
                                               FunctionInvoker* invoker,
                                               QualityResult& result_out,
                                               ErrorContext* ctx) -> Status
    {
        if (domain_id == ID{})
        {
            result_out.parsed_value = value;
            result_out.standardized_value = value;
            result_out.enriched_value = value;
            result_out.metadata.clear();
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (domain.quality.parse_function.empty() &&
            domain.quality.standardize_function.empty() &&
            domain.quality.enrich_function.empty())
        {
            result_out.parsed_value = value;
            result_out.standardized_value = value;
            result_out.enriched_value = value;
            result_out.metadata.clear();
            return Status::OK;
        }

        QualityConfig config;
        config.parse_function = domain.quality.parse_function;
        config.standardize_function = domain.quality.standardize_function;
        config.enrich_function = domain.quality.enrich_function;

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                result_out.parsed_value = value;
                result_out.standardized_value = value;
                result_out.enriched_value = value;
                result_out.metadata.clear();
                return Status::OK;
            }

            std::vector<TypedValue> elements = value.getArray();
            for (auto& element : elements)
            {
                QualityResult element_result;
                status = QualityPipeline::executePipeline(element, config, invoker, element_result, ctx);
                if (status != Status::OK)
                {
                    if (ctx && ctx->message.empty())
                    {
                        SET_ERROR_CONTEXT(ctx, status, "Domain quality pipeline failed");
                    }
                    return status;
                }
                element = element_result.enriched_value;
            }

            value = TypedValue::makeArray(elements);
            result_out.parsed_value = value;
            result_out.standardized_value = value;
            result_out.enriched_value = value;
            result_out.metadata.clear();
            return Status::OK;
        }

        status = QualityPipeline::executePipeline(value, config, invoker, result_out, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Domain quality pipeline failed");
            }
            return status;
        }

        value = result_out.enriched_value;
        return Status::OK;
    }

    auto DomainManager::checkGlobalUniqueness(const ID& domain_id,
                                              const TypedValue& value,
                                              uint64_t tx_id,
                                              bool& is_unique_out,
                                              ErrorContext* ctx) -> Status
    {
        is_unique_out = true;

        if (domain_id == ID{})
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!domain.enforce_global_uniqueness && !domain.integrity.uniqueness_check)
        {
            return Status::OK;
        }

        if (!uniqueness_index_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                              "Global uniqueness index unavailable");
            return Status::INTERNAL_ERROR;
        }

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                return Status::OK;
            }

            for (const auto& element : value.getArray())
            {
                if (element.isNull())
                {
                    continue;
                }

                bool element_unique = true;
                status = uniqueness_index_->checkUniqueness(domain_id, element, tx_id,
                                                            element_unique, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                if (!element_unique)
                {
                    is_unique_out = false;
                    if (ctx && ctx->message.empty())
                    {
                        ctx->message = "Domain uniqueness violation";
                    }
                    return Status::OK;
                }
            }

            return Status::OK;
        }

        status = uniqueness_index_->checkUniqueness(domain_id, value, tx_id, is_unique_out, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!is_unique_out && ctx && ctx->message.empty())
        {
            ctx->message = "Domain uniqueness violation";
        }

        return Status::OK;
    }

    auto DomainManager::registerUniqueValue(const ID& domain_id,
                                            const ID& table_id,
                                            const ID& column_id,
                                            const TID& row_id,
                                            const TypedValue& value,
                                            uint64_t tx_id,
                                            ErrorContext* ctx) -> Status
    {
        if (domain_id == ID{})
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!domain.enforce_global_uniqueness && !domain.integrity.uniqueness_check)
        {
            return Status::OK;
        }

        if (!uniqueness_index_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                              "Global uniqueness index unavailable");
            return Status::INTERNAL_ERROR;
        }

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                return Status::OK;
            }

            for (const auto& element : value.getArray())
            {
                if (element.isNull())
                {
                    continue;
                }

                status = uniqueness_index_->insertValue(domain_id, table_id, column_id,
                                                        row_id, element, tx_id, ctx);
                if (status == Status::UNIQUE_VIOLATION && ctx && ctx->message.empty())
                {
                    ctx->message = "Domain uniqueness violation";
                }
                if (status != Status::OK)
                {
                    return status;
                }
            }

            return Status::OK;
        }

        status = uniqueness_index_->insertValue(domain_id, table_id, column_id,
                                                 row_id, value, tx_id, ctx);
        if (status == Status::UNIQUE_VIOLATION && ctx && ctx->message.empty())
        {
            ctx->message = "Domain uniqueness violation";
        }
        return status;
    }

    auto DomainManager::unregisterUniqueValue(const ID& domain_id,
                                              const ID& table_id,
                                              const ID& column_id,
                                              const TID& row_id,
                                              const TypedValue& value,
                                              uint64_t tx_id,
                                              ErrorContext* ctx) -> Status
    {
        if (domain_id == ID{})
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!domain.enforce_global_uniqueness && !domain.integrity.uniqueness_check)
        {
            return Status::OK;
        }

        if (!uniqueness_index_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                              "Global uniqueness index unavailable");
            return Status::INTERNAL_ERROR;
        }

        if (value.type() == DataType::ARRAY)
        {
            if (value.isNull())
            {
                return Status::OK;
            }

            for (const auto& element : value.getArray())
            {
                if (element.isNull())
                {
                    continue;
                }

                status = uniqueness_index_->deleteValue(domain_id, table_id, column_id,
                                                        row_id, element, tx_id, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    return status;
                }
            }

            return Status::OK;
        }

        status = uniqueness_index_->deleteValue(domain_id, table_id, column_id,
                                                 row_id, value, tx_id, ctx);
        if (status == Status::NOT_FOUND)
        {
            return Status::OK;
        }
        return status;
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
                                          const DomainCreateOptions& options,
                                          ID& domain_id,
                                          ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }

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

        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::RECORD;
        info.base_type = DataType::COMPOSITE;
        info.fields = fields;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        // Write to catalog
        status = writeDomainRecord(info, ctx);
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

    auto DomainManager::createRecordDomain(const ID& schema_id,
                                          const std::string& domain_name,
                                          const std::vector<RecordField>& fields,
                                          ID& domain_id,
                                          ErrorContext* ctx) -> Status
    {
        DomainCreateOptions options;
        return createRecordDomain(schema_id, domain_name, fields, options, domain_id, ctx);
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
        if (field_name.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Field name cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        // Check if value is COMPOSITE type
        if (record_value.type() != DataType::COMPOSITE)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not a COMPOSITE/RECORD type");
            return Status::TYPE_MISMATCH;
        }
        if (record_value.isNull())
        {
            field_value = TypedValue::makeNull();
            return Status::OK;
        }

        const auto names = record_value.getCompositeFieldNames();
        const auto& values = record_value.getCompositeValues();

        if (names.size() != values.size())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Composite field/value count mismatch");
            return Status::DATA_CORRUPTED;
        }

        for (size_t i = 0; i < names.size(); ++i)
        {
            if (names[i] == field_name)
            {
                field_value = values[i];
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Field not found in composite value");
        return Status::NOT_FOUND;
    }

    // ====================
    // Phase 3: ENUM Domains
    // ====================

    auto DomainManager::createEnumDomain(const ID& schema_id,
                                        const std::string& domain_name,
                                        const std::vector<EnumValue>& values,
                                        const DomainCreateOptions& options,
                                        ID& domain_id,
                                        ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }

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

        // Validate positions are sequential starting from 1
        for (size_t i = 0; i < values.size(); i++)
        {
            uint32_t expected = static_cast<uint32_t>(i + 1);
            if (values[i].position != expected)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                 "ENUM positions must be sequential starting from 1");
                return Status::INVALID_ARGUMENT;
            }
        }

        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::ENUM;
        info.base_type = DataType::VARCHAR;  // ENUMs stored as VARCHAR
        info.enum_values = values;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        // Write to catalog
        status = writeDomainRecord(info, ctx);
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

    auto DomainManager::createEnumDomain(const ID& schema_id,
                                        const std::string& domain_name,
                                        const std::vector<EnumValue>& values,
                                        ID& domain_id,
                                        ErrorContext* ctx) -> Status
    {
        DomainCreateOptions options;
        return createEnumDomain(schema_id, domain_name, values, options, domain_id, ctx);
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

        // Find current value index
        int32_t current_index = -1;
        for (size_t i = 0; i < domain.enum_values.size(); ++i)
        {
            if (domain.enum_values[i].label == current_label)
            {
                current_index = static_cast<int32_t>(i);
                break;
            }
        }

        if (current_index == -1)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Current value not found in ENUM");
            return Status::NOT_FOUND;
        }

        // Check if there's a next value
        int32_t next_index = current_index + 1;
        if (next_index >= static_cast<int32_t>(domain.enum_values.size()))
        {
            if (!domain.enum_wrap)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "No next value (already at last position)");
                return Status::OUT_OF_RANGE;
            }
            next_index = 0;
        }

        // Return next value
        next_label = domain.enum_values[next_index].label;
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

        // Validate position (1-based)
        if (position < 1 || position > static_cast<int32_t>(domain.enum_values.size()))
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Position out of range");
            return Status::OUT_OF_RANGE;
        }

        // Return value at position
        label = domain.enum_values[static_cast<size_t>(position - 1)].label;
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
                                       const DomainTypeRef& element_type,
                                       const DomainCreateOptions& options,
                                       ID& domain_id,
                                       ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Validate element type
        if (element_type.type == DataType::UNKNOWN && element_type.domain_id == ID{})
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "SET element type cannot be UNKNOWN");
            return Status::INVALID_ARGUMENT;
        }

        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::SET;
        info.base_type = DataType::ARRAY;  // SETs stored as ARRAY with unique elements
        info.set_element_type = element_type;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        // Write to catalog
        status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write SET domain record");
            return status;
        }

        // Add to cache
        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created SET domain '%s' with element type %d",
                domain_name.c_str(), static_cast<int>(element_type.type));

        return Status::OK;
    }

    auto DomainManager::createSetDomain(const ID& schema_id,
                                       const std::string& domain_name,
                                       DataType element_type,
                                       ID& domain_id,
                                       ErrorContext* ctx) -> Status
    {
        DomainTypeRef ref;
        ref.type = element_type;
        DomainCreateOptions options;
        return createSetDomain(schema_id, domain_name, ref, options, domain_id, ctx);
    }

    auto DomainManager::setContains(const TypedValue& set_value,
                                   const TypedValue& element,
                                   bool& result,
                                   ErrorContext* ctx) -> Status
    {
        if (element.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::NULL_VALUE_NOT_ALLOWED, "SET element cannot be NULL");
            return Status::NULL_VALUE_NOT_ALLOWED;
        }

        NormalizedSet normalized;
        Status status = normalizeSetValue(set_value, normalized, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (normalized.element_type != DataType::UNKNOWN &&
            element.type() != normalized.element_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "SET element type mismatch");
            return Status::TYPE_MISMATCH;
        }

        SetElementKey key;
        status = buildSetElementKey(element, key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto it = std::lower_bound(normalized.keys.begin(), normalized.keys.end(), key);
        result = (it != normalized.keys.end() && *it == key);
        return Status::OK;
    }

    auto DomainManager::setsOverlap(const TypedValue& set1,
                                   const TypedValue& set2,
                                   bool& result,
                                   ErrorContext* ctx) -> Status
    {
        NormalizedSet left;
        Status status = normalizeSetValue(set1, left, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        NormalizedSet right;
        status = normalizeSetValue(set2, right, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!left.keys.empty() && !right.keys.empty() &&
            left.element_type != right.element_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "SET element type mismatch");
            return Status::TYPE_MISMATCH;
        }

        result = false;
        size_t i = 0;
        size_t j = 0;
        while (i < left.keys.size() && j < right.keys.size())
        {
            if (left.keys[i] == right.keys[j])
            {
                result = true;
                break;
            }
            if (left.keys[i] < right.keys[j])
            {
                ++i;
            }
            else
            {
                ++j;
            }
        }

        return Status::OK;
    }

    auto DomainManager::setUnion(const TypedValue& set1,
                               const TypedValue& set2,
                               TypedValue& result,
                               ErrorContext* ctx) -> Status
    {
        NormalizedSet left;
        Status status = normalizeSetValue(set1, left, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        NormalizedSet right;
        status = normalizeSetValue(set2, right, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!left.keys.empty() && !right.keys.empty() &&
            left.element_type != right.element_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "SET element type mismatch");
            return Status::TYPE_MISMATCH;
        }

        std::vector<TypedValue> merged;
        merged.reserve(left.values.size() + right.values.size());

        size_t i = 0;
        size_t j = 0;
        while (i < left.keys.size() || j < right.keys.size())
        {
            if (i >= left.keys.size())
            {
                merged.push_back(right.values[j++]);
                continue;
            }
            if (j >= right.keys.size())
            {
                merged.push_back(left.values[i++]);
                continue;
            }

            if (left.keys[i] == right.keys[j])
            {
                merged.push_back(left.values[i]);
                ++i;
                ++j;
            }
            else if (left.keys[i] < right.keys[j])
            {
                merged.push_back(left.values[i++]);
            }
            else
            {
                merged.push_back(right.values[j++]);
            }
        }

        result = TypedValue::makeArray(merged);
        return Status::OK;
    }

    auto DomainManager::setIntersection(const TypedValue& set1,
                                      const TypedValue& set2,
                                      TypedValue& result,
                                      ErrorContext* ctx) -> Status
    {
        NormalizedSet left;
        Status status = normalizeSetValue(set1, left, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        NormalizedSet right;
        status = normalizeSetValue(set2, right, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!left.keys.empty() && !right.keys.empty() &&
            left.element_type != right.element_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "SET element type mismatch");
            return Status::TYPE_MISMATCH;
        }

        std::vector<TypedValue> intersection;
        size_t i = 0;
        size_t j = 0;
        while (i < left.keys.size() && j < right.keys.size())
        {
            if (left.keys[i] == right.keys[j])
            {
                intersection.push_back(left.values[i]);
                ++i;
                ++j;
            }
            else if (left.keys[i] < right.keys[j])
            {
                ++i;
            }
            else
            {
                ++j;
            }
        }

        result = TypedValue::makeArray(intersection);
        return Status::OK;
    }

    auto DomainManager::setDifference(const TypedValue& set1,
                                      const TypedValue& set2,
                                      TypedValue& result,
                                      ErrorContext* ctx) -> Status
    {
        NormalizedSet left;
        Status status = normalizeSetValue(set1, left, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        NormalizedSet right;
        status = normalizeSetValue(set2, right, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!left.keys.empty() && !right.keys.empty() &&
            left.element_type != right.element_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "SET element type mismatch");
            return Status::TYPE_MISMATCH;
        }

        std::vector<TypedValue> difference;
        difference.reserve(left.values.size());

        size_t i = 0;
        size_t j = 0;
        while (i < left.keys.size())
        {
            if (j >= right.keys.size())
            {
                difference.push_back(left.values[i++]);
                continue;
            }

            if (left.keys[i] == right.keys[j])
            {
                ++i;
                ++j;
            }
            else if (left.keys[i] < right.keys[j])
            {
                difference.push_back(left.values[i++]);
            }
            else
            {
                ++j;
            }
        }

        result = TypedValue::makeArray(difference);
        return Status::OK;
    }

    // ====================
    // Phase 5: VARIANT Type
    // ====================

    auto DomainManager::createVariantDomain(const ID& schema_id,
                                           const std::string& domain_name,
                                           const std::vector<DomainTypeRef>& allowed_types,
                                           const DomainCreateOptions& options,
                                           ID& domain_id,
                                           ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Validate allowed types
        if (allowed_types.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "VARIANT domain must allow at least one type");
            return Status::INVALID_ARGUMENT;
        }

        // Check for UNKNOWN type and duplicates
        std::unordered_set<std::string> type_set;
        for (const auto& type : allowed_types)
        {
            if (type.type == DataType::UNKNOWN && type.domain_id == ID{})
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "VARIANT cannot allow UNKNOWN type");
                return Status::INVALID_ARGUMENT;
            }
            std::string key;
            if (type.domain_id != ID{})
            {
                key = type.domain_id.toString();
            }
            else
            {
                key = std::to_string(static_cast<uint32_t>(type.type)) + ":" +
                      std::to_string(type.precision) + ":" + std::to_string(type.scale);
            }
            if (type_set.count(key) > 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Duplicate type in VARIANT allowed types");
                return Status::INVALID_ARGUMENT;
            }
            type_set.insert(key);
        }

        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create domain info
        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::VARIANT;
        info.base_type = DataType::VARIANT;  // VARIANT stores tagged union
        info.variant_allowed_types = allowed_types;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        // Write to catalog
        status = writeDomainRecord(info, ctx);
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

    auto DomainManager::createVariantDomain(const ID& schema_id,
                                           const std::string& domain_name,
                                           const std::vector<DataType>& allowed_types,
                                           ID& domain_id,
                                           ErrorContext* ctx) -> Status
    {
        std::vector<DomainTypeRef> refs;
        refs.reserve(allowed_types.size());
        for (auto type : allowed_types)
        {
            DomainTypeRef ref;
            ref.type = type;
            refs.push_back(ref);
        }
        DomainCreateOptions options;
        return createVariantDomain(schema_id, domain_name, refs, options, domain_id, ctx);
    }

    // ====================
    // Phase 6b: RANGE/BASE/SHELL Types
    // ====================

    auto DomainManager::createRangeDomain(const ID& schema_id,
                                         const std::string& domain_name,
                                         const RangeTypeInfo& range_info,
                                         const DomainCreateOptions& options,
                                         ID& domain_id,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (range_info.subtype.type == DataType::UNKNOWN && range_info.subtype.domain_id == ID{})
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "RANGE subtype is required");
            return Status::INVALID_ARGUMENT;
        }

        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::RANGE;
        info.base_type = range_info.subtype.type;
        info.range_info = range_info;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write RANGE domain record");
            return status;
        }

        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created RANGE domain '%s'", domain_name.c_str());
        return Status::OK;
    }

    auto DomainManager::createBaseDomain(const ID& schema_id,
                                        const std::string& domain_name,
                                        const BaseTypeInfo& base_info,
                                        const DomainCreateOptions& options,
                                        ID& domain_id,
                                        ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (base_info.input_function.empty() || base_info.output_function.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BASE type requires INPUT and OUTPUT functions");
            return Status::INVALID_ARGUMENT;
        }

        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::BASE;
        info.base_type = DataType::UNKNOWN;
        info.base_info = base_info;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write BASE domain record");
            return status;
        }

        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created BASE domain '%s'", domain_name.c_str());
        return Status::OK;
    }

    auto DomainManager::createShellDomain(const ID& schema_id,
                                         const std::string& domain_name,
                                         const DomainCreateOptions& options,
                                         ID& domain_id,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Status status = validateDomainCreateName(domain_cache_, schema_id, domain_name,
                                                 options.allow_system_reserved_name, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = resolveDomainCreateId(domain_cache_, options.fixed_domain_id, domain_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        DomainInfo info;
        info.domain_id = domain_id;
        info.schema_id = schema_id;
        info.domain_name = domain_name;
        info.domain_type = DomainType::SHELL;
        info.base_type = DataType::UNKNOWN;
        info.nullable = options.nullable;
        info.default_value = options.default_value;
        info.constraints = options.constraints;
        info.collation_name = options.collation_name;
        info.dialect_tag = options.dialect_tag;
        info.compat_name = options.compat_name;
        info.enum_wrap = options.enum_wrap;
        info.shell_finalized = false;
        info.created_time = std::time(nullptr);
        info.last_modified_time = info.created_time;
        if (info.dialect_tag.empty())
        {
            info.dialect_tag = defaultDialectTagForCreate();
        }

        status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to write SHELL domain record");
            return status;
        }

        domain_cache_[domain_id] = info;
        domain_count_++;

        LOG_INFO(CATALOG, "Created SHELL domain '%s'", domain_name.c_str());
        return Status::OK;
    }

    auto DomainManager::addEnumValue(const ID& domain_id,
                                    const std::string& label,
                                    const std::optional<std::string>& before_label,
                                    const std::optional<std::string>& after_label,
                                    ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto& info = it->second;
        if (info.domain_type != DomainType::ENUM)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not an ENUM type");
            return Status::INVALID_ARGUMENT;
        }

        for (const auto& value : info.enum_values)
        {
            if (value.label == label)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ENUM label already exists");
                return Status::INVALID_ARGUMENT;
            }
        }

        std::vector<EnumValue> ordered = info.enum_values;
        auto insert_at = ordered.end();
        if (before_label.has_value())
        {
            insert_at = std::find_if(ordered.begin(), ordered.end(),
                                     [&](const EnumValue& v) { return v.label == *before_label; });
            if (insert_at == ordered.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "BEFORE label not found");
                return Status::NOT_FOUND;
            }
        }
        else if (after_label.has_value())
        {
            insert_at = std::find_if(ordered.begin(), ordered.end(),
                                     [&](const EnumValue& v) { return v.label == *after_label; });
            if (insert_at == ordered.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "AFTER label not found");
                return Status::NOT_FOUND;
            }
            ++insert_at;
        }

        EnumValue new_value(label, 0);
        ordered.insert(insert_at, new_value);

        for (size_t i = 0; i < ordered.size(); ++i)
        {
            ordered[i].position = static_cast<int32_t>(i + 1);
        }

        info.enum_values = std::move(ordered);
        info.last_modified_time = std::time(nullptr);

        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update ENUM domain");
            return status;
        }

        return Status::OK;
    }

    auto DomainManager::renameEnumValue(const ID& domain_id,
                                       const std::string& old_label,
                                       const std::string& new_label,
                                       ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto& info = it->second;
        if (info.domain_type != DomainType::ENUM)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not an ENUM type");
            return Status::INVALID_ARGUMENT;
        }

        bool found = false;
        for (auto& value : info.enum_values)
        {
            if (value.label == old_label)
            {
                value.label = new_label;
                found = true;
                break;
            }
        }

        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "ENUM label not found");
            return Status::NOT_FOUND;
        }

        info.last_modified_time = std::time(nullptr);

        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update ENUM domain");
            return status;
        }

        return Status::OK;
    }

    auto DomainManager::updateRangeOptions(const ID& domain_id,
                                          const RangeTypeInfo& range_info,
                                          ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto& info = it->second;
        if (info.domain_type != DomainType::RANGE)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not a RANGE type");
            return Status::INVALID_ARGUMENT;
        }

        info.range_info = range_info;
        info.last_modified_time = std::time(nullptr);
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update RANGE domain");
            return status;
        }
        return Status::OK;
    }

    auto DomainManager::updateBaseOptions(const ID& domain_id,
                                         const BaseTypeInfo& base_info,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto& info = it->second;
        if (info.domain_type != DomainType::BASE)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not a BASE type");
            return Status::INVALID_ARGUMENT;
        }

        info.base_info = base_info;
        info.last_modified_time = std::time(nullptr);
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update BASE domain");
            return status;
        }
        return Status::OK;
    }

    auto DomainManager::finalizeShellType(const ID& domain_id,
                                         const BaseTypeInfo& base_info,
                                         ErrorContext* ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = domain_cache_.find(domain_id);
        if (it == domain_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Domain not found");
            return Status::NOT_FOUND;
        }

        auto& info = it->second;
        if (info.domain_type != DomainType::SHELL)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain is not a SHELL type");
            return Status::INVALID_ARGUMENT;
        }

        info.domain_type = DomainType::BASE;
        info.base_info = base_info;
        info.shell_finalized = true;
        info.last_modified_time = std::time(nullptr);
        Status status = writeDomainRecord(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to finalize SHELL type");
            return status;
        }
        return Status::OK;
    }

    auto DomainManager::extractDataType(const TypedValue& variant_value,
                                       DataType& type,
                                       ErrorContext* ctx) -> Status
    {
        if (variant_value.isNull())
        {
            type = DataType::NULL_TYPE;
            return Status::OK;
        }

        if (variant_value.type() != DataType::VARIANT)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not a VARIANT type");
            return Status::TYPE_MISMATCH;
        }

        auto tag = variant_value.getVariantTag();
        const auto& payload = variant_value.getVariantValue();
        if (tag.has_value())
        {
            if (payload.type() != DataType::NULL_TYPE && payload.type() != *tag)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "VARIANT tag/value mismatch");
                return Status::DATA_CORRUPTED;
            }
            type = *tag;
            return Status::OK;
        }

        type = payload.type();
        return Status::OK;
    }

    auto DomainManager::isOfType(const TypedValue& variant_value,
                                DataType expected_type,
                                bool& result,
                                ErrorContext* ctx) -> Status
    {
        if (expected_type == DataType::UNKNOWN)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Expected type cannot be UNKNOWN");
            return Status::INVALID_ARGUMENT;
        }

        if (variant_value.isNull())
        {
            result = false;
            return Status::OK;
        }

        if (variant_value.type() != DataType::VARIANT)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not a VARIANT type");
            return Status::TYPE_MISMATCH;
        }

        auto tag = variant_value.getVariantTag();
        const auto& payload = variant_value.getVariantValue();
        DataType runtime = tag.has_value() ? *tag : payload.type();
        if (tag.has_value() && payload.type() != DataType::NULL_TYPE && payload.type() != *tag)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "VARIANT tag/value mismatch");
            return Status::DATA_CORRUPTED;
        }

        result = (runtime == expected_type);
        return Status::OK;
    }

    auto DomainManager::variantCast(const TypedValue& variant_value,
                                   DataType target_type,
                                   TypedValue& result,
                                   ErrorContext* ctx) -> Status
    {
        if (target_type == DataType::UNKNOWN)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Target type cannot be UNKNOWN");
            return Status::INVALID_ARGUMENT;
        }

        if (variant_value.isNull())
        {
            result = TypedValue::makeNull(target_type);
            return Status::OK;
        }

        if (variant_value.type() != DataType::VARIANT)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "Value is not a VARIANT type");
            return Status::TYPE_MISMATCH;
        }

        auto tag = variant_value.getVariantTag();
        const auto& payload = variant_value.getVariantValue();
        DataType runtime = tag.has_value() ? *tag : payload.type();
        if (tag.has_value() && payload.type() != DataType::NULL_TYPE && payload.type() != *tag)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "VARIANT tag/value mismatch");
            return Status::DATA_CORRUPTED;
        }

        if (runtime != target_type)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH, "VARIANT does not contain target type");
            return Status::TYPE_MISMATCH;
        }

        if (payload.isNull())
        {
            result = TypedValue::makeNull(target_type);
            return Status::OK;
        }

        result = payload;
        return Status::OK;
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

        DomainSecurity updated_security = security;
        if (updated_security.encryption_enabled)
        {
            EncryptionKeyManager* key_mgr = db_ ? db_->encryption_key_manager() : nullptr;
            if (!key_mgr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "EncryptionKeyManager not available");
                return Status::INVALID_ARGUMENT;
            }

            EncryptionKey active_key;
            Status key_status = key_mgr->getActiveKey(domain_id, active_key, ctx);
            if (key_status == Status::NOT_FOUND)
            {
                EncryptionAlgorithm algo = updated_security.encryption_algorithm;
                if (algo == EncryptionAlgorithm::NONE)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Encryption algorithm not set for domain");
                    return Status::INVALID_ARGUMENT;
                }
                ID new_key_id;
                key_status = key_mgr->generateKey(domain_id, algo, new_key_id, ctx);
                if (key_status != Status::OK && key_status != Status::FILE_EXISTS)
                {
                    return key_status;
                }
                key_status = key_mgr->getActiveKey(domain_id, active_key, ctx);
            }
            if (key_status != Status::OK)
            {
                if (ctx && ctx->message.empty())
                {
                    SET_ERROR_CONTEXT(ctx, key_status, "Failed to resolve active encryption key");
                }
                return key_status;
            }

            updated_security.encryption_algorithm = active_key.algorithm;
            updated_security.encryption_key_id = active_key.key_id;
        }
        else
        {
            updated_security.encryption_algorithm = EncryptionAlgorithm::NONE;
            updated_security.encryption_key_id = ID{};
        }

        // Update security options
        it->second.security = updated_security;
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
        it->second.enforce_global_uniqueness = integrity.uniqueness_check;
        it->second.last_modified_time = std::time(nullptr);

        // Update catalog
        Status status = writeDomainRecord(it->second, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update domain record");
            return status;
        }

        if (uniqueness_index_)
        {
            if (it->second.enforce_global_uniqueness)
            {
                status = uniqueness_index_->enableUniqueness(domain_id, ctx);
            }
            else
            {
                status = uniqueness_index_->disableUniqueness(domain_id, ctx);
            }
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to update domain uniqueness index");
                return status;
            }
        }

        LOG_INFO(CATALOG, "Set integrity options for domain %s",
                domain_id.toString().c_str());

        return Status::OK;
    }

    auto DomainManager::setValidationOptions(const ID& domain_id,
                                            const DomainValidationConfig& validation,
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
            if (normalized == "COPY" || normalized == "COPY_FILE")
            {
                privilege_out = CatalogManager::Privilege::COPY_FILE;
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

            constexpr std::array<CatalogManager::Privilege, 15> kPrivileges = {
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
                CatalogManager::Privilege::TEMPORARY,
                CatalogManager::Privilege::COPY_FILE
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
        (void)domain;

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

    auto DomainManager::encryptValue(const ID& domain_id,
                                     TypedValue& value,
                                     ErrorContext* ctx) -> Status
    {
        if (value.isNull())
        {
            return Status::OK;
        }

        if (value.isEncrypted())
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (!domain.security.encryption_enabled)
        {
            return Status::OK;
        }

        EncryptionKeyManager* key_mgr = db_ ? db_->encryption_key_manager() : nullptr;
        if (!key_mgr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "EncryptionKeyManager not available");
            return Status::INVALID_ARGUMENT;
        }

        EncryptionKey active_key;
        status = key_mgr->getActiveKey(domain_id, active_key, ctx);
        if (status == Status::NOT_FOUND)
        {
            EncryptionAlgorithm algo = domain.security.encryption_algorithm;
            if (algo == EncryptionAlgorithm::NONE)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Encryption algorithm not set for domain");
                return Status::INVALID_ARGUMENT;
            }

            ID new_key_id;
            status = key_mgr->generateKey(domain_id, algo, new_key_id, ctx);
            if (status != Status::OK && status != Status::FILE_EXISTS)
            {
                return status;
            }
            status = key_mgr->getActiveKey(domain_id, active_key, ctx);
        }
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to resolve active encryption key");
            }
            return status;
        }

        if (domain.security.encryption_key_id != active_key.key_id ||
            domain.security.encryption_algorithm != active_key.algorithm)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = domain_cache_.find(domain_id);
            if (it != domain_cache_.end())
            {
                it->second.security.encryption_enabled = true;
                it->second.security.encryption_algorithm = active_key.algorithm;
                it->second.security.encryption_key_id = active_key.key_id;
                it->second.last_modified_time = std::time(nullptr);

                Status update_status = writeDomainRecord(it->second, ctx);
                if (update_status != Status::OK)
                {
                    return update_status;
                }
            }
        }

        std::vector<uint8_t> plaintext_key;
        status = key_mgr->decryptKey(active_key, plaintext_key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return value.encrypt(plaintext_key, active_key.algorithm, active_key.key_version, ctx);
    }

    auto DomainManager::decryptValue(const ID& domain_id,
                                     TypedValue& value,
                                     ErrorContext* ctx) -> Status
    {
        if (!value.isEncrypted())
        {
            return Status::OK;
        }

        DomainInfo domain;
        Status status = getDomain(domain_id, domain, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptionKeyManager* key_mgr = db_ ? db_->encryption_key_manager() : nullptr;
        if (!key_mgr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "EncryptionKeyManager not available");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t key_version = value.encryptionKeyVersion();
        if (key_version == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Encrypted value missing key version");
            return Status::INVALID_ARGUMENT;
        }

        EncryptionKey key;
        status = key_mgr->getKeyByVersion(domain_id, key_version, key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> plaintext_key;
        status = key_mgr->decryptKey(key, plaintext_key, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return value.decrypt(plaintext_key, ctx);
    }

    // ====================
    // Private Helper Methods
    // ====================

    auto DomainManager::writeDomainRecord(const DomainInfo& domain, ErrorContext* ctx) -> Status
    {
        BufferPool* bp = db_->buffer_pool();
        Status status = Status::OK;
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t current_page = domains_table_page_;
        DomainRecord* record = nullptr;
        uint32_t record_page = 0;

        while (current_page != 0)
        {
            void* page_buffer = nullptr;
            Status status = bp->pinPage(current_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
                return status;
            }

            auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);

            // Scan for existing record in this page
            for (uint32_t i = 0; i < catalog_page->record_count; i++)
            {
                auto* candidate = reinterpret_cast<DomainRecord*>(
                    catalog_page->data + (i * sizeof(DomainRecord)));

                if (candidate->domain_id == domain.domain_id)
                {
                    record = candidate;
                    record_page = current_page;
                    break;
                }
            }

            if (record)
            {
                break;
            }

            const uint32_t capacity =
                (db_->page_size() - sizeof(DomainCatalogPage)) / sizeof(DomainRecord);

            if (catalog_page->record_count < capacity)
            {
                record = reinterpret_cast<DomainRecord*>(
                    catalog_page->data + (catalog_page->record_count * sizeof(DomainRecord)));
                catalog_page->record_count++;
                record_page = current_page;
                break;
            }

            if (catalog_page->next_page == 0)
            {
                uint32_t new_page_id = 0;
                void* new_buffer = nullptr;
                status = bp->allocatePage(&new_page_id, &new_buffer, ctx);
                if (status != Status::OK)
                {
                    bp->unpinPage(current_page, false, ctx);
                    return status;
                }

                std::memset(new_buffer, 0, db_->page_size());
                auto* new_page = reinterpret_cast<DomainCatalogPage*>(new_buffer);
                new_page->header.magic = K_MAGIC_SBRD;
                new_page->header.version = 1;
                new_page->header.page_type = PAGE_TYPE_HEAP;
                new_page->header.page_size = db_->page_size();
                new_page->header.page_id = new_page_id;
                new_page->header.generation = 1;
                new_page->header.checksum = 0;
                new_page->header.flags = 0;
                new_page->header.lsn = 0;
                pageSetLower(new_page->header, sizeof(DomainCatalogPage));
                pageSetUpper(new_page->header, db_->page_size());
                pageSetSpecial(new_page->header, db_->page_size());
                new_page->record_count = 0;
                new_page->free_offset = sizeof(DomainCatalogPage);
                new_page->next_page = 0;
                new_page->reserved = 0;

                catalog_page->next_page = new_page_id;
                status = bp->unpinPage(new_page_id, true, ctx);
                if (status != Status::OK)
                {
                    bp->unpinPage(current_page, true, ctx);
                    return status;
                }

                status = bp->unpinPage(current_page, true, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                current_page = new_page_id;
                continue;
            }

            uint32_t next_page = catalog_page->next_page;
            bp->unpinPage(current_page, false, ctx);
            current_page = next_page;
        }

        if (!record || record_page == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to allocate domain record slot");
            return Status::IO_ERROR;
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
        record->set_element_type = static_cast<uint16_t>(domain.set_element_type.type);

        std::strncpy(record->dialect_tag, domain.dialect_tag.c_str(), sizeof(record->dialect_tag) - 1);
        record->dialect_tag[sizeof(record->dialect_tag) - 1] = '\0';

        std::strncpy(record->compat_name, domain.compat_name.c_str(), sizeof(record->compat_name) - 1);
        record->compat_name[sizeof(record->compat_name) - 1] = '\0';

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            bp->unpinPage(record_page, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        uint64_t xmin = ConnectionContext::getCurrentTransactionId();

        record->security_oid = ID{};
        std::string security_blob = serializeDomainSecurity(domain.security);
        if (!security_blob.empty())
        {
            status = catalog->storeStringInToast(security_blob, xmin, record->security_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain security in TOAST");
                return status;
            }
        }

        record->integrity_oid = ID{};
        std::string integrity_blob = serializeDomainIntegrity(domain.integrity);
        if (!integrity_blob.empty())
        {
            status = catalog->storeStringInToast(integrity_blob, xmin, record->integrity_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain integrity in TOAST");
                return status;
            }
        }

        record->validation_oid = ID{};
        std::string validation_blob = serializeDomainValidation(domain.validation);
        if (!validation_blob.empty())
        {
            status = catalog->storeStringInToast(validation_blob, xmin, record->validation_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain validation in TOAST");
                return status;
            }
        }

        record->quality_oid = ID{};
        std::string quality_blob = serializeDomainQuality(domain.quality);
        if (!quality_blob.empty())
        {
            status = catalog->storeStringInToast(quality_blob, xmin, record->quality_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain quality in TOAST");
                return status;
            }
        }

        record->constraints_oid = ID{};
        std::string constraints_blob = serializeDomainConstraints(domain);
        if (!constraints_blob.empty())
        {
            status = catalog->storeStringInToast(constraints_blob, xmin, record->constraints_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain constraints in TOAST");
                return status;
            }
        }

        record->fields_oid = ID{};
        std::string fields_blob;
        if (domain.domain_type == DomainType::RECORD)
        {
            fields_blob = serializeDomainFields(domain.fields);
        }
        else if (domain.domain_type == DomainType::RANGE)
        {
            fields_blob = serializeDomainRangeInfo(domain.range_info);
        }
        else if (domain.domain_type == DomainType::BASE)
        {
            fields_blob = serializeDomainBaseInfo(domain.base_info);
        }
        if (!fields_blob.empty())
        {
            status = catalog->storeStringInToast(fields_blob, xmin, record->fields_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain fields in TOAST");
                return status;
            }
        }

        record->enum_values_oid = ID{};
        std::string enum_values_blob;
        if (domain.domain_type == DomainType::ENUM)
        {
            enum_values_blob = serializeDomainEnumValues(domain.enum_values);
        }
        if (!enum_values_blob.empty())
        {
            status = catalog->storeStringInToast(enum_values_blob, xmin,
                                                 record->enum_values_oid, ctx);
            if (status != Status::OK)
            {
                bp->unpinPage(record_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to store domain enum values in TOAST");
                return status;
            }
        }

        return bp->unpinPage(record_page, true, ctx);
    }

    auto DomainManager::readDomainRecords(ErrorContext* ctx) -> Status
    {
        domain_cache_.clear();
        domain_count_ = 0;
        BufferPool* bp = db_->buffer_pool();
        void* page_buffer;
        uint32_t page_id = domains_table_page_;

        while (page_id != 0)
        {
            Status status = bp->pinPage(page_id, &page_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin domains catalog page");
                return status;
            }

            auto* catalog_page = reinterpret_cast<DomainCatalogPage*>(page_buffer);
            uint32_t next_page = catalog_page->next_page;

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
                    info.set_element_type.type = static_cast<DataType>(record->set_element_type);
                    info.dialect_tag = record->dialect_tag;
                    if (info.dialect_tag.empty())
                    {
                        info.dialect_tag = "scratchbird";
                    }
                    info.compat_name = record->compat_name;

                    CatalogManager* catalog = db_->catalog_manager();
                    if (!catalog)
                    {
                        bp->unpinPage(page_id, false, ctx);
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
                        return Status::INVALID_ARGUMENT;
                    }

                    uint64_t xmin = ConnectionContext::getCurrentTransactionId();
                    std::string blob;

                    if (!isZeroUuidLocal(record->security_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->security_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        load_status = deserializeDomainSecurity(blob, info.security, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    if (!isZeroUuidLocal(record->integrity_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->integrity_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        load_status = deserializeDomainIntegrity(blob, info.integrity, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    info.enforce_global_uniqueness = info.integrity.uniqueness_check;

                    if (!isZeroUuidLocal(record->validation_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->validation_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        load_status = deserializeDomainValidation(blob, info.validation, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    if (!isZeroUuidLocal(record->quality_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->quality_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        load_status = deserializeDomainQuality(blob, info.quality, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    if (!isZeroUuidLocal(record->constraints_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->constraints_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        load_status = deserializeDomainConstraints(blob,
                                                                   info.constraints,
                                                                   info.variant_allowed_types,
                                                                   info.collation_name,
                                                                   info.enum_wrap,
                                                                   info.set_element_type,
                                                                   ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    if (!isZeroUuidLocal(record->fields_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->fields_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }

                        if (info.domain_type == DomainType::RECORD)
                        {
                            load_status = deserializeDomainFields(blob, info.fields, ctx);
                        }
                        else if (info.domain_type == DomainType::RANGE)
                        {
                            load_status = deserializeDomainRangeInfo(blob, info.range_info, ctx);
                        }
                        else if (info.domain_type == DomainType::BASE)
                        {
                            load_status = deserializeDomainBaseInfo(blob, info.base_info, ctx);
                        }
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    if (!isZeroUuidLocal(record->enum_values_oid))
                    {
                        Status load_status = catalog->loadStringFromToast(record->enum_values_oid,
                                                                          xmin, blob, ctx);
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        if (info.domain_type == DomainType::ENUM)
                        {
                            load_status = deserializeDomainEnumValues(blob, info.enum_values, ctx);
                        }
                        if (load_status != Status::OK)
                        {
                            bp->unpinPage(page_id, false, ctx);
                            return load_status;
                        }
                        blob.clear();
                    }

                    if (info.domain_type != DomainType::VARIANT)
                    {
                        info.variant_allowed_types.clear();
                    }
                    if (info.domain_type != DomainType::SET)
                    {
                        info.set_element_type = DomainTypeRef{};
                    }
                    if (info.domain_type != DomainType::ENUM)
                    {
                        info.enum_wrap = false;
                    }
                    if (info.domain_type != DomainType::RANGE)
                    {
                        info.range_info = RangeTypeInfo{};
                    }
                    if (info.domain_type != DomainType::BASE)
                    {
                        info.base_info = BaseTypeInfo{};
                    }

                    domain_cache_[info.domain_id] = info;
                    domain_count_++;
                }
            }

            status = bp->unpinPage(page_id, false, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            page_id = next_page;
        }

        return Status::OK;
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
