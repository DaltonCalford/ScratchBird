/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/redis_catalog.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

namespace {

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trimAscii(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool parseInt32Value(const std::string& text, int32_t& out) {
    if (text.empty()) {
        return false;
    }
    size_t index = 0;
    try {
        const long parsed = std::stol(text, &index, 10);
        if (index != text.size()) {
            return false;
        }
        if (parsed < std::numeric_limits<int32_t>::min() ||
            parsed > std::numeric_limits<int32_t>::max()) {
            return false;
        }
        out = static_cast<int32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseNoteField(const std::string& notes,
                    const std::string& key,
                    std::string& value_out) {
    const std::string needle = toLowerAscii(key) + "=";
    const std::string lower_notes = toLowerAscii(notes);
    const size_t pos = lower_notes.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    size_t start = pos + needle.size();
    size_t end = notes.size();
    for (size_t i = start; i < notes.size(); ++i) {
        if (notes[i] == ';' || notes[i] == ',' || notes[i] == '\n' || notes[i] == '\r') {
            end = i;
            break;
        }
    }
    value_out = trimAscii(notes.substr(start, end - start));
    return !value_out.empty();
}

} // namespace

RedisCatalogHandler::RedisCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType RedisCatalogHandler::getProtocolType() const {
    return ProtocolType::REDIS;
}

bool RedisCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "redis_meta");
}

bool RedisCatalogHandler::ownsTable(const std::string& schema_name,
                                    const std::string& table_name) const {
    if (!ownsSchema(schema_name)) {
        return false;
    }
    for (const auto& name : table_names_) {
        if (equalsCaseInsensitive(name, table_name)) {
            return true;
        }
    }
    return false;
}

Status RedisCatalogHandler::queryTable(const std::string& schema_name,
                                       const std::string& table_name,
                                       const std::string& /*where_clause*/,
                                       VirtualResultSet& results,
                                       ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    const ColumnDefs* def = getTableDefinition(table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Unknown table: " + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setResultColumns(*def, results);

    if (equalsCaseInsensitive(table_name, "info")) {
        return queryInfo(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "config")) {
        return queryConfig(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "clients")) {
        return queryClients(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "slowlog")) {
        return querySlowlog(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "keyspace")) {
        return queryKeyspace(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "commands")) {
        return queryCommands(results, ctx);
    }

    return Status::OK;
}

Status RedisCatalogHandler::getTableColumns(
    const std::string& schema_name,
    const std::string& table_name,
    std::vector<CatalogManager::ColumnInfo>& columns,
    ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }
    const ColumnDefs* def = getTableDefinition(table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Unknown table: " + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setColumnInfo(*def, columns);
    return Status::OK;
}

Status RedisCatalogHandler::listTables(const std::string& schema_name,
                                       std::vector<std::string>& table_names,
                                       ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status RedisCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                        ErrorContext* /*ctx*/) {
    schema_names = {"redis_meta"};
    return Status::OK;
}

void RedisCatalogHandler::initializeTableNames() {
    table_names_ = {"info", "config", "clients", "slowlog", "keyspace", "commands"};
}

const RedisCatalogHandler::ColumnDefs* RedisCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"info",
         {
             {"section", DataType::VARCHAR, false},
             {"key", DataType::VARCHAR, false},
             {"value", DataType::VARCHAR, false},
         }},
        {"config",
         {
             {"parameter", DataType::VARCHAR, false},
             {"value", DataType::VARCHAR, false},
         }},
        {"clients",
         {
             {"client_id", DataType::VARCHAR, false},
             {"username", DataType::VARCHAR, false},
             {"emulation_mode", DataType::VARCHAR, true},
             {"last_activity_time", DataType::INT64, false},
         }},
        {"slowlog",
         {
             {"entry_id", DataType::INT64, false},
             {"command", DataType::VARCHAR, false},
             {"duration_us", DataType::INT64, false},
             {"start_time", DataType::INT64, false},
         }},
        {"keyspace",
         {
             {"db", DataType::VARCHAR, false},
             {"keys", DataType::INT64, false},
             {"expires", DataType::INT64, false},
         }},
        {"commands",
         {
             {"command_name", DataType::VARCHAR, false},
             {"arity", DataType::INT32, false},
             {"flags", DataType::VARCHAR, false},
         }},
    };
    std::string key = table_name;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = kDefs.find(key);
    if (it == kDefs.end()) {
        return nullptr;
    }
    return &it->second;
}

void RedisCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void RedisCatalogHandler::setColumnInfo(const ColumnDefs& def,
                                        std::vector<CatalogManager::ColumnInfo>& columns) const {
    columns.clear();
    uint16_t ordinal = 0;
    for (const auto& col : def) {
        CatalogManager::ColumnInfo info{};
        info.column_name = col.name;
        info.ordinal = ++ordinal;
        info.data_type = static_cast<uint16_t>(col.type);
        info.nullable = col.nullable;
        columns.push_back(std::move(info));
    }
}

Status RedisCatalogHandler::queryInfo(VirtualResultSet& results, ErrorContext* ctx) {
    std::vector<CatalogManager::SessionInfo> sessions;
    if (catalog_manager_ && catalog_manager_->listSessions(sessions, ctx) != Status::OK) {
        sessions.clear();
    }

    VirtualRow row_connections;
    row_connections.columns = {
        {"section", TypedValue::makeVarchar("clients")},
        {"key", TypedValue::makeVarchar("connected_clients")},
        {"value", TypedValue::makeVarchar(std::to_string(sessions.size()))},
    };
    results.rows.push_back(std::move(row_connections));

    VirtualRow row_mode;
    row_mode.columns = {
        {"section", TypedValue::makeVarchar("server")},
        {"key", TypedValue::makeVarchar("mode")},
        {"value", TypedValue::makeVarchar("standalone")},
    };
    results.rows.push_back(std::move(row_mode));
    return Status::OK;
}

