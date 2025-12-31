/**
 * ScratchBird SBLR v2.0 - Semantic Analyzer Implementation
 *
 * See: include/scratchbird/sblr/semantic_analyzer_v2.h
 */

#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/domain_manager.h"
#include <algorithm>
#include <array>
#include <cassert>

namespace scratchbird::parser::v2 {
namespace {
core::CatalogManager::ObjectType toCatalogObjectType(DdlObjectType type) {
    return static_cast<core::CatalogManager::ObjectType>(static_cast<uint8_t>(type));
}

core::ObjectPath buildObjectPath(const SchemaPath& path, const StringPool& pool) {
    core::ObjectPath out;
    out.type = static_cast<core::PathType>(path.type);
    out.no_search_path = path.no_search_path;
    out.components.reserve(path.components.size());
    for (auto id : path.components) {
        out.components.emplace_back(pool.get(id));
    }
    return out;
}

SchemaPath appendPathComponent(const SchemaPath& base,
                               StringPool::StringId name,
                               SourceSpan span) {
    SchemaPath combined = base;
    combined.components.push_back(name);
    combined.span = span;
    return combined;
}

std::string stripRootPrefixForDisplay(const std::string& schema_path) {
    if (schema_path.empty()) {
        return schema_path;
    }

    size_t dot_pos = schema_path.find('.');
    std::string first_component =
        dot_pos == std::string::npos ? schema_path : schema_path.substr(0, dot_pos);
    if (core::IdentifierUtils::namesMatch(first_component, false /*search_delimited*/,
                                          "root", false /*stored_delimited*/)) {
        if (dot_pos == std::string::npos) {
            return std::string();
        }
        return schema_path.substr(dot_pos + 1);
    }

    return schema_path;
}
} // namespace

// =============================================================================
// SemanticResult Implementation
// =============================================================================

void SemanticResult::addError(const SemanticError& error) {
    errors_.push_back(error);
}

void SemanticResult::addWarning(const SemanticError& warning) {
    warnings_.push_back(warning);
}

// =============================================================================
// ResolutionScope Implementation
// =============================================================================

void ResolutionScope::addTable(const TableEntry& entry) {
    size_t index = tables_.size();
    tables_.push_back(entry);
    if (entry.alias != StringPool::INVALID_ID) {
        table_map_[entry.alias] = index;
    }
}

const ResolutionScope::TableEntry* ResolutionScope::findTable(StringPool::StringId name) const {
    auto it = table_map_.find(name);
    if (it != table_map_.end()) {
        return &tables_[it->second];
    }
    return nullptr;
}

ResolutionScope::ColumnLookupResult ResolutionScope::findColumn(StringPool::StringId name) const {
    ColumnLookupResult result;

    for (const auto& table : tables_) {
        for (const auto& col : table.columns) {
            if (col.name == name) {
                if (result.column != nullptr) {
                    result.ambiguous = true;
                    return result;
                }
                result.table = &table;
                result.column = &col;
            }
        }
    }

    return result;
}

const ResolvedTableRef::ColumnInfo* ResolutionScope::findColumn(
    StringPool::StringId table_name,
    StringPool::StringId column_name) const
{
    const TableEntry* table = findTable(table_name);
    if (!table) {
        return nullptr;
    }

    for (const auto& col : table->columns) {
        if (col.name == column_name) {
            return &col;
        }
    }

    return nullptr;
}

void ResolutionScope::clear() {
    tables_.clear();
    table_map_.clear();
}

// =============================================================================
// ResolvedASTArena Implementation
// =============================================================================

ResolvedASTArena::ResolvedASTArena(size_t block_size)
    : current_block_(nullptr)
    , block_size_(block_size)
    , total_allocated_(0)
{
    current_block_ = allocateBlock(block_size_);
}

ResolvedASTArena::~ResolvedASTArena() {
    callDestructors();
    reset();
    if (current_block_) {
        std::free(current_block_->data);
        delete current_block_;
    }
}

ResolvedASTArena::Block* ResolvedASTArena::allocateBlock(size_t size) {
    Block* block = new Block;
    block->data = static_cast<char*>(std::malloc(size));
    block->size = size;
    block->used = 0;
    block->next = nullptr;
    return block;
}

void* ResolvedASTArena::allocate(size_t size, size_t alignment) {
    size_t current = reinterpret_cast<size_t>(current_block_->data + current_block_->used);
    size_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;

    if (current_block_->used + padding + size > current_block_->size) {
        size_t new_size = std::max(block_size_, size + alignment);
        Block* new_block = allocateBlock(new_size);
        new_block->next = current_block_;
        current_block_ = new_block;

        current = reinterpret_cast<size_t>(current_block_->data);
        aligned = (current + alignment - 1) & ~(alignment - 1);
        padding = aligned - current;
    }

    void* result = current_block_->data + current_block_->used + padding;
    current_block_->used += padding + size;
    total_allocated_ += size;

    return result;
}

void ResolvedASTArena::trackDestructor(std::function<void()> dtor) {
    destructors_.push_back(std::move(dtor));
}

void ResolvedASTArena::callDestructors() {
    for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it) {
        (*it)();
    }
    destructors_.clear();
}

void ResolvedASTArena::reset() {
    callDestructors();
    while (current_block_ && current_block_->next) {
        Block* next = current_block_->next;
        std::free(current_block_->data);
        delete current_block_;
        current_block_ = next;
    }
    if (current_block_) {
        current_block_->used = 0;
    }
    total_allocated_ = 0;
}

// =============================================================================
// ResolvedType Implementation
// =============================================================================

bool ResolvedType::isNumeric() const {
    switch (data_type) {
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::FLOAT32:
        case DataType::FLOAT64:
        case DataType::DECIMAL:
            return true;
        default:
            return false;
    }
}

bool ResolvedType::isString() const {
    switch (data_type) {
        case DataType::VARCHAR:
        case DataType::TEXT:
        case DataType::CHAR:
            return true;
        default:
            return false;
    }
}

bool ResolvedType::isBoolean() const {
    return data_type == DataType::BOOLEAN;
}

bool ResolvedType::isTemporal() const {
    switch (data_type) {
        case DataType::DATE:
        case DataType::TIME:
        case DataType::TIMESTAMP:
        case DataType::INTERVAL:
            return true;
        default:
            return false;
    }
}

bool ResolvedType::isComparableTo(const ResolvedType& other) const {
    if (data_type == other.data_type) {
        return true;
    }
    if (isNumeric() && other.isNumeric()) {
        return true;
    }
    if (isString() && other.isString()) {
        return true;
    }
    if (isTemporal() && other.isTemporal()) {
        return true;
    }
    return false;
}

bool ResolvedType::isAssignableTo(const ResolvedType& target) const {
    if (data_type == target.data_type) {
        return true;
    }
    if (isNumeric() && target.isNumeric()) {
        return true;
    }
    if (isString() && target.isString()) {
        return true;
    }
    return false;
}

// =============================================================================
// SemanticAnalyzerV2 Implementation
// =============================================================================

namespace {
    bool isZeroUuidLocal(const ID& id) {
        for (auto b : id.bytes) { if (b != 0) return false; }
        return true;
    }
}

SemanticAnalyzerV2::SemanticAnalyzerV2(CatalogManager& catalog, StringPool& string_pool)
    : catalog_(catalog)
    , string_pool_(string_pool)
{
    auto* conn_ctx = core::ConnectionContext::getCurrent();
    if (conn_ctx)
    {
        current_schema_ = conn_ctx->getCurrentSchemaId();
        if (isZeroUuidLocal(current_schema_) && !conn_ctx->current_schema().empty())
        {
            CatalogManager::SchemaInfo schema_info;
            if (catalog_.getSchema(conn_ctx->current_schema(), schema_info) == Status::OK)
            {
                current_schema_ = schema_info.schema_id;
            }
        }

        search_path_.clear();
        for (const auto& schema_path : conn_ctx->search_path())
        {
            CatalogManager::SchemaInfo schema_info;
            if (catalog_.getSchema(schema_path, schema_info) == Status::OK)
            {
                search_path_.push_back(schema_info.schema_id);
            }
        }

        if (search_path_.empty() && !isZeroUuidLocal(current_schema_))
        {
            search_path_.push_back(current_schema_);
        }
    }

    if (search_path_.empty())
    {
        // Initialize with default public schema
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema("public", schema_info) == Status::OK)
        {
            current_schema_ = schema_info.schema_id;
            search_path_.push_back(schema_info.schema_id);
        }
    }
}

SemanticAnalyzerV2::~SemanticAnalyzerV2() = default;

SemanticResult SemanticAnalyzerV2::analyze(Statement* stmt) {
    SemanticResult result;
    current_result_ = &result;
    result.setStringPool(&string_pool_);

    // Reset state
    scope_stack_.clear();
    in_aggregate_ = false;
    has_aggregates_ = false;
    subquery_depth_ = 0;

    // Analyze statement
    ResolvedStatement* resolved = analyzeStatement(stmt);
    result.setStatement(resolved);

    current_result_ = nullptr;
    return result;
}

// =============================================================================
// Error Handling
// =============================================================================

void SemanticAnalyzerV2::error(SourceSpan span, const std::string& message, const std::string& hint) {
    if (current_result_) {
        SemanticError err;
        err.span = span;
        err.message = message;
        err.hint = hint;
        err.severity = SemanticError::Severity::ERROR;
        current_result_->addError(err);
    }
}

void SemanticAnalyzerV2::warning(SourceSpan span, const std::string& message, const std::string& hint) {
    if (current_result_) {
        SemanticError warn;
        warn.span = span;
        warn.message = message;
        warn.hint = hint;
        warn.severity = SemanticError::Severity::WARNING;
        current_result_->addWarning(warn);
    }
}

// =============================================================================
// Scope Management
// =============================================================================

void SemanticAnalyzerV2::pushScope() {
    scope_stack_.emplace_back();
}

void SemanticAnalyzerV2::popScope() {
    if (!scope_stack_.empty()) {
        scope_stack_.pop_back();
    }
}

ResolutionScope& SemanticAnalyzerV2::currentScope() {
    if (scope_stack_.empty()) {
        scope_stack_.emplace_back();
    }
    return scope_stack_.back();
}

// =============================================================================
// Name Resolution
// =============================================================================

