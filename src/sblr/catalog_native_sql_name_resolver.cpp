#include "scratchbird/sblr/catalog_native_sql_name_resolver.h"

#include <string>
#include <string_view>

#include "scratchbird/core/catalog_manager.h"

namespace scratchbird::sblr {

namespace {

using core::CatalogManager;
using ObjectType = core::CatalogManager::ObjectType;

int hexNibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

bool parseUuidText(std::string_view text, core::ID& out) {
    if (text.size() != 36) {
        return false;
    }
    if (text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-') {
        return false;
    }

    size_t byte_index = 0;
    for (size_t i = 0; i < text.size() && byte_index < out.bytes.size();) {
        if (text[i] == '-') {
            ++i;
            continue;
        }
        if (i + 1 >= text.size()) {
            return false;
        }
        const int hi = hexNibble(text[i]);
        const int lo = hexNibble(text[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.bytes[byte_index++] = static_cast<uint8_t>((hi << 4) | lo);
        i += 2;
    }
    return byte_index == out.bytes.size();
}

ObjectType mapHint(NativeSqlObjectTypeHint hint) {
    switch (hint) {
        case NativeSqlObjectTypeHint::SCHEMA:
            return ObjectType::SCHEMA;
        case NativeSqlObjectTypeHint::TABLE:
            return ObjectType::TABLE;
        case NativeSqlObjectTypeHint::INDEX:
            return ObjectType::INDEX;
        case NativeSqlObjectTypeHint::VIEW:
            return ObjectType::VIEW;
        case NativeSqlObjectTypeHint::POLICY:
            return ObjectType::POLICY;
        case NativeSqlObjectTypeHint::USER:
            return ObjectType::USER;
        case NativeSqlObjectTypeHint::ROLE:
            return ObjectType::ROLE;
        case NativeSqlObjectTypeHint::GROUP:
            return ObjectType::GROUP;
        case NativeSqlObjectTypeHint::JOB:
            return ObjectType::JOB;
        case NativeSqlObjectTypeHint::DATABASE:
            return ObjectType::DATABASE;
        case NativeSqlObjectTypeHint::UNKNOWN:
        default:
            return ObjectType::UNKNOWN;
    }
}

}  // namespace

CatalogNativeSqlNameResolver::CatalogNativeSqlNameResolver(core::CatalogManager* catalog_manager)
    : catalog_manager_(catalog_manager) {}

bool CatalogNativeSqlNameResolver::resolveNameByUuid(const std::string& uuid_text,
                                                     NativeSqlObjectTypeHint hint,
                                                     std::string& resolved_name) {
    resolved_name.clear();
    if (catalog_manager_ == nullptr) {
        return false;
    }

    core::ID object_id{};
    if (!parseUuidText(uuid_text, object_id)) {
        return false;
    }

    core::ErrorContext ctx;
    core::CatalogManager::ResolvedObject resolved{};
    if (catalog_manager_->resolveObjectId(object_id, resolved, &ctx) != core::Status::OK) {
        return false;
    }

    const ObjectType expected = mapHint(hint);
    if (expected != ObjectType::UNKNOWN && resolved.object_type != expected) {
        return false;
    }

    if (!resolved.object_name.empty()) {
        resolved_name = resolved.object_name;
        return true;
    }
    if (!resolved.full_path.empty()) {
        resolved_name = resolved.full_path;
        return true;
    }
    return false;
}

}  // namespace scratchbird::sblr