Status RedisCatalogHandler::queryConfig(VirtualResultSet& results, ErrorContext* /*ctx*/) {
    const std::vector<std::pair<std::string, std::string>> rows = {
        {"timeout", "0"},
        {"maxmemory", "0"},
        {"appendonly", "no"},
    };
    for (const auto& row_data : rows) {
        VirtualRow row;
        row.columns = {
            {"parameter", TypedValue::makeVarchar(row_data.first)},
            {"value", TypedValue::makeVarchar(row_data.second)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status RedisCatalogHandler::queryClients(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SessionInfo> sessions;
    if (catalog_manager_->listSessions(sessions, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& session : sessions) {
        VirtualRow row;
        row.columns = {
            {"client_id", TypedValue::makeVarchar(session.session_id.toString())},
            {"username", TypedValue::makeVarchar(session.username)},
            {"emulation_mode", TypedValue::makeVarchar(session.emulation_mode)},
            {"last_activity_time", TypedValue::makeInt64(static_cast<int64_t>(session.last_activity_time))},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status RedisCatalogHandler::querySlowlog(VirtualResultSet& results, ErrorContext* ctx) {
    std::vector<CatalogManager::SessionInfo> sessions;
    if (catalog_manager_ && catalog_manager_->listSessions(sessions, ctx) != Status::OK) {
        sessions.clear();
    }

    int64_t entry_id = 1;
    for (const auto& session : sessions) {
        VirtualRow row;
        row.columns = {
            {"entry_id", TypedValue::makeInt64(entry_id++)},
            {"command", TypedValue::makeVarchar("QUERY")},
            {"duration_us", TypedValue::makeInt64(0)},
            {"start_time", TypedValue::makeInt64(static_cast<int64_t>(session.last_activity_time))},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status RedisCatalogHandler::queryKeyspace(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    int64_t total_keys = 0;
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
                continue;
            }
            total_keys += static_cast<int64_t>(tables.size());
        }
    }

    VirtualRow row;
    row.columns = {
        {"db", TypedValue::makeVarchar("db0")},
        {"keys", TypedValue::makeInt64(total_keys)},
        {"expires", TypedValue::makeInt64(0)},
    };
    results.rows.push_back(std::move(row));
    return Status::OK;
}

Status RedisCatalogHandler::queryCommands(VirtualResultSet& results, ErrorContext* ctx) {
    bool emitted_from_catalog = false;
    std::set<std::string> emitted_command_names;

    if (catalog_manager_) {
        std::vector<CatalogManager::ParserProfileCatalogInfo> profiles;
        if (catalog_manager_->listParserProfileCatalogEntries(
                CatalogManager::EmulationEngine::REDIS, profiles, ctx) == Status::OK) {
            for (const auto& profile : profiles) {
                if (!profile.is_enabled || !profile.is_valid) {
                    continue;
                }

                for (const std::string& family : {std::string("redis_command"), std::string("command")}) {
                    std::vector<CatalogManager::ParserCapabilityCatalogInfo> capabilities;
                    if (catalog_manager_->listParserCapabilityCatalogEntries(
                            profile.parser_profile_id, family, capabilities, ctx) != Status::OK) {
                        continue;
                    }

                    for (const auto& capability : capabilities) {
                        if (!capability.is_enabled || !capability.is_valid ||
                            capability.feature_key.empty()) {
                            continue;
                        }

                        const std::string command_name = toUpperAscii(capability.feature_key);
                        if (!emitted_command_names.insert(command_name).second) {
                            continue;
                        }

                        int32_t arity = -1;
                        std::string field_value;
                        if (parseNoteField(capability.notes, "arity", field_value)) {
                            int32_t parsed_arity = 0;
                            if (parseInt32Value(field_value, parsed_arity)) {
                                arity = parsed_arity;
                            }
                        }

                        std::string flags;
                        if (parseNoteField(capability.notes, "flags", field_value)) {
                            flags = field_value;
                        } else {
                            switch (capability.capability_action) {
                                case CatalogManager::ParserCapabilityAction::IMPLEMENT:
                                    flags = "write";
                                    break;
                                case CatalogManager::ParserCapabilityAction::REMAP:
                                    flags = "readonly";
                                    break;
                                case CatalogManager::ParserCapabilityAction::REJECT:
                                    flags = "disabled";
                                    break;
                                default:
                                    flags = "write";
                                    break;
                            }
                        }

                        VirtualRow row;
                        row.columns = {
                            {"command_name", TypedValue::makeVarchar(command_name)},
                            {"arity", TypedValue::makeInt32(arity)},
                            {"flags", TypedValue::makeVarchar(flags)},
                        };
                        results.rows.push_back(std::move(row));
                        emitted_from_catalog = true;
                    }
                }
            }
        }
    }

    if (emitted_from_catalog) {
        return Status::OK;
    }

    struct CommandDef {
        const char* name;
        int32_t arity;
        const char* flags;
    };
    const std::vector<CommandDef> commands = {
        {"GET", 2, "readonly"},
        {"SET", -3, "write"},
        {"DEL", -2, "write"},
        {"HGETALL", 2, "readonly"},
        {"XADD", -5, "write"},
    };
    for (const auto& cmd : commands) {
        VirtualRow row;
        row.columns = {
            {"command_name", TypedValue::makeVarchar(cmd.name)},
            {"arity", TypedValue::makeInt32(cmd.arity)},
            {"flags", TypedValue::makeVarchar(cmd.flags)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

} // namespace scratchbird::catalog