std::optional<ResolvedTableRef> SemanticAnalyzerV2::resolveTable(
    const SchemaPath& path, SourceSpan span, bool allow_search_path)
{
    if (path.components.empty())
    {
        error(span, "Invalid table reference: empty path");
        return std::nullopt;
    }

    std::vector<std::string> components;
    components.reserve(path.components.size());
    for (auto id : path.components)
    {
        components.emplace_back(string_pool_.get(id));
    }

    auto join_components = [&](size_t count) -> std::string {
        std::string out;
        for (size_t i = 0; i < count; ++i)
        {
            if (i > 0)
            {
                out += ".";
            }
            out += components[i];
        }
        return out;
    };

    auto is_object_resolver = [&](const std::vector<std::string>& comps) -> bool {
        if (comps.size() != 3)
        {
            return false;
        }
        return core::IdentifierUtils::toUpper(comps[0]) == "SYS" &&
               core::IdentifierUtils::toUpper(comps[1]) == "CATALOG" &&
               core::IdentifierUtils::toUpper(comps[2]) == "OBJECT_RESOLVER";
    };

    if (path.type == PathType::ABSOLUTE && is_object_resolver(components))
    {
        ResolvedTableRef ref;
        ref.table_uuid = ID{};
        ref.schema_uuid = ID{};
        ref.name = internString("sys.catalog.object_resolver");
        ref.object_type = ResolvedTableRef::ObjectType::VIEW;

        const std::array<const char*, 7> col_names = {
            "object_id",
            "object_type",
            "schema_path",
            "full_path",
            "object_name",
            "dialect_tag",
            "compat_name"
        };

        ref.columns.clear();
        ref.columns.reserve(col_names.size());
        uint32_t index = 0;
        for (const auto* col_name : col_names)
        {
            ResolvedTableRef::ColumnInfo col_info;
            col_info.name = internString(col_name);
            col_info.data_type = DataType::VARCHAR;
            col_info.is_nullable = true;
            col_info.column_index = index++;
            ref.columns.push_back(col_info);
        }

        return ref;
    }

    std::string table_name = components.back();
    std::string schema_path = components.size() > 1 ? join_components(components.size() - 1)
                                                    : std::string();

    CatalogManager::TableInfo table_info;
    Status status = Status::NOT_FOUND;

    bool search_path_allowed = allow_search_path && !path.no_search_path;

    if (path.type == PathType::UNQUALIFIED)
    {
        if (components.size() != 1)
        {
            error(span, "Invalid table reference: too many parts in path");
            return std::nullopt;
        }

        if (search_path_allowed)
        {
            for (const auto& schema_id : search_path_)
            {
                status = catalog_.getTable(schema_id, table_name, table_info);
                if (status == Status::OK)
                {
                    break;
                }
            }
        }
        if (status != Status::OK && !isZeroUuidLocal(current_schema_))
        {
            status = catalog_.getTable(current_schema_, table_name, table_info);
        }
        if (status != Status::OK && !search_path_allowed && isZeroUuidLocal(current_schema_))
        {
            error(span, "Current schema not set");
            return std::nullopt;
        }
    }
    else
    {
        CatalogManager::SchemaInfo schema_info;
        ID schema_id{};
        bool schema_id_resolved = false;
        std::string resolved_schema_path = schema_path;

        if (path.type == PathType::CURRENT)
        {
            if (schema_path.empty())
            {
                schema_id = current_schema_;
                schema_id_resolved = true;
            }
            else if (!isZeroUuidLocal(current_schema_))
            {
                std::string current_path;
                core::ErrorContext err_ctx;
                if (catalog_.getSchemaPath(current_schema_, current_path, &err_ctx) == Status::OK &&
                    !current_path.empty())
                {
                    resolved_schema_path = current_path + "." + schema_path;
                }
            }
        }
        else if (path.type == PathType::PARENT)
        {
            if (isZeroUuidLocal(current_schema_))
            {
                error(span, "Current schema not set for parent resolution");
                return std::nullopt;
            }

            CatalogManager::SchemaInfo current_info;
            if (catalog_.getSchema(current_schema_, current_info) != Status::OK ||
                isZeroUuidLocal(current_info.parent_schema_id))
            {
                error(span, "Current schema has no parent");
                return std::nullopt;
            }

            if (schema_path.empty())
            {
                schema_id = current_info.parent_schema_id;
                schema_id_resolved = true;
            }
            else
            {
                std::string parent_path;
                core::ErrorContext err_ctx;
                if (catalog_.getSchemaPath(current_info.parent_schema_id, parent_path, &err_ctx) == Status::OK &&
                    !parent_path.empty())
                {
                    resolved_schema_path = parent_path + "." + schema_path;
                }
            }
        }
        else if (path.type == PathType::ABSOLUTE && schema_path.empty())
        {
            error(span, "Invalid table reference: missing schema");
            return std::nullopt;
        }

        if (!schema_id_resolved)
        {
            status = catalog_.getSchema(resolved_schema_path, schema_info);
            if (status != Status::OK)
            {
                error(span, "Schema not found: " + resolved_schema_path);
                return std::nullopt;
            }
            schema_id = schema_info.schema_id;
        }

        status = catalog_.getTable(schema_id, table_name, table_info);
    }

    if (status != Status::OK)
    {
        error(span, "Table not found: " + table_name);
        return std::nullopt;
    }

    // Build resolved reference
    ResolvedTableRef ref;
    ref.table_uuid = table_info.table_id;
    ref.schema_uuid = table_info.schema_id;
    ref.name = internString(table_name);  // Store original name for v1 bytecode compat

    // Check if this is actually a view, not a table
    CatalogManager::ViewInfo view_info;
    if (catalog_.getViewById(table_info.table_id, view_info) == Status::OK) {
        // It's a view
        if (view_info.materialized) {
            ref.object_type = ResolvedTableRef::ObjectType::MATERIALIZED_VIEW;
        } else {
            ref.object_type = ResolvedTableRef::ObjectType::VIEW;
        }
    } else {
        // It's a real table
        ref.object_type = ResolvedTableRef::ObjectType::TABLE;
    }

    // Load columns
    loadTableColumns(ref);

    return ref;
}

bool SemanticAnalyzerV2::loadTableColumns(ResolvedTableRef& ref) {
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_.getColumns(ref.table_uuid, columns);

    if (status != Status::OK) {
        return false;
    }

    ref.columns.clear();
    ref.columns.reserve(columns.size());

    for (size_t i = 0; i < columns.size(); ++i) {
        ResolvedTableRef::ColumnInfo col_info;
        col_info.name = internString(columns[i].column_name);
        col_info.data_type = static_cast<DataType>(columns[i].data_type);
        col_info.is_nullable = columns[i].nullable;
        col_info.column_index = static_cast<uint32_t>(i);
        ref.columns.push_back(col_info);
    }

    return true;
}

std::optional<ResolvedColumnRef> SemanticAnalyzerV2::resolveColumn(
    StringPool::StringId table_alias,
    StringPool::StringId column_name,
    SourceSpan span)
{
    if (scope_stack_.empty()) {
        error(span, "No tables in scope for column resolution");
        return std::nullopt;
    }

    if (table_alias != StringPool::INVALID_ID) {
        // Qualified column reference
        const auto* col = currentScope().findColumn(table_alias, column_name);
        if (!col) {
            std::string col_str = std::string(getString(column_name));
            std::string table_str = std::string(getString(table_alias));
            error(span, "Column not found: " + table_str + "." + col_str);
            return std::nullopt;
        }

        const auto* table = currentScope().findTable(table_alias);
        ResolvedColumnRef ref;
        ref.table_uuid = table->table_uuid;
        ref.column_index = col->column_index;
        ref.data_type = col->data_type;
        ref.is_nullable = col->is_nullable;
        ref.column_name = column_name;
        ref.table_alias = table_alias;
        return ref;
    }

    // Unqualified - search all tables
    auto result = currentScope().findColumn(column_name);
    if (result.ambiguous) {
        error(span, "Ambiguous column reference: " + std::string(getString(column_name)));
        return std::nullopt;
    }
    if (!result.column) {
        error(span, "Column not found: " + std::string(getString(column_name)));
        return std::nullopt;
    }

    ResolvedColumnRef ref;
    ref.table_uuid = result.table->table_uuid;
    ref.column_index = result.column->column_index;
    ref.data_type = result.column->data_type;
    ref.is_nullable = result.column->is_nullable;
    ref.column_name = column_name;
    ref.table_alias = result.table->alias;
    return ref;
}

std::optional<ResolvedFunctionRef> SemanticAnalyzerV2::resolveFunction(
    const SchemaPath& path,
    const std::vector<ResolvedType>& arg_types,
    SourceSpan span)
{
    if (path.components.empty()) {
        error(span, "Empty function name");
        return std::nullopt;
    }

    std::string func_name;
    if (path.components.size() == 1) {
        func_name = std::string(string_pool_.get(path.components[0]));
    } else {
        func_name = std::string(string_pool_.get(path.components.back()));
        // Try package dependency on prefix
        std::string pkg_name = std::string(string_pool_.get(path.components.front()));
        core::CatalogManager::PackageInfo pkg;
        core::ErrorContext ctx;
        if (!isZeroUuidLocal(current_schema_) &&
            catalog_.getPackageByName(current_schema_, pkg_name, pkg, &ctx) == Status::OK) {
            if (current_result_) {
                current_result_->addDependency(pkg.package_id, core::CatalogManager::ObjectType::PACKAGE);
            }
        }
    }

    ResolvedFunctionRef ref;
    ref.function_uuid = ID{};  // Zero UUID for built-in
    ref.function_name = path.components.back();
    ref.is_builtin = true;

    // Create return type in arena
    auto* ret_type = arena_.create<ResolvedType>();

    // Determine function type and return type based on name
    std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::tolower);

    // Aggregate functions
    if (func_name == "count" || func_name == "sum" || func_name == "avg" ||
        func_name == "min" || func_name == "max") {
        ref.is_aggregate = true;

        if (func_name == "count") {
            ret_type->data_type = DataType::INT64;
        } else if (func_name == "avg") {
            ret_type->data_type = DataType::FLOAT64;
        } else if (!arg_types.empty()) {
            *ret_type = arg_types[0];
        }
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    // String functions
    if (func_name == "length" || func_name == "char_length" || func_name == "octet_length") {
        ret_type->data_type = DataType::INT32;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "upper" || func_name == "lower" || func_name == "trim" ||
        func_name == "ltrim" || func_name == "rtrim" || func_name == "substring") {
        ret_type->data_type = DataType::VARCHAR;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "concat" || func_name == "concat_ws") {
        ret_type->data_type = DataType::VARCHAR;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    // Date/time functions
    if (func_name == "now" || func_name == "current_timestamp") {
        ret_type->data_type = DataType::TIMESTAMP;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "current_date") {
        ret_type->data_type = DataType::DATE;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "current_time") {
        ret_type->data_type = DataType::TIME;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    // Numeric functions
    if (func_name == "abs" || func_name == "ceil" || func_name == "floor" ||
        func_name == "round" || func_name == "trunc") {
        if (!arg_types.empty()) {
            *ret_type = arg_types[0];
        } else {
            ret_type->data_type = DataType::FLOAT64;
        }
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "sqrt" || func_name == "log" || func_name == "ln" ||
        func_name == "exp" || func_name == "power" || func_name == "sin" ||
        func_name == "cos" || func_name == "tan") {
        ret_type->data_type = DataType::FLOAT64;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    // Coalesce/nullif
    if (func_name == "coalesce" || func_name == "nullif") {
        if (!arg_types.empty()) {
            *ret_type = arg_types[0];
        }
        ref.return_type = ret_type;
        return ref;
    }

    // Not a built-in: try catalog-resolved function/procedure/UDR
    core::ErrorContext ctx;
    core::CatalogManager::FunctionInfo fi;
    if (catalog_.getFunction(func_name, fi, &ctx) == Status::OK) {
        ref.function_uuid = fi.function_id;
        ref.is_builtin = false;
        ret_type->data_type = fi.return_type;
        ret_type->precision = static_cast<int32_t>(fi.return_type_precision);
        ret_type->scale = static_cast<int32_t>(fi.return_type_scale);
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        if (current_result_) {
            current_result_->addDependency(fi.function_id, core::CatalogManager::ObjectType::FUNCTION);
        }
        return ref;
    }

    core::CatalogManager::ProcedureInfo pi;
    if (catalog_.getProcedure(func_name, pi, &ctx) == Status::OK) {
        ref.function_uuid = pi.procedure_id;
        ref.is_builtin = false;
        ret_type->data_type = DataType::UNKNOWN;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        if (current_result_) {
            current_result_->addDependency(pi.procedure_id, core::CatalogManager::ObjectType::PROCEDURE);
        }
        return ref;
    }

    core::CatalogManager::UDRInfo ui;
    if (catalog_.getUDRByName(current_schema_, func_name, ui, &ctx) == Status::OK) {
        ref.function_uuid = ui.udr_id;
        ref.is_builtin = false;
        ret_type->data_type = DataType::UNKNOWN;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        if (current_result_) {
            current_result_->addDependency(ui.udr_id, core::CatalogManager::ObjectType::UDR);
        }
        return ref;
    }

    // Unknown function - return UNKNOWN type
    ret_type->data_type = DataType::UNKNOWN;
    ref.return_type = ret_type;
    warning(span, "Unknown function: " + func_name + " - assuming returns UNKNOWN type");
    return ref;
}

// =============================================================================
// Type Checking
// =============================================================================

std::optional<ResolvedType> SemanticAnalyzerV2::getCommonType(
    const ResolvedType& left,
    const ResolvedType& right,
    BinaryOp op)
{
    // Handle NULL (UNKNOWN) type - NULL adopts the type of the other operand
    // For any operation with NULL, the result type comes from the non-NULL side
    if (left.data_type == DataType::UNKNOWN && right.data_type != DataType::UNKNOWN) {
        ResolvedType result = right;
        result.is_nullable = true;  // NULL makes result nullable
        return result;
    }
    if (right.data_type == DataType::UNKNOWN && left.data_type != DataType::UNKNOWN) {
        ResolvedType result = left;
        result.is_nullable = true;  // NULL makes result nullable
        return result;
    }
    if (left.data_type == DataType::UNKNOWN && right.data_type == DataType::UNKNOWN) {
        // Both are NULL - result is NULL with UNKNOWN type (or BOOLEAN for comparisons)
        ResolvedType result;
        if (op == BinaryOp::EQ || op == BinaryOp::NE || op == BinaryOp::LT ||
            op == BinaryOp::LE || op == BinaryOp::GT || op == BinaryOp::GE) {
            result.data_type = DataType::BOOLEAN;
        } else {
            result.data_type = DataType::UNKNOWN;
        }
        result.is_nullable = true;
        return result;
    }

    // Same type - use it
    if (left.data_type == right.data_type) {
        ResolvedType result = left;
        result.is_nullable = left.is_nullable || right.is_nullable;
        return result;
    }

    // Comparison operators result in boolean
    switch (op) {
        case BinaryOp::EQ:
        case BinaryOp::NE:
        case BinaryOp::LT:
        case BinaryOp::LE:
        case BinaryOp::GT:
        case BinaryOp::GE:
            if (left.isComparableTo(right)) {
                ResolvedType result;
                result.data_type = DataType::BOOLEAN;
                result.is_nullable = left.is_nullable || right.is_nullable;
                return result;
            }
            return std::nullopt;

        case BinaryOp::AND:
        case BinaryOp::OR:
            if (left.isBoolean() && right.isBoolean()) {
                ResolvedType result;
                result.data_type = DataType::BOOLEAN;
                result.is_nullable = left.is_nullable || right.is_nullable;
                return result;
            }
            return std::nullopt;

        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::MOD:
            if (left.isNumeric() && right.isNumeric()) {
                ResolvedType result;
                result.is_nullable = left.is_nullable || right.is_nullable;

                if (left.data_type == DataType::FLOAT64 || right.data_type == DataType::FLOAT64) {
                    result.data_type = DataType::FLOAT64;
                } else if (left.data_type == DataType::FLOAT32 || right.data_type == DataType::FLOAT32) {
                    result.data_type = DataType::FLOAT32;
                } else if (left.data_type == DataType::DECIMAL || right.data_type == DataType::DECIMAL) {
                    result.data_type = DataType::DECIMAL;
                } else if (left.data_type == DataType::INT64 || right.data_type == DataType::INT64) {
                    result.data_type = DataType::INT64;
                } else if (left.data_type == DataType::INT32 || right.data_type == DataType::INT32) {
                    result.data_type = DataType::INT32;
                } else {
                    result.data_type = DataType::INT16;
                }
                return result;
            }
            return std::nullopt;

        case BinaryOp::CONCAT:
            if (left.isString() && right.isString()) {
                ResolvedType result;
                result.data_type = DataType::VARCHAR;
                result.is_nullable = left.is_nullable || right.is_nullable;
                return result;
            }
            return std::nullopt;

        default:
            break;
    }

    return std::nullopt;
}

bool SemanticAnalyzerV2::canImplicitCast(const ResolvedType& from, const ResolvedType& to) {
    if (from.data_type == to.data_type) {
        return true;
    }
    if (from.isNumeric() && to.isNumeric()) {
        return true;
    }
    if (from.isString() && to.isString()) {
        return true;
    }
    return false;
}

ResolvedExpression* SemanticAnalyzerV2::insertImplicitCast(
    ResolvedExpression* expr,
    const ResolvedType& target_type)
{
    if (expr->type.data_type == target_type.data_type) {
        return expr;
    }

    auto* cast = arena_.create<ResolvedCast>();
    cast->expr = expr;
    cast->target_type = target_type;
    cast->type = target_type;
    cast->implicit = true;
    cast->span = expr->span;
    return cast;
}

// =============================================================================
// Statement Analysis
// =============================================================================

ResolvedStatement* SemanticAnalyzerV2::analyzeStatement(Statement* stmt) {
    if (!stmt) {
        return nullptr;
    }

    switch (stmt->kind()) {
        // DDL
        case ASTKind::CreateTableStmt:
            return analyzeCreateTable(static_cast<CreateTableStmt*>(stmt));
        case ASTKind::CreateIndexStmt:
            return analyzeCreateIndex(static_cast<CreateIndexStmt*>(stmt));
        case ASTKind::CreateViewStmt:
            return analyzeCreateView(static_cast<CreateViewStmt*>(stmt));
        case ASTKind::CreateSchemaStmt:
            return analyzeCreateSchema(static_cast<CreateSchemaStmt*>(stmt));
        case ASTKind::DropSchemaStmt:
            return analyzeDropSchema(static_cast<DropSchemaStmt*>(stmt));
        case ASTKind::AlterSchemaStmt:
            return analyzeAlterSchema(static_cast<AlterSchemaStmt*>(stmt));
        case ASTKind::CreateDatabaseStmt:
            return analyzeCreateDatabase(static_cast<CreateDatabaseStmt*>(stmt));
        case ASTKind::DropDatabaseStmt:
            return analyzeDropDatabase(static_cast<DropDatabaseStmt*>(stmt));
        case ASTKind::AlterDatabaseStmt:
            return analyzeAlterDatabase(static_cast<AlterDatabaseStmt*>(stmt));
        case ASTKind::AlterTableStmt:
            return analyzeAlterTable(static_cast<AlterTableStmt*>(stmt));
        case ASTKind::RenameObjectStmt:
            return analyzeRenameObject(static_cast<RenameObjectStmt*>(stmt));
        case ASTKind::MoveObjectStmt:
            return analyzeMoveObject(static_cast<MoveObjectStmt*>(stmt));
        case ASTKind::DropTableStmt:
            return analyzeDropTable(static_cast<DropTableStmt*>(stmt));
        case ASTKind::DropIndexStmt:
            return analyzeDropIndex(static_cast<DropIndexStmt*>(stmt));
        case ASTKind::DropViewStmt:
            return analyzeDropView(static_cast<DropViewStmt*>(stmt));
        case ASTKind::TruncateTableStmt:
            return analyzeTruncateTable(static_cast<TruncateTableStmt*>(stmt));

        // DML
        case ASTKind::SelectStmt:
            return analyzeSelect(static_cast<SelectStmt*>(stmt));
        case ASTKind::InsertStmt:
            return analyzeInsert(static_cast<InsertStmt*>(stmt));
        case ASTKind::UpdateStmt:
            return analyzeUpdate(static_cast<UpdateStmt*>(stmt));
        case ASTKind::DeleteStmt:
            return analyzeDelete(static_cast<DeleteStmt*>(stmt));

        // Transaction
        case ASTKind::StartTransactionStmt:
            return analyzeStartTransaction(static_cast<StartTransactionStmt*>(stmt));
        case ASTKind::PrepareTransactionStmt:
            return analyzePrepareTransaction(static_cast<PrepareTransactionStmt*>(stmt));
        case ASTKind::CommitStmt:
            return analyzeCommit(static_cast<CommitStmt*>(stmt));
        case ASTKind::RollbackStmt:
            return analyzeRollback(static_cast<RollbackStmt*>(stmt));
        case ASTKind::SavepointStmt:
            return analyzeSavepoint(static_cast<SavepointStmt*>(stmt));
        case ASTKind::ReleaseSavepointStmt:
            return analyzeReleaseSavepoint(static_cast<ReleaseSavepointStmt*>(stmt));

        // Session
        case ASTKind::SetStmt:
            return analyzeSet(static_cast<SetStmt*>(stmt));
        case ASTKind::ShowStmt:
            return analyzeShow(static_cast<ShowStmt*>(stmt));
        case ASTKind::ExplainStmt:
            return analyzeExplain(static_cast<ExplainStmt*>(stmt));

        default:
            error(stmt->span, "Unsupported statement type for semantic analysis");
            return nullptr;
    }
}

// =============================================================================
// Transaction/Session Statement Analysis
// =============================================================================

ResolvedStatement* SemanticAnalyzerV2::analyzeStartTransaction(StartTransactionStmt* stmt) {
    auto* resolved = arena_.create<ResolvedStartTransactionStmt>();
    resolved->span = stmt->span;
    resolved->has_isolation_level = stmt->has_isolation_level;
    resolved->isolation_level = stmt->isolation_level;
    resolved->has_access_mode = stmt->has_access_mode;
    resolved->access_mode = stmt->access_mode;
    resolved->has_read_committed_mode = stmt->has_read_committed_mode;
    resolved->read_committed_mode = stmt->read_committed_mode;
    resolved->has_deferrable = stmt->deferrable || stmt->not_deferrable;
    resolved->deferrable = stmt->deferrable;
    resolved->has_wait_mode = stmt->has_wait_mode;
    resolved->wait_mode = stmt->wait_mode;
    resolved->has_lock_timeout = stmt->has_lock_timeout;
    resolved->lock_timeout_seconds = stmt->lock_timeout_seconds;
    resolved->table_reservations = stmt->table_reservations;
    resolved->has_autocommit = stmt->has_autocommit;
    resolved->autocommit_mode = stmt->autocommit_mode;
    resolved->conflict_action = stmt->conflict_action;
    resolved->has_conflict_error_code = stmt->has_conflict_error_code;
    resolved->conflict_error_code = stmt->conflict_error_code;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzePrepareTransaction(PrepareTransactionStmt* stmt) {
    auto* resolved = arena_.create<ResolvedPrepareTransactionStmt>();
    resolved->span = stmt->span;
    resolved->gid = stmt->gid;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCommit(CommitStmt* stmt) {
    auto* resolved = arena_.create<ResolvedCommitStmt>();
    resolved->span = stmt->span;
    resolved->and_chain = stmt->and_chain;
    resolved->and_no_chain = stmt->and_no_chain;
    resolved->retaining = stmt->retaining;
    resolved->is_prepared = stmt->is_prepared;
    resolved->prepared_gid = stmt->prepared_gid;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeRollback(RollbackStmt* stmt) {
    auto* resolved = arena_.create<ResolvedRollbackStmt>();
    resolved->span = stmt->span;
    resolved->to_savepoint = stmt->to_savepoint;
    resolved->savepoint_name = stmt->savepoint_name;
    resolved->and_chain = stmt->and_chain;
    resolved->and_no_chain = stmt->and_no_chain;
    resolved->retaining = stmt->retaining;
    resolved->is_prepared = stmt->is_prepared;
    resolved->prepared_gid = stmt->prepared_gid;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeSavepoint(SavepointStmt* stmt) {
    auto* resolved = arena_.create<ResolvedSavepointStmt>();
    resolved->span = stmt->span;
    resolved->name = stmt->name;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeReleaseSavepoint(ReleaseSavepointStmt* stmt) {
    auto* resolved = arena_.create<ResolvedSavepointStmt>();
    resolved->span = stmt->span;
    resolved->name = stmt->name;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeSet(SetStmt* stmt) {
    auto* resolved = arena_.create<ResolvedSetStmt>();
    resolved->span = stmt->span;
    resolved->set_type = stmt->set_type;
    resolved->scope = stmt->scope;
    resolved->variable_name = stmt->name;
    resolved->is_default = stmt->is_default;

    if (stmt->value) {
        resolved->value = analyzeExpression(stmt->value);
    }

    resolved->has_isolation_level = stmt->has_isolation_level;
    resolved->isolation_level = stmt->isolation_level;
    resolved->has_access_mode = stmt->has_access_mode;
    resolved->access_mode = stmt->access_mode;
    resolved->has_read_committed_mode = stmt->has_read_committed_mode;
    resolved->read_committed_mode = stmt->read_committed_mode;
    resolved->has_deferrable = stmt->deferrable || stmt->not_deferrable;
    resolved->deferrable = stmt->deferrable;
    resolved->has_wait_mode = stmt->has_wait_mode;
    resolved->wait_mode = stmt->wait_mode;
    resolved->has_lock_timeout = stmt->has_lock_timeout;
    resolved->lock_timeout_seconds = stmt->lock_timeout_seconds;
    resolved->table_reservations = stmt->table_reservations;
    resolved->has_autocommit = stmt->has_autocommit;
    resolved->autocommit_mode = stmt->autocommit_mode;
    resolved->conflict_action = stmt->conflict_action;
    resolved->has_conflict_error_code = stmt->has_conflict_error_code;
    resolved->conflict_error_code = stmt->conflict_error_code;

    // For SET PARSER VERSION
    resolved->parser_version = stmt->parser_version;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeShow(ShowStmt* stmt) {
    auto* resolved = arena_.create<ResolvedShowStmt>();
    resolved->span = stmt->span;
    resolved->show_type = stmt->show_type;
    resolved->variable_name = stmt->name;
    resolved->from_name = stmt->from_name;
    resolved->like_pattern = stmt->like_pattern;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeExplain(ExplainStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedExplainStmt>();
    resolved->span = stmt->span;
    resolved->analyze = stmt->analyze;
    resolved->verbose = stmt->verbose;
    resolved->costs = stmt->costs;
    resolved->buffers = stmt->buffers;
    resolved->timing = stmt->timing;
    resolved->format_json = stmt->format_json;
    resolved->format_xml = stmt->format_xml;
    resolved->format_yaml = stmt->format_yaml;

    // Analyze the query to explain
    if (stmt->query) {
        resolved->query = analyzeStatement(stmt->query);
        if (!resolved->query) {
            // Query analysis failed - error already reported
            return nullptr;
        }
    } else {
        error(stmt->span, "EXPLAIN requires a query to explain");
        return nullptr;
    }

    return resolved;
}

// =============================================================================
// DDL Statement Analysis
// =============================================================================

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateTable(CreateTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateTableStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;

    // Resolve schema from table path
    if (stmt->table_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->table_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->table_path.components[0];
        }
        resolved->table_name = stmt->table_path.components[1];
    } else if (stmt->table_path.components.size() == 1) {
        // Use current schema
        resolved->schema.schema_uuid = current_schema_;
        resolved->table_name = stmt->table_path.components[0];
    }

    // Analyze column definitions
    for (auto* col_def : stmt->columns) {
        ResolvedColumnDef resolved_col = analyzeColumnDef(col_def);
        resolved->columns.push_back(resolved_col);
    }

    // Analyze table constraints
    for (auto* constraint : stmt->constraints) {
        ResolvedTableConstraint resolved_constraint = analyzeTableConstraint(constraint, resolved->columns);
        resolved->constraints.push_back(resolved_constraint);
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateIndex(CreateIndexStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateIndexStmt>();
    resolved->span = stmt->span;
    resolved->index_name = stmt->index_name;
    resolved->unique = stmt->unique;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->concurrent = stmt->concurrent;

    // Resolve table
    auto table_ref = resolveTable(stmt->table_path, stmt->span, false);
    if (!table_ref) {
        return nullptr;
    }
    resolved->table_uuid = table_ref->table_uuid;

    // Map index type to string
    switch (stmt->index_type) {
        case IndexType::BTREE: resolved->index_method = internString("btree"); break;
        case IndexType::HASH: resolved->index_method = internString("hash"); break;
        case IndexType::GIN: resolved->index_method = internString("gin"); break;
        case IndexType::GIST: resolved->index_method = internString("gist"); break;
        case IndexType::BRIN: resolved->index_method = internString("brin"); break;
        case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;
    }

    // Resolve index columns
    for (const auto& idx_col : stmt->columns) {
        if (idx_col.column != StringPool::INVALID_ID) {
            // Named column
            bool found = false;
            for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                if (table_ref->columns[i].name == idx_col.column) {
                    resolved->column_indexes.push_back(i);
                    resolved->column_desc.push_back(!idx_col.ascending);
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "Index column not found: " + std::string(getString(idx_col.column)));
            }
        } else if (idx_col.expr) {
            // Expression index - we'd need to store the expression
            // For now, just note it's an expression index
            warning(stmt->span, "Expression indexes not fully supported yet");
        }
    }

    // Analyze WHERE clause for partial index
    if (stmt->where_clause) {
        // Push table scope for WHERE analysis
        pushScope();
        ResolutionScope::TableEntry entry;
        entry.table_uuid = table_ref->table_uuid;
        entry.columns = table_ref->columns;
        currentScope().addTable(entry);

        resolved->where_clause = analyzeExpression(stmt->where_clause);

        popScope();
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateView(CreateViewStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateViewStmt>();
    resolved->span = stmt->span;
    resolved->or_replace = stmt->or_replace;
    resolved->materialized = stmt->materialized;

    // Resolve schema from view path
    if (stmt->view_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->view_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->view_path.components[0];
        }
        resolved->view_name = stmt->view_path.components[1];
    } else if (stmt->view_path.components.size() == 1) {
        resolved->schema.schema_uuid = current_schema_;
        resolved->view_name = stmt->view_path.components[0];
    }

    // Copy column names
    resolved->column_names = stmt->column_names;

    // Analyze the view query
    if (stmt->query) {
        resolved->query = analyzeSelect(static_cast<SelectStmt*>(stmt->query));
    }

    resolved->check_option = stmt->with_check_option;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateSchema(CreateSchemaStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateSchemaStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->schema_path = stmt->schema_path;
    resolved->owner = stmt->has_owner ? stmt->owner : StringPool::INVALID_ID;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropSchema(DropSchemaStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropSchemaStmt>();
    resolved->span = stmt->span;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;
    resolved->restrict = stmt->restrict;
    resolved->schema_paths = stmt->schemas;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateDatabase(CreateDatabaseStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateDatabaseStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->database_path = stmt->database_path;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropDatabase(DropDatabaseStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropDatabaseStmt>();
    resolved->span = stmt->span;
    resolved->if_exists = stmt->if_exists;
    resolved->force = stmt->force;
    resolved->database_path = stmt->database_path;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterSchema(AlterSchemaStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedAlterSchemaStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->schema_path = stmt->schema_path;
    resolved->new_name = stmt->new_name;
    resolved->owner = stmt->owner;
    resolved->new_path = stmt->new_path;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterDatabase(AlterDatabaseStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedAlterDatabaseStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->database_path = stmt->database_path;
    resolved->new_name = stmt->new_name;
    resolved->owner = stmt->owner;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeRenameObject(RenameObjectStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedRenameObjectStmt>();
    resolved->span = stmt->span;
    resolved->object_type = stmt->object_type;
    resolved->if_exists = stmt->if_exists;
    resolved->object_path = stmt->object_path;
    resolved->new_name = stmt->new_name;
    resolved->has_uuid = false;

    core::ObjectPath obj_path = buildObjectPath(stmt->object_path, string_pool_);
    core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
    core::ErrorContext err_ctx;
    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    Status status = catalog_.resolveObjectPath(
        obj_path,
        toCatalogObjectType(stmt->object_type),
        opts,
        resolved->object_uuid,
        resolved_type,
        &err_ctx);

    if (status == Status::OK) {
        resolved->has_uuid = true;
    } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
        std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeMoveObject(MoveObjectStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedMoveObjectStmt>();
    resolved->span = stmt->span;
    resolved->object_type = stmt->object_type;
    resolved->if_exists = stmt->if_exists;
    resolved->object_path = stmt->object_path;
    resolved->target_schema = stmt->target_schema;
    resolved->has_new_name = stmt->has_new_name;
    resolved->new_name = stmt->new_name;
    resolved->has_uuid = false;

    core::ObjectPath obj_path = buildObjectPath(stmt->object_path, string_pool_);
    core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
    core::ErrorContext err_ctx;
    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    Status status = catalog_.resolveObjectPath(
        obj_path,
        toCatalogObjectType(stmt->object_type),
        opts,
        resolved->object_uuid,
        resolved_type,
        &err_ctx);

    if (status == Status::OK) {
        resolved->has_uuid = true;
    } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
        std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterTable(AlterTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto resolve_rename = [&](DdlObjectType type,
                              const SchemaPath& path,
                              StringPool::StringId new_name) -> ResolvedStatement* {
        auto* resolved = arena_.create<ResolvedRenameObjectStmt>();
        resolved->span = stmt->span;
        resolved->object_type = type;
        resolved->if_exists = stmt->if_exists;
        resolved->object_path = path;
        resolved->new_name = new_name;
        resolved->has_uuid = false;

        core::ObjectPath obj_path = buildObjectPath(path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::ErrorContext err_ctx;
        core::CatalogManager::ResolveOptions opts;
        opts.allow_search_path = false;
        Status status = catalog_.resolveObjectPath(
            obj_path,
            toCatalogObjectType(type),
            opts,
            resolved->object_uuid,
            resolved_type,
            &err_ctx);

        if (status == Status::OK) {
            resolved->has_uuid = true;
        } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
            std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
            error(stmt->span, msg);
            return nullptr;
        }

        return resolved;
    };

    auto resolve_move = [&](const SchemaPath& path,
                            const SchemaPath& target_schema) -> ResolvedStatement* {
        auto* resolved = arena_.create<ResolvedMoveObjectStmt>();
        resolved->span = stmt->span;
        resolved->object_type = DdlObjectType::TABLE;
        resolved->if_exists = stmt->if_exists;
        resolved->object_path = path;
        resolved->target_schema = target_schema;
        resolved->has_uuid = false;

        core::ObjectPath obj_path = buildObjectPath(path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::ErrorContext err_ctx;
        core::CatalogManager::ResolveOptions opts;
        opts.allow_search_path = false;
        Status status = catalog_.resolveObjectPath(
            obj_path,
            toCatalogObjectType(DdlObjectType::TABLE),
            opts,
            resolved->object_uuid,
            resolved_type,
            &err_ctx);

        if (status == Status::OK) {
            resolved->has_uuid = true;
        } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
            std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
            error(stmt->span, msg);
            return nullptr;
        }

        return resolved;
    };

    switch (stmt->action) {
        case AlterTableAction::RENAME_TABLE:
            return resolve_rename(DdlObjectType::TABLE, stmt->table_path, stmt->new_name);
        case AlterTableAction::RENAME_COLUMN: {
            SchemaPath full_path = appendPathComponent(stmt->table_path, stmt->column_name, stmt->span);
            return resolve_rename(DdlObjectType::COLUMN, full_path, stmt->new_name);
        }
        case AlterTableAction::RENAME_CONSTRAINT: {
            SchemaPath full_path = appendPathComponent(stmt->table_path, stmt->constraint_name, stmt->span);
            return resolve_rename(DdlObjectType::CONSTRAINT, full_path, stmt->new_name);
        }
        case AlterTableAction::SET_SCHEMA:
            return resolve_move(stmt->table_path, stmt->target_schema);
        default:
            break;
    }

    auto table_ref = resolveTable(stmt->table_path, stmt->span, false);
    if (!table_ref) {
        return nullptr;
    }

    if (table_ref->object_type != ResolvedTableRef::ObjectType::TABLE) {
        error(stmt->span, "ALTER TABLE requires a base table");
        return nullptr;
    }

    core::ErrorContext err_ctx;
    std::string schema_path;
    if (catalog_.getSchemaPath(table_ref->schema_uuid, schema_path, &err_ctx) != Status::OK) {
        std::string msg = err_ctx.message.empty() ? "Failed to resolve schema path" : err_ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    std::string table_name = std::string(getString(table_ref->name));
    std::string display_schema_path = stripRootPrefixForDisplay(schema_path);
    std::string qualified_name =
        display_schema_path.empty() ? table_name : display_schema_path + "." + table_name;

    if (stmt->only) {
        warning(stmt->span, "ALTER TABLE ONLY is not supported");
    }
    if (stmt->if_exists) {
        warning(stmt->span, "ALTER TABLE IF EXISTS is not enforced at bytecode level");
    }

    auto* resolved = arena_.create<ResolvedAlterTableStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->if_exists = stmt->if_exists;
    resolved->only = stmt->only;
    resolved->cascade = stmt->cascade;
    resolved->table_uuid = table_ref->table_uuid;
    resolved->schema_uuid = table_ref->schema_uuid;
    resolved->table_name = table_ref->name;
    resolved->qualified_table_name = internString(qualified_name);

    switch (stmt->action) {
        case AlterTableAction::ADD_COLUMN: {
            if (!stmt->column) {
                error(stmt->span, "ALTER TABLE ADD COLUMN requires a column definition");
                return nullptr;
            }
            if (stmt->column->is_computed || stmt->column->computed_expr) {
                error(stmt->span, "ALTER TABLE ADD COLUMN does not support computed columns");
                return nullptr;
            }
            for (const auto& constraint : stmt->column->constraints) {
                if (constraint.type == ConstraintType::NOT_NULL ||
                    constraint.type == ConstraintType::NULL_ALLOWED) {
                    continue;
                }
                error(stmt->span, "ALTER TABLE ADD COLUMN supports only NULL/NOT NULL constraints");
                return nullptr;
            }

            ResolvedColumnDef col_def = analyzeColumnDef(stmt->column);
            if (col_def.name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE ADD COLUMN requires a column name");
                return nullptr;
            }
            if (col_def.type.data_type == DataType::UNKNOWN) {
                error(stmt->span, "ALTER TABLE ADD COLUMN has unsupported data type");
                return nullptr;
            }
            if (col_def.default_value || col_def.check_expr || col_def.is_primary_key ||
                col_def.is_unique || col_def.has_fk) {
                error(stmt->span, "ALTER TABLE ADD COLUMN supports only type and NULL/NOT NULL");
                return nullptr;
            }

            core::CatalogManager::ColumnInfo existing;
            if (catalog_.getColumn(table_ref->table_uuid, std::string(getString(col_def.name)),
                                   existing, &err_ctx) == Status::OK) {
                error(stmt->span, "Column already exists: " + std::string(getString(col_def.name)));
                return nullptr;
            }

            resolved->column_def = col_def;
            resolved->has_column_def = true;
            resolved->column_name = col_def.name;
            return resolved;
        }
        case AlterTableAction::DROP_COLUMN: {
            if (stmt->column_name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE DROP COLUMN requires a column name");
                return nullptr;
            }

            core::CatalogManager::ColumnInfo existing;
            if (catalog_.getColumn(table_ref->table_uuid, std::string(getString(stmt->column_name)),
                                   existing, &err_ctx) != Status::OK) {
                error(stmt->span, "Column not found: " + std::string(getString(stmt->column_name)));
                return nullptr;
            }

            resolved->column_name = stmt->column_name;
            return resolved;
        }
        case AlterTableAction::ALTER_COLUMN: {
            if (!stmt->column && stmt->column_name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN requires a column name");
                return nullptr;
            }

            StringPool::StringId col_name = stmt->column_name;
            if (col_name == StringPool::INVALID_ID && stmt->column) {
                col_name = stmt->column->name;
            }
            if (col_name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN requires a column name");
                return nullptr;
            }

            core::CatalogManager::ColumnInfo existing;
            if (catalog_.getColumn(table_ref->table_uuid, std::string(getString(col_name)),
                                   existing, &err_ctx) != Status::OK) {
                error(stmt->span, "Column not found: " + std::string(getString(col_name)));
                return nullptr;
            }

            if (!stmt->column) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN requires a type definition");
                return nullptr;
            }
            if (stmt->column->is_computed || stmt->column->computed_expr) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN does not support computed columns");
                return nullptr;
            }
            if (!stmt->column->constraints.empty()) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN supports only type changes");
                return nullptr;
            }

            ResolvedColumnDef col_def = analyzeColumnDef(stmt->column);
            if (col_def.type.data_type == DataType::UNKNOWN) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN has unsupported data type");
                return nullptr;
            }
            if (col_def.default_value || col_def.check_expr || col_def.is_primary_key ||
                col_def.is_unique || col_def.has_fk) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN supports only type changes");
                return nullptr;
            }

            col_def.name = col_name;
            resolved->column_def = col_def;
            resolved->has_column_def = true;
            resolved->column_name = col_name;
            return resolved;
        }
        case AlterTableAction::SET_TABLESPACE: {
            if (stmt->tablespace.components.empty()) {
                error(stmt->span, "ALTER TABLE SET TABLESPACE requires a tablespace name");
                return nullptr;
            }
            if (stmt->tablespace.components.size() > 1) {
                warning(stmt->span, "Tablespace paths are global; using the final component");
            }
            resolved->tablespace_name = stmt->tablespace.components.back();
            resolved->tablespace_online = false;
            return resolved;
        }
        case AlterTableAction::ENABLE_RLS:
            resolved->rls_action = 0;
            return resolved;
        case AlterTableAction::DISABLE_RLS:
            resolved->rls_action = 1;
            return resolved;
        case AlterTableAction::ADD_CONSTRAINT:
        case AlterTableAction::DROP_CONSTRAINT:
            error(stmt->span, "ALTER TABLE constraint operations are not supported");
            return nullptr;
        default:
            break;
    }

    warning(stmt->span, "ALTER TABLE semantic analysis not fully implemented");
    return nullptr;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropTable(DropTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::TABLE;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    // Resolve each table
    for (const auto& table_path : stmt->tables) {
        auto table_ref = resolveTable(table_path, stmt->span, false);
        if (table_ref) {
            resolved->object_uuids.push_back(table_ref->table_uuid);
        } else if (!stmt->if_exists) {
            // Error already reported by resolveTable
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropIndex(DropIndexStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::INDEX;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    // Index resolution would require looking up indexes in catalog
    // For now, we just note the index names
    for (const auto& index_path : stmt->indexes) {
        if (!index_path.components.empty()) {
            // Would need catalog_.getIndex() or similar
            warning(stmt->span, "Index resolution not fully implemented");
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropView(DropViewStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::VIEW;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    // View resolution - views are treated like tables in catalog
    for (const auto& view_path : stmt->views) {
        auto view_ref = resolveTable(view_path, stmt->span, false);
        if (view_ref) {
            resolved->object_uuids.push_back(view_ref->table_uuid);
        } else if (!stmt->if_exists) {
            // Error already reported
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeTruncateTable(TruncateTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedTruncateTableStmt>();
    resolved->span = stmt->span;
    resolved->cascade = stmt->cascade;
    resolved->restart_identity = stmt->restart_identity;
    resolved->async_mode = !stmt->sync_mode;  // ASYNC is default (sync_mode = false)

    for (const auto& table_path : stmt->tables) {
        auto table_ref = resolveTable(table_path, stmt->span, false);
        if (table_ref) {
            resolved->table_uuids.push_back(table_ref->table_uuid);
        } else {
            // Table resolution failed - error already reported
            return nullptr;
        }
    }

    if (resolved->table_uuids.empty()) {
        error(stmt->span, "TRUNCATE TABLE requires at least one table");
        return nullptr;
    }

    return resolved;
}

// =============================================================================
// DML Statement Analysis
// =============================================================================

ResolvedSelectStmt* SemanticAnalyzerV2::analyzeSelect(SelectStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedSelectStmt>();
    resolved->span = stmt->span;
    resolved->distinct = stmt->distinct;
    resolved->all = stmt->all;
    resolved->for_update = stmt->for_update;
    resolved->for_share = stmt->for_share;

    // Push a new scope for this SELECT
    pushScope();

    // 1. Analyze FROM clause first (populates scope with table columns)
    analyzeFromClause(stmt, resolved);

    // 2. Analyze SELECT list
    analyzeSelectList(stmt, resolved);

    // 3. Analyze WHERE clause
    if (stmt->where) {
        resolved->where = analyzeExpression(stmt->where);
        if (resolved->where && !resolved->where->type.isBoolean() &&
            resolved->where->type.data_type != DataType::UNKNOWN) {
            error(stmt->where->span, "WHERE clause must be a boolean expression");
        }
    }

    // 4. Analyze GROUP BY clause
    analyzeGroupByClause(stmt, resolved);

    // 5. Analyze HAVING clause
    if (stmt->having) {
        resolved->having = analyzeExpression(stmt->having);
        if (resolved->having && !resolved->having->type.isBoolean() &&
            resolved->having->type.data_type != DataType::UNKNOWN) {
            error(stmt->having->span, "HAVING clause must be a boolean expression");
        }
    }

    // 6. Analyze ORDER BY clause
    analyzeOrderByClause(stmt->order_by, resolved->order_by);

    // 7. Analyze LIMIT/OFFSET
    if (stmt->limit) {
        resolved->limit = analyzeExpression(stmt->limit);
    }
    if (stmt->offset) {
        resolved->offset = analyzeExpression(stmt->offset);
    }

    // 8. Analyze set operations (UNION, INTERSECT, EXCEPT)
    if (stmt->set_op != SetOpType::NONE && stmt->set_op_right) {
        resolved->set_op = stmt->set_op;
        resolved->set_op_all = stmt->set_op_all;
        resolved->set_op_right = analyzeSelect(stmt->set_op_right);

        // Verify column count matches
        if (resolved->set_op_right &&
            resolved->select_list.size() != resolved->set_op_right->select_list.size()) {
            error(stmt->set_op_right->span, "Set operation queries must have same number of columns");
        }
    }

    // Validate GROUP BY semantics
    if (!resolved->group_by.empty() || has_aggregates_) {
        validateGroupBy(resolved);
    }

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeInsert(InsertStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedInsertStmt>();
    resolved->span = stmt->span;

    // Resolve target table
    auto table_ref = resolveTable(stmt->table_path, stmt->span);
    if (!table_ref) {
        return nullptr;
    }
    resolved->target_table = *table_ref;

    // Add table to scope for RETURNING clause
    pushScope();
    ResolutionScope::TableEntry entry;
    entry.table_uuid = table_ref->table_uuid;
    entry.columns = table_ref->columns;
    entry.alias = StringPool::INVALID_ID;  // No alias for target table
    currentScope().addTable(entry);

    // Resolve target columns
    if (stmt->columns.empty()) {
        // All columns in table order
        for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
            resolved->target_column_indexes.push_back(i);
        }
    } else {
        // Specified columns
        for (auto col_name : stmt->columns) {
            bool found = false;
            for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                if (table_ref->columns[i].name == col_name) {
                    resolved->target_column_indexes.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "Column not found: " + std::string(getString(col_name)));
                return nullptr;
            }
        }
    }

    // Analyze source based on InsertStmt::Source enum
    switch (stmt->source) {
        case InsertStmt::Source::SELECT:
            resolved->source = ResolvedInsertStmt::Source::SELECT;
            resolved->select_source = analyzeSelect(stmt->select_source);

            // Verify column count matches
            if (resolved->select_source &&
                resolved->target_column_indexes.size() != resolved->select_source->select_list.size()) {
                error(stmt->span, "INSERT column count doesn't match SELECT column count");
            }
            break;

        case InsertStmt::Source::DEFAULT:
            resolved->source = ResolvedInsertStmt::Source::DEFAULT;
            break;

        case InsertStmt::Source::VALUES:
            resolved->source = ResolvedInsertStmt::Source::VALUES;

            // Analyze value rows
            for (const auto& row : stmt->values_rows) {
                if (row.size() != resolved->target_column_indexes.size()) {
                    error(stmt->span, "VALUES row has wrong number of columns");
                    continue;
                }

                std::vector<ResolvedExpression*> resolved_row;
                for (auto* expr : row) {
                    resolved_row.push_back(analyzeExpression(expr));
                }
                resolved->values_rows.push_back(std::move(resolved_row));
            }
            break;
    }

    // Analyze ON CONFLICT if present
    if (stmt->on_conflict) {
        auto* on_conflict = new ResolvedInsertStmt::OnConflict();
        on_conflict->action = stmt->on_conflict->action;

        // Resolve conflict columns
        for (auto col_name : stmt->on_conflict->columns) {
            bool found = false;
            for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                if (table_ref->columns[i].name == col_name) {
                    on_conflict->conflict_columns.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "ON CONFLICT column not found: " + std::string(getString(col_name)));
            }
        }

        // Analyze update assignments for DO UPDATE
        if (stmt->on_conflict->action == ConflictAction::UPDATE) {
            for (const auto& assign : stmt->on_conflict->set_items) {
                bool found = false;
                for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                    if (table_ref->columns[i].name == assign.first) {
                        on_conflict->update_assignments.push_back(
                            std::make_pair(i, analyzeExpression(assign.second)));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    error(stmt->span, "UPDATE column not found: " + std::string(getString(assign.first)));
                }
            }

            if (stmt->on_conflict->where_action) {
                on_conflict->where = analyzeExpression(stmt->on_conflict->where_action);
            }
        }

        resolved->on_conflict.reset(on_conflict);
    }

    // Analyze RETURNING clause
    analyzeReturningClause(stmt->returning, resolved->returning);

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeUpdate(UpdateStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedUpdateStmt>();
    resolved->span = stmt->span;

    // Resolve target table
    auto table_ref = resolveTable(stmt->table_path, stmt->span);
    if (!table_ref) {
        return nullptr;
    }
    resolved->target_table = *table_ref;

    // Add table to scope
    pushScope();
    ResolutionScope::TableEntry entry;
    entry.table_uuid = table_ref->table_uuid;
    entry.columns = table_ref->columns;
    entry.alias = stmt->alias;
    currentScope().addTable(entry);

    // Analyze FROM clause if present (for UPDATE ... FROM ...)
    if (stmt->from) {
        auto* from_ref = analyzeTableRef(stmt->from);
        if (from_ref) {
            resolved->from_tables.push_back(from_ref);
        }
    }

    // Analyze SET assignments (set_items is the field name in UpdateStmt)
    for (const auto& assign : stmt->set_items) {
        bool found = false;
        for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
            if (table_ref->columns[i].name == assign.first) {
                auto* expr = analyzeExpression(assign.second);
                resolved->assignments.push_back(std::make_pair(i, expr));
                found = true;
                break;
            }
        }
        if (!found) {
            error(stmt->span, "SET column not found: " + std::string(getString(assign.first)));
        }
    }

    // Analyze WHERE clause
    if (stmt->where) {
        resolved->where = analyzeExpression(stmt->where);
    }

    // Analyze RETURNING clause
    analyzeReturningClause(stmt->returning, resolved->returning);

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDelete(DeleteStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDeleteStmt>();
    resolved->span = stmt->span;

    // Resolve target table
    auto table_ref = resolveTable(stmt->table_path, stmt->span);
    if (!table_ref) {
        return nullptr;
    }
    resolved->target_table = *table_ref;

    // Add table to scope
    pushScope();
    ResolutionScope::TableEntry entry;
    entry.table_uuid = table_ref->table_uuid;
    entry.columns = table_ref->columns;
    entry.alias = stmt->alias;
    currentScope().addTable(entry);

    // Analyze USING clause if present (using_clause is the field name in DeleteStmt)
    if (stmt->using_clause) {
        auto* using_ref = analyzeTableRef(stmt->using_clause);
        if (using_ref) {
            resolved->using_tables.push_back(using_ref);
        }
    }

    // Analyze WHERE clause
    if (stmt->where) {
        resolved->where = analyzeExpression(stmt->where);
    }

    // Analyze RETURNING clause
    analyzeReturningClause(stmt->returning, resolved->returning);

    popScope();
    return resolved;
}

// =============================================================================
// Expression Analysis
// =============================================================================

ResolvedExpression* SemanticAnalyzerV2::analyzeExpression(Expression* expr) {
    if (!expr) {
        return nullptr;
    }

    switch (expr->kind()) {
        case ASTKind::LiteralExpr:
            return analyzeLiteral(static_cast<LiteralExpr*>(expr));
        case ASTKind::ColumnRefExpr:
            return analyzeColumnRef(static_cast<ColumnRefExpr*>(expr));
        case ASTKind::BinaryExpr:
            return analyzeBinaryExpr(static_cast<BinaryExpr*>(expr));
        case ASTKind::UnaryExpr:
            return analyzeUnaryExpr(static_cast<UnaryExpr*>(expr));
        case ASTKind::FunctionCallExpr:
            return analyzeFunctionCall(static_cast<FunctionCallExpr*>(expr));
        case ASTKind::CastExpr:
            return analyzeCast(static_cast<CastExpr*>(expr));
        case ASTKind::CaseExpr:
            return analyzeCase(static_cast<CaseExpr*>(expr));
        case ASTKind::SubqueryExpr:
            return analyzeSubquery(static_cast<SubqueryExpr*>(expr));
        case ASTKind::ExistsExpr:
            return analyzeExists(static_cast<ExistsExpr*>(expr));
        case ASTKind::InExpr:
            return analyzeIn(static_cast<InExpr*>(expr));
        case ASTKind::BetweenExpr:
            return analyzeBetween(static_cast<BetweenExpr*>(expr));
        case ASTKind::LikeExpr:
            return analyzeLike(static_cast<LikeExpr*>(expr));
        case ASTKind::IsNullExpr:
            return analyzeIsNull(static_cast<IsNullExpr*>(expr));
        case ASTKind::ArrayExpr:
            return analyzeArray(static_cast<ArrayExpr*>(expr));
        default:
            error(expr->span, "Unknown expression type");
            return nullptr;
    }
}

ResolvedExpression* SemanticAnalyzerV2::analyzeLiteral(LiteralExpr* expr) {
    auto* resolved = arena_.create<ResolvedLiteral>();
    resolved->span = expr->span;
    resolved->literal_type = expr->literal_type;  // Copy the literal type

    switch (expr->literal_type) {
        case LiteralType::INTEGER:
            resolved->type.data_type = DataType::INT64;
            resolved->type.is_nullable = false;
            resolved->int_value = expr->int_value;
            break;

        case LiteralType::FLOAT:
            resolved->type.data_type = DataType::FLOAT64;
            resolved->type.is_nullable = false;
            resolved->float_value = expr->float_value;
            break;

        case LiteralType::STRING:
            resolved->type.data_type = DataType::VARCHAR;
            resolved->type.is_nullable = false;
            resolved->string_value = expr->string_value;
            break;

        case LiteralType::BLOB:
            resolved->type.data_type = DataType::BLOB;
            resolved->type.is_nullable = false;
            resolved->string_value = expr->string_value;
            break;

        case LiteralType::BOOLEAN:
            resolved->type.data_type = DataType::BOOLEAN;
            resolved->type.is_nullable = false;
            resolved->bool_value = expr->bool_value;
            break;

        case LiteralType::NULL_VALUE:
            resolved->type.data_type = DataType::UNKNOWN;  // NULL takes type from context
            resolved->type.is_nullable = true;
            resolved->is_null = true;
            break;

        case LiteralType::DEFAULT:
            resolved->type.data_type = DataType::UNKNOWN;  // DEFAULT type determined by column
            resolved->type.is_nullable = true;
            resolved->is_default = true;
            break;
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeColumnRef(ColumnRefExpr* expr) {
    StringPool::StringId table_alias = StringPool::INVALID_ID;
    if (expr->column.has_table_qualifier && !expr->column.table_path.components.empty()) {
        // Use last component of table path as alias
        table_alias = expr->column.table_path.components.back();
    }

    auto resolved_col = resolveColumn(table_alias, expr->column.column_name, expr->span);
    if (!resolved_col) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedColumnRefExpr>();
    resolved->span = expr->span;
    resolved->column = *resolved_col;
    resolved->type.data_type = resolved_col->data_type;
    resolved->type.is_nullable = resolved_col->is_nullable;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeBinaryExpr(BinaryExpr* expr) {
    auto* left = analyzeExpression(expr->left);
    auto* right = analyzeExpression(expr->right);

    if (!left || !right) {
        return nullptr;
    }

    auto common_type = getCommonType(left->type, right->type, expr->op);
    if (!common_type) {
        error(expr->span, "Incompatible types for binary operator");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedBinaryExpr>();
    resolved->span = expr->span;
    resolved->op = expr->op;
    resolved->left = left;
    resolved->right = right;
    resolved->type = *common_type;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeUnaryExpr(UnaryExpr* expr) {
    auto* operand = analyzeExpression(expr->operand);
    if (!operand) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedUnaryExpr>();
    resolved->span = expr->span;
    resolved->op = expr->op;
    resolved->operand = operand;

    switch (expr->op) {
        case UnaryOp::NEGATE:
            if (!operand->type.isNumeric()) {
                error(expr->span, "NEGATE operator requires numeric operand");
                return nullptr;
            }
            resolved->type = operand->type;
            break;

        case UnaryOp::NOT:
            if (!operand->type.isBoolean()) {
                error(expr->span, "NOT operator requires boolean operand");
                return nullptr;
            }
            resolved->type.data_type = DataType::BOOLEAN;
            resolved->type.is_nullable = operand->type.is_nullable;
            break;

        case UnaryOp::BIT_NOT:
            if (!operand->type.isNumeric()) {
                error(expr->span, "Bitwise NOT operator requires numeric operand");
                return nullptr;
            }
            resolved->type = operand->type;
            break;

        case UnaryOp::IS_NULL:
        case UnaryOp::IS_NOT_NULL:
            resolved->type.data_type = DataType::BOOLEAN;
            resolved->type.is_nullable = false;
            break;
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeFunctionCall(FunctionCallExpr* expr) {
    // Analyze arguments first
    std::vector<ResolvedType> arg_types;
    std::vector<ResolvedExpression*> resolved_args;

    for (auto* arg : expr->arguments) {
        auto* resolved_arg = analyzeExpression(arg);
        if (!resolved_arg) {
            return nullptr;
        }
        arg_types.push_back(resolved_arg->type);
        resolved_args.push_back(resolved_arg);
    }

    // Resolve function
    auto func_ref = resolveFunction(expr->function_path, arg_types, expr->span);
    if (!func_ref) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedFunctionCall>();
    resolved->span = expr->span;
    resolved->function = *func_ref;
    resolved->arguments = std::move(resolved_args);
    resolved->distinct = expr->distinct;

    // Analyze FILTER clause if present
    if (expr->filter) {
        resolved->filter = analyzeExpression(expr->filter);
    }

    // Set result type from function
    if (func_ref->return_type) {
        resolved->type = *func_ref->return_type;
    }

    // Track aggregate usage
    if (func_ref->is_aggregate) {
        has_aggregates_ = true;
        if (in_aggregate_) {
            error(expr->span, "Nested aggregate functions are not allowed");
            return nullptr;
        }
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeCast(CastExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    if (!operand) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCast>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->target_type = resolveTypeName(expr->target_type);
    resolved->type = resolved->target_type;
    resolved->implicit = false;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeCase(CaseExpr* expr) {
    auto* resolved = arena_.create<ResolvedCase>();
    resolved->span = expr->span;

    // Analyze operand for simple CASE
    if (expr->operand) {
        resolved->operand = analyzeExpression(expr->operand);
        if (!resolved->operand) {
            return nullptr;
        }
    }

    // Track result type from WHEN clauses
    ResolvedType result_type;
    bool first_result = true;

    for (const auto& when : expr->when_clauses) {
        ResolvedCase::WhenClause resolved_when;

        resolved_when.when_expr = analyzeExpression(when.when_expr);
        if (!resolved_when.when_expr) {
            return nullptr;
        }

        resolved_when.then_expr = analyzeExpression(when.then_expr);
        if (!resolved_when.then_expr) {
            return nullptr;
        }

        // Type check WHEN condition
        if (expr->operand) {
            // Simple CASE: WHEN value must match operand type
            if (!resolved_when.when_expr->type.isComparableTo(resolved->operand->type)) {
                error(when.when_expr->span, "WHEN value type doesn't match CASE operand");
                return nullptr;
            }
        } else {
            // Searched CASE: WHEN must be boolean
            if (!resolved_when.when_expr->type.isBoolean()) {
                error(when.when_expr->span, "WHEN condition must be boolean");
                return nullptr;
            }
        }

        // Track common result type
        if (first_result) {
            result_type = resolved_when.then_expr->type;
            first_result = false;
        } else {
            if (!result_type.isComparableTo(resolved_when.then_expr->type)) {
                error(when.then_expr->span, "CASE result types are incompatible");
                return nullptr;
            }
        }

        resolved->when_clauses.push_back(resolved_when);
    }

    // Analyze ELSE
    if (expr->else_expr) {
        resolved->else_expr = analyzeExpression(expr->else_expr);
        if (!resolved->else_expr) {
            return nullptr;
        }
        if (!first_result && !result_type.isComparableTo(resolved->else_expr->type)) {
            error(expr->else_expr->span, "ELSE type is incompatible with WHEN results");
            return nullptr;
        }
    } else {
        result_type.is_nullable = true;  // No ELSE means NULL is possible
    }

    resolved->type = result_type;
    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeSubquery(SubqueryExpr* expr) {
    ++subquery_depth_;
    pushScope();

    auto* subselect = analyzeSelect(expr->subquery);

    popScope();
    --subquery_depth_;

    if (!subselect) {
        return nullptr;
    }

    // Scalar subquery must return single column
    if (subselect->select_list.size() != 1) {
        error(expr->span, "Scalar subquery must return exactly one column");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedSubqueryExpr>();
    resolved->span = expr->span;
    resolved->subquery = subselect;
    resolved->type = subselect->select_list[0].type;
    resolved->type.is_nullable = true;  // Subquery may return 0 rows

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeExists(ExistsExpr* expr) {
    ++subquery_depth_;
    pushScope();

    auto* subselect = analyzeSelect(expr->subquery);

    popScope();
    --subquery_depth_;

    if (!subselect) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedExistsExpr>();
    resolved->span = expr->span;
    resolved->subquery = subselect;
    resolved->negated = expr->negated;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = false;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeIn(InExpr* expr) {
    auto* left = analyzeExpression(expr->expr);
    if (!left) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedInExpr>();
    resolved->span = expr->span;
    resolved->expr = left;
    resolved->negated = expr->negated;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = false;

    if (expr->has_subquery) {
        ++subquery_depth_;
        pushScope();

        auto* subselect = analyzeSelect(expr->subquery);

        popScope();
        --subquery_depth_;

        if (!subselect) {
            return nullptr;
        }

        // IN subquery must return single column
        if (subselect->select_list.size() != 1) {
            error(expr->span, "IN subquery must return exactly one column");
            return nullptr;
        }

        if (!left->type.isComparableTo(subselect->select_list[0].type)) {
            error(expr->span, "IN expression and subquery types are incompatible");
            return nullptr;
        }

        resolved->subquery = subselect;
        resolved->has_subquery = true;
    } else {
        for (auto* val : expr->values) {
            auto* resolved_val = analyzeExpression(val);
            if (!resolved_val) {
                return nullptr;
            }

            if (!left->type.isComparableTo(resolved_val->type)) {
                error(val->span, "IN list value type is incompatible with expression");
                return nullptr;
            }

            resolved->values.push_back(resolved_val);
        }
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeBetween(BetweenExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    auto* low = analyzeExpression(expr->low);
    auto* high = analyzeExpression(expr->high);

    if (!operand || !low || !high) {
        return nullptr;
    }

    if (!operand->type.isComparableTo(low->type) || !operand->type.isComparableTo(high->type)) {
        error(expr->span, "BETWEEN operand types are incompatible");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedBetweenExpr>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->negated = expr->negated;
    resolved->symmetric = expr->symmetric;
    resolved->low = low;
    resolved->high = high;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = operand->type.is_nullable || low->type.is_nullable || high->type.is_nullable;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeLike(LikeExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    auto* pattern = analyzeExpression(expr->pattern);

    if (!operand || !pattern) {
        return nullptr;
    }

    if (!operand->type.isString()) {
        error(expr->expr->span, "LIKE operand must be a string type");
        return nullptr;
    }

    if (!pattern->type.isString()) {
        error(expr->pattern->span, "LIKE pattern must be a string type");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedLikeExpr>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->negated = expr->negated;
    resolved->case_insensitive = expr->case_insensitive;
    resolved->pattern = pattern;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = operand->type.is_nullable || pattern->type.is_nullable;

    if (expr->escape) {
        resolved->escape = analyzeExpression(expr->escape);
        if (!resolved->escape) {
            return nullptr;
        }
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeIsNull(IsNullExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    if (!operand) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedIsNullExpr>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->negated = expr->negated;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = false;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeArray(ArrayExpr* expr) {
    auto* resolved = arena_.create<ResolvedArrayExpr>();
    resolved->span = expr->span;

    if (expr->has_subquery) {
        ++subquery_depth_;
        pushScope();

        auto* subselect = analyzeSelect(expr->subquery);

        popScope();
        --subquery_depth_;

        if (!subselect) {
            return nullptr;
        }

        if (subselect->select_list.size() != 1) {
            error(expr->span, "ARRAY subquery must return exactly one column");
            return nullptr;
        }

        resolved->subquery = subselect;
        resolved->has_subquery = true;
        resolved->type = subselect->select_list[0].type;
        resolved->type.is_array = true;
    } else {
        // Determine element type from first element
        ResolvedType element_type;
        bool first = true;

        for (auto* elem : expr->elements) {
            auto* resolved_elem = analyzeExpression(elem);
            if (!resolved_elem) {
                return nullptr;
            }

            if (first) {
                element_type = resolved_elem->type;
                first = false;
            } else {
                if (!element_type.isComparableTo(resolved_elem->type)) {
                    error(elem->span, "Array element types must be compatible");
                    return nullptr;
                }
            }

            resolved->elements.push_back(resolved_elem);
        }

        resolved->type = element_type;
        resolved->type.is_array = true;
    }

    return resolved;
}

// =============================================================================
// Clause Analysis
// =============================================================================

void SemanticAnalyzerV2::analyzeFromClause(SelectStmt* stmt, ResolvedSelectStmt* resolved) {
    if (!stmt->from) {
        return;  // SELECT without FROM (e.g., SELECT 1+1)
    }

    // Analyze main table reference
    auto* main_ref = analyzeTableRef(stmt->from);
    if (main_ref) {
        resolved->from_tables.push_back(main_ref);

        // Add to scope
        ResolutionScope::TableEntry entry;
        entry.table_uuid = main_ref->table_uuid;
        entry.alias = main_ref->alias;
        entry.columns = main_ref->columns;
        entry.is_cte = (main_ref->object_type == ResolvedTableRef::ObjectType::CTE);
        currentScope().addTable(entry);
    }

    // Analyze joins
    for (auto* join : stmt->joins) {
        auto* resolved_join = analyzeJoin(join);
        if (resolved_join) {
            resolved->joins.push_back(resolved_join);
        }
    }
}

void SemanticAnalyzerV2::analyzeWhereClause(Expression* where, ResolvedExpression*& resolved) {
    if (where) {
        resolved = analyzeExpression(where);
    }
}

void SemanticAnalyzerV2::analyzeGroupByClause(SelectStmt* stmt, ResolvedSelectStmt* resolved) {
    has_aggregates_ = false;

    for (auto* expr : stmt->group_by) {
        auto* resolved_expr = analyzeExpression(expr);
        if (resolved_expr) {
            resolved->group_by.push_back(resolved_expr);
        }
    }
}

void SemanticAnalyzerV2::analyzeHavingClause(Expression* having, ResolvedExpression*& resolved) {
    if (having) {
        resolved = analyzeExpression(having);
    }
}

void SemanticAnalyzerV2::analyzeOrderByClause(
    const std::vector<OrderByItem*>& items,
    std::vector<ResolvedOrderByItem*>& resolved)
{
    for (auto* item : items) {
        auto* resolved_item = arena_.create<ResolvedOrderByItem>();
        resolved_item->expr = analyzeExpression(item->expr);
        resolved_item->ascending = item->ascending;
        resolved_item->nulls_first = item->nulls_first;
        resolved_item->nulls_last = item->nulls_last;
        resolved.push_back(resolved_item);
    }
}

void SemanticAnalyzerV2::analyzeSelectList(SelectStmt* stmt, ResolvedSelectStmt* resolved) {
    for (auto* item : stmt->items) {
        ResolvedSelectItem resolved_item;

        switch (item->item_type) {
            case SelectItem::Type::EXPRESSION:
                resolved_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;
                resolved_item.expr = analyzeExpression(item->expr);
                resolved_item.alias = item->alias;
                resolved_item.has_alias = item->has_alias;
                if (resolved_item.expr) {
                    resolved_item.type = resolved_item.expr->type;
                }
                break;

            case SelectItem::Type::STAR:
                resolved_item.item_type = ResolvedSelectItem::ItemType::STAR;
                // Expand * to all columns in scope
                for (const auto& table : currentScope().tables()) {
                    for (const auto& col : table.columns) {
                        ResolvedSelectItem col_item;
                        col_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;

                        // Create a column reference expression
                        auto* col_expr = arena_.create<ResolvedColumnRefExpr>();
                        col_expr->column.table_uuid = table.table_uuid;
                        col_expr->column.column_index = col.column_index;
                        col_expr->column.data_type = col.data_type;
                        col_expr->column.is_nullable = col.is_nullable;
                        col_expr->column.column_name = col.name;
                        col_expr->column.table_alias = table.alias;
                        col_expr->type.data_type = col.data_type;
                        col_expr->type.is_nullable = col.is_nullable;

                        col_item.expr = col_expr;
                        col_item.type.data_type = col.data_type;
                        col_item.type.is_nullable = col.is_nullable;
                        resolved->select_list.push_back(col_item);
                    }
                }
                continue;  // Already added items, skip the push at end

            case SelectItem::Type::TABLE_STAR:
                resolved_item.item_type = ResolvedSelectItem::ItemType::TABLE_STAR;
                // Find the table and expand t.* to its columns
                if (!item->table_path.components.empty()) {
                    StringPool::StringId table_name = item->table_path.components.back();
                    const auto* table = currentScope().findTable(table_name);
                    if (table) {
                        resolved_item.table_uuid = table->table_uuid;
                        for (const auto& col : table->columns) {
                            ResolvedSelectItem col_item;
                            col_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;

                            auto* col_expr = arena_.create<ResolvedColumnRefExpr>();
                            col_expr->column.table_uuid = table->table_uuid;
                            col_expr->column.column_index = col.column_index;
                            col_expr->column.data_type = col.data_type;
                            col_expr->column.is_nullable = col.is_nullable;
                            col_expr->column.column_name = col.name;
                            col_expr->column.table_alias = table->alias;
                            col_expr->type.data_type = col.data_type;
                            col_expr->type.is_nullable = col.is_nullable;

                            col_item.expr = col_expr;
                            col_item.type.data_type = col.data_type;
                            col_item.type.is_nullable = col.is_nullable;
                            resolved->select_list.push_back(col_item);
                        }
                        continue;  // Already added items
                    } else {
                        error(item->span, "Table not found for .*: " + std::string(getString(table_name)));
                    }
                }
                break;
        }

        resolved->select_list.push_back(resolved_item);
    }
}

void SemanticAnalyzerV2::analyzeReturningClause(
    const std::vector<SelectItem*>& returning,
    std::vector<ResolvedSelectItem>& resolved)
{
    for (auto* item : returning) {
        ResolvedSelectItem resolved_item;

        switch (item->item_type) {
            case SelectItem::Type::EXPRESSION:
                resolved_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;
                resolved_item.expr = analyzeExpression(item->expr);
                resolved_item.alias = item->alias;
                resolved_item.has_alias = item->has_alias;
                if (resolved_item.expr) {
                    resolved_item.type = resolved_item.expr->type;
                }
                break;

            case SelectItem::Type::STAR:
                // RETURNING * expands to all columns of the target table
                resolved_item.item_type = ResolvedSelectItem::ItemType::STAR;
                break;

            case SelectItem::Type::TABLE_STAR:
                resolved_item.item_type = ResolvedSelectItem::ItemType::TABLE_STAR;
                if (!item->table_path.components.empty()) {
                    StringPool::StringId table_name = item->table_path.components.back();
                    const auto* table = currentScope().findTable(table_name);
                    if (table) {
                        resolved_item.table_uuid = table->table_uuid;
                    } else {
                        error(item->span, "Table not found for .*: " + std::string(getString(table_name)));
                    }
                }
                break;
        }

        resolved.push_back(resolved_item);
    }
}

// =============================================================================
// Table Reference Analysis
// =============================================================================

ResolvedTableRef* SemanticAnalyzerV2::analyzeTableRef(TableRefNode* node) {
    if (!node) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedTableRef>();
    resolved->alias = node->alias;
    resolved->has_alias = node->has_alias;

    switch (node->ref_type) {
        case TableRefNode::Type::TABLE: {
            auto table_ref = resolveTable(node->table_path, node->span);
            if (!table_ref) {
                return nullptr;
            }
            *resolved = *table_ref;
            resolved->alias = node->alias;
            resolved->has_alias = node->has_alias;
            break;
        }

        case TableRefNode::Type::SUBQUERY: {
            ++subquery_depth_;
            pushScope();

            auto* subselect = analyzeSelect(static_cast<SelectStmt*>(node->subquery));

            popScope();
            --subquery_depth_;

            if (!subselect) {
                return nullptr;
            }

            resolved->object_type = ResolvedTableRef::ObjectType::SUBQUERY;
            resolved->subquery = subselect;

            // Build column info from subquery result
            for (size_t i = 0; i < subselect->select_list.size(); ++i) {
                const auto& item = subselect->select_list[i];
                ResolvedTableRef::ColumnInfo col;

                // Use alias if available, otherwise generate name
                if (item.has_alias) {
                    col.name = item.alias;
                } else if (!node->column_aliases.empty() && i < node->column_aliases.size()) {
                    col.name = node->column_aliases[i];
                } else {
                    col.name = internString("column" + std::to_string(i + 1));
                }

                col.data_type = item.type.data_type;
                col.is_nullable = item.type.is_nullable;
                col.column_index = static_cast<uint32_t>(i);
                resolved->columns.push_back(col);
            }
            break;
        }

        case TableRefNode::Type::FUNCTION: {
            // Table-valued function
            auto* func = analyzeFunctionCall(node->function);
            if (!func) {
                return nullptr;
            }

            resolved->object_type = ResolvedTableRef::ObjectType::FUNCTION;
            // For table-valued functions, we'd need to look up the function's
            // return table structure. For now, create a placeholder.
            break;
        }

        case TableRefNode::Type::JOIN: {
            // JOIN is typically handled by analyzeJoin, but handle nested case
            error(node->span, "Unexpected JOIN in table reference");
            return nullptr;
        }
    }

    return resolved;
}

ResolvedJoin* SemanticAnalyzerV2::analyzeJoin(JoinNode* node) {
    if (!node) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedJoin>();
    resolved->join_type = node->join_type;

    // Analyze left side
    resolved->left = analyzeTableRef(node->left);
    if (!resolved->left) {
        return nullptr;
    }

    // Add left side columns to scope
    ResolutionScope::TableEntry left_entry;
    left_entry.table_uuid = resolved->left->table_uuid;
    left_entry.alias = resolved->left->alias;
    left_entry.columns = resolved->left->columns;
    currentScope().addTable(left_entry);

    // Analyze right side
    resolved->right = analyzeTableRef(node->right);
    if (!resolved->right) {
        return nullptr;
    }

    // Add right side columns to scope
    ResolutionScope::TableEntry right_entry;
    right_entry.table_uuid = resolved->right->table_uuid;
    right_entry.alias = resolved->right->alias;
    right_entry.columns = resolved->right->columns;
    currentScope().addTable(right_entry);

    // Analyze ON condition
    if (node->on_condition) {
        resolved->on_condition = analyzeExpression(node->on_condition);
        if (resolved->on_condition && !resolved->on_condition->type.isBoolean()) {
            error(node->on_condition->span, "JOIN ON condition must be boolean");
        }
    }

    // Handle USING clause
    if (node->has_using) {
        resolved->has_using = true;
        for (auto col_name : node->using_columns) {
            // Find column in both tables
            const ResolvedTableRef::ColumnInfo* left_col = nullptr;
            const ResolvedTableRef::ColumnInfo* right_col = nullptr;

            for (const auto& col : resolved->left->columns) {
                if (col.name == col_name) {
                    left_col = &col;
                    break;
                }
            }

            for (const auto& col : resolved->right->columns) {
                if (col.name == col_name) {
                    right_col = &col;
                    break;
                }
            }

            if (!left_col || !right_col) {
                error(node->span, "USING column not found in both tables: " + std::string(getString(col_name)));
                continue;
            }

            ResolvedColumnRef resolved_col;
            resolved_col.table_uuid = resolved->left->table_uuid;
            resolved_col.column_index = left_col->column_index;
            resolved_col.data_type = left_col->data_type;
            resolved_col.is_nullable = left_col->is_nullable;
            resolved_col.column_name = col_name;
            resolved->using_columns.push_back(resolved_col);
        }
    }

    return resolved;
}

// =============================================================================
// Type Resolution Helpers
// =============================================================================

ResolvedType SemanticAnalyzerV2::resolveTypeName(const TypeName& type_name) {
    ResolvedType resolved;
    resolved.is_nullable = true;

    // Parse type name string to DataType
    if (type_name.name != StringPool::INVALID_ID) {
        std::string name_str = std::string(string_pool_.get(type_name.name));
        // Convert to lowercase for comparison
        std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower);

        // Map string to DataType
        if (name_str == "int" || name_str == "integer" || name_str == "int32") {
            resolved.data_type = DataType::INT32;
        } else if (name_str == "smallint" || name_str == "int16") {
            resolved.data_type = DataType::INT16;
        } else if (name_str == "bigint" || name_str == "int64") {
            resolved.data_type = DataType::INT64;
        } else if (name_str == "float" || name_str == "real" || name_str == "float32") {
            resolved.data_type = DataType::FLOAT32;
        } else if (name_str == "double" || name_str == "double precision" || name_str == "float64") {
            resolved.data_type = DataType::FLOAT64;
        } else if (name_str == "decimal" || name_str == "numeric") {
            resolved.data_type = DataType::DECIMAL;
        } else if (name_str == "varchar" || name_str == "character varying") {
            resolved.data_type = DataType::VARCHAR;
        } else if (name_str == "char" || name_str == "character") {
            resolved.data_type = DataType::CHAR;
        } else if (name_str == "text") {
            resolved.data_type = DataType::TEXT;
        } else if (name_str == "boolean" || name_str == "bool") {
            resolved.data_type = DataType::BOOLEAN;
        } else if (name_str == "date") {
            resolved.data_type = DataType::DATE;
        } else if (name_str == "time") {
            resolved.data_type = DataType::TIME;
        } else if (name_str == "timestamp") {
            resolved.data_type = DataType::TIMESTAMP;
        } else if (name_str == "interval") {
            resolved.data_type = DataType::INTERVAL;
        } else if (name_str == "blob" || name_str == "bytea") {
            resolved.data_type = DataType::BLOB;
        } else if (name_str == "uuid") {
            resolved.data_type = DataType::UUID;
        } else if (name_str == "json") {
            resolved.data_type = DataType::JSON;
        } else if (name_str == "jsonb") {
            resolved.data_type = DataType::JSONB;
        } else {
            // Try resolving as a domain
            core::ErrorContext ctx;
            core::DomainInfo dinfo;
            // Search current schema then search_path
            bool found = false;
            if (!isZeroUuidLocal(current_schema_) &&
                catalog_.getDomainByName(current_schema_, name_str, dinfo, &ctx) == Status::OK) {
                found = true;
            } else {
                for (const auto& sch : search_path_) {
                    if (catalog_.getDomainByName(sch, name_str, dinfo, &ctx) == Status::OK) {
                        found = true;
                        break;
                    }
                }
            }
            if (found && current_result_) {
                current_result_->addDependency(dinfo.domain_id, core::CatalogManager::ObjectType::DOMAIN);
            }
            resolved.data_type = DataType::UNKNOWN;
        }
    } else {
        resolved.data_type = DataType::UNKNOWN;
    }

    // Copy optional parameters
    if (type_name.precision.has_value()) {
        resolved.precision = type_name.precision.value();
    }
    if (type_name.scale.has_value()) {
        resolved.scale = type_name.scale.value();
    }
    if (type_name.length.has_value()) {
        resolved.length = type_name.length.value();
    }
    resolved.is_array = type_name.is_array;
    if (type_name.array_size.has_value()) {
        resolved.array_size = type_name.array_size.value();
    }
    resolved.with_time_zone = type_name.with_time_zone;

    return resolved;
}

DataType SemanticAnalyzerV2::mapToDataType(DataType ast_type, int32_t /*precision*/, int32_t /*scale*/) {
    return ast_type;
}

// =============================================================================
// Column Definition Analysis
// =============================================================================

ResolvedColumnDef SemanticAnalyzerV2::analyzeColumnDef(ColumnDef* def) {
    ResolvedColumnDef resolved;

    if (!def) {
        return resolved;
    }

    resolved.name = def->name;
    resolved.type = resolveTypeName(def->type);
    resolved.is_nullable = true;  // Default to nullable

    // Process column constraints
    for (const auto& constraint : def->constraints) {
        switch (constraint.type) {
            case ConstraintType::NOT_NULL:
                resolved.is_nullable = false;
                break;

            case ConstraintType::NULL_ALLOWED:
                resolved.is_nullable = true;
                break;

            case ConstraintType::PRIMARY_KEY:
                resolved.is_primary_key = true;
                resolved.is_nullable = false;  // PK implies NOT NULL
                break;

            case ConstraintType::UNIQUE:
                resolved.is_unique = true;
                break;

            case ConstraintType::DEFAULT:
                if (constraint.default_expr) {
                    resolved.default_value = analyzeExpression(constraint.default_expr);
                }
                break;

            case ConstraintType::CHECK:
                if (constraint.check_expr) {
                    // Set up a temporary scope with the column being defined
                    // so CHECK expressions can reference it
                    pushScope();

                    // Create a pseudo-table entry for the column
                    ResolutionScope::TableEntry col_entry;
                    col_entry.alias = StringPool::INVALID_ID;  // No table qualifier needed
                    col_entry.table_uuid = ID{};

                    // Add the column being defined
                    ResolvedTableRef::ColumnInfo col_info;
                    col_info.name = def->name;
                    col_info.data_type = resolved.type.data_type;
                    col_info.is_nullable = resolved.is_nullable;
                    col_info.column_index = 0;
                    col_entry.columns.push_back(col_info);

                    currentScope().addTable(col_entry);

                    resolved.check_expr = analyzeExpression(constraint.check_expr);

                    popScope();
                }
                break;

            case ConstraintType::REFERENCES:
                resolved.has_fk = true;
                // Would need to resolve the referenced table
                if (!constraint.ref_table.components.empty()) {
                    auto ref_table = resolveTable(constraint.ref_table, SourceSpan{}, false);
                    if (ref_table) {
                        resolved.fk_table_uuid = ref_table->table_uuid;
                        // Find referenced column (use first column from ref_columns vector)
                        if (!constraint.ref_columns.empty()) {
                            auto ref_col = constraint.ref_columns[0];
                            for (uint32_t i = 0; i < ref_table->columns.size(); ++i) {
                                if (ref_table->columns[i].name == ref_col) {
                                    resolved.fk_column_index = i;
                                    break;
                                }
                            }
                        }
                    }
                }
                resolved.on_delete = constraint.on_delete;
                resolved.on_update = constraint.on_update;
                break;

            case ConstraintType::GENERATED:
                // Generated column - would need special handling
                break;

            case ConstraintType::COLLATE:
                // Collation constraint - would need special handling
                break;
        }
    }

    return resolved;
}

ResolvedTableConstraint SemanticAnalyzerV2::analyzeTableConstraint(
    TableConstraint* constraint,
    const std::vector<ResolvedColumnDef>& columns)
{
    ResolvedTableConstraint resolved;

    if (!constraint) {
        return resolved;
    }

    resolved.name = constraint->name;

    switch (constraint->type) {
        case TableConstraintType::PRIMARY_KEY:
            resolved.constraint_type = ResolvedTableConstraint::Type::PRIMARY_KEY;
            // Resolve column names to indexes
            for (auto col_name : constraint->columns) {
                for (uint32_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        resolved.column_indexes.push_back(i);
                        break;
                    }
                }
            }
            break;

        case TableConstraintType::UNIQUE:
            resolved.constraint_type = ResolvedTableConstraint::Type::UNIQUE;
            for (auto col_name : constraint->columns) {
                for (uint32_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        resolved.column_indexes.push_back(i);
                        break;
                    }
                }
            }
            break;

        case TableConstraintType::FOREIGN_KEY:
            resolved.constraint_type = ResolvedTableConstraint::Type::FOREIGN_KEY;
            // Resolve local columns
            for (auto col_name : constraint->columns) {
                for (uint32_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        resolved.column_indexes.push_back(i);
                        break;
                    }
                }
            }
            // Resolve referenced table and columns
            if (!constraint->ref_table.components.empty()) {
                auto ref_table = resolveTable(constraint->ref_table, SourceSpan{}, false);
                if (ref_table) {
                    resolved.fk_table_uuid = ref_table->table_uuid;
                    for (auto ref_col_name : constraint->ref_columns) {
                        for (uint32_t i = 0; i < ref_table->columns.size(); ++i) {
                            if (ref_table->columns[i].name == ref_col_name) {
                                resolved.fk_column_indexes.push_back(i);
                                break;
                            }
                        }
                    }
                }
            }
            resolved.on_delete = constraint->on_delete;
            resolved.on_update = constraint->on_update;
            break;

        case TableConstraintType::CHECK:
            resolved.constraint_type = ResolvedTableConstraint::Type::CHECK;
            if (constraint->check_expr) {
                resolved.check_expr = analyzeExpression(constraint->check_expr);
            }
            break;

        case TableConstraintType::EXCLUDE:
            // Exclusion constraints are PostgreSQL-specific
            // Would need additional handling
            break;
    }

    return resolved;
}

// =============================================================================
// Utility Methods
// =============================================================================

std::string_view SemanticAnalyzerV2::getString(StringPool::StringId id) const {
    return string_pool_.get(id);
}

StringPool::StringId SemanticAnalyzerV2::internString(std::string_view str) {
    return string_pool_.intern(str);
}

bool SemanticAnalyzerV2::isAggregate(const ResolvedExpression* expr) const {
    if (!expr) {
        return false;
    }

    if (auto* func = dynamic_cast<const ResolvedFunctionCall*>(expr)) {
        return func->function.is_aggregate;
    }

    return false;
}

bool SemanticAnalyzerV2::validateGroupBy(ResolvedSelectStmt* /*stmt*/) {
    // TODO: Validate that non-aggregate columns in SELECT are in GROUP BY
    return true;
}

} // namespace scratchbird::parser::v2
