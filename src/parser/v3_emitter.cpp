#include "scratchbird/parser/v3_emitter.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

namespace scratchbird::parser::v3 {

namespace {

using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::TypeSpec;
using scratchbird::sblr::v3::Value;
using scratchbird::sblr::v3::Buffer;
using scratchbird::sblr::v3::DecodeError;

uint16_t op(Opcode opcode) {
    return static_cast<uint16_t>(opcode);
}

std::shared_ptr<Instruction> makeInstr(const Instruction& inst) {
    return std::make_shared<Instruction>(inst);
}

std::string toUpper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

uint8_t mapJoinType(parser::JoinType type) {
    switch (type) {
        case parser::JoinType::INNER: return 1;
        case parser::JoinType::LEFT: return 2;
        case parser::JoinType::RIGHT: return 3;
        case parser::JoinType::FULL: return 4;
        case parser::JoinType::CROSS: return 5;
        case parser::JoinType::NATURAL: return 1;
        case parser::JoinType::NATURAL_LEFT: return 2;
        case parser::JoinType::NATURAL_RIGHT: return 3;
        case parser::JoinType::NATURAL_FULL: return 4;
    }
    return 1;
}

uint8_t mapSortOrder(bool ascending) {
    return ascending ? 0 : 1;
}

uint8_t mapNullsOrder(const parser::v2::OrderByItem* item) {
    if (!item->has_nulls_spec) return 0;
    return item->nulls_first ? 1 : 2;
}

uint8_t mapGroupingType(parser::GroupingType type) {
    switch (type) {
        case parser::GroupingType::STANDARD: return 0;
        case parser::GroupingType::ROLLUP: return 1;
        case parser::GroupingType::CUBE: return 2;
        case parser::GroupingType::GROUPING_SETS: return 3;
    }
    return 0;
}

std::string indexTypeName(parser::v2::IndexType type) {
    switch (type) {
        case parser::v2::IndexType::BTREE: return "BTREE";
        case parser::v2::IndexType::HASH: return "HASH";
        case parser::v2::IndexType::GIN: return "GIN";
        case parser::v2::IndexType::GIST: return "GIST";
        case parser::v2::IndexType::SPGIST: return "SPGIST";
        case parser::v2::IndexType::BRIN: return "BRIN";
        case parser::v2::IndexType::RTREE: return "RTREE";
        case parser::v2::IndexType::HNSW: return "HNSW";
        case parser::v2::IndexType::BITMAP: return "BITMAP";
        case parser::v2::IndexType::COLUMNSTORE: return "COLUMNSTORE";
        case parser::v2::IndexType::LSM: return "LSM";
        case parser::v2::IndexType::FULLTEXT: return "FULLTEXT";
        case parser::v2::IndexType::IVF: return "IVF";
        case parser::v2::IndexType::ZONEMAP: return "ZONEMAP";
    }
    return "BTREE";
}

uint8_t mapFrameUnit(parser::v2::FrameType type) {
    switch (type) {
        case parser::v2::FrameType::ROWS: return 0;
        case parser::v2::FrameType::RANGE: return 1;
        case parser::v2::FrameType::GROUPS: return 2;
    }
    return 0;
}

uint8_t mapFrameBound(parser::v2::FrameBoundType type) {
    switch (type) {
        case parser::v2::FrameBoundType::UNBOUNDED_PRECEDING: return 0;
        case parser::v2::FrameBoundType::VALUE_PRECEDING: return 1;
        case parser::v2::FrameBoundType::CURRENT_ROW: return 2;
        case parser::v2::FrameBoundType::VALUE_FOLLOWING: return 3;
        case parser::v2::FrameBoundType::UNBOUNDED_FOLLOWING: return 4;
    }
    return 0;
}

void appendLE16(uint16_t v, Value::Bytes& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void appendLE32(uint32_t v, Value::Bytes& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void appendLE64(uint64_t v, Value::Bytes& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 56) & 0xFF));
}

void appendVaruint(uint64_t v, Value::Bytes& out) {
    Buffer tmp;
    scratchbird::sblr::v3::encodeVaruint(v, tmp);
    out.insert(out.end(), tmp.begin(), tmp.end());
}

void appendBytesWithLen(const Value::Bytes& bytes, Value::Bytes& out) {
    appendVaruint(bytes.size(), out);
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void appendStringWithLen(std::string_view s, Value::Bytes& out) {
    appendVaruint(s.size(), out);
    out.insert(out.end(), s.begin(), s.end());
}

}  // namespace

V3Emitter::V3Emitter(parser::v2::StringPool& pool)
    : pool_(pool) {}

bool V3Emitter::emitStatementToContainer(parser::v2::Statement* stmt,
                                         scratchbird::sblr::v3::Container& out,
                                         std::string& err) {
    ok_ = true;
    error_.clear();

    Instruction root = emitStatement(stmt);
    if (!ok_) {
        err = error_;
        return false;
    }

    scratchbird::sblr::v3::Container container;
    container.header.version_major = 3;
    container.header.version_minor = 0;
    container.header.version_patch = 0;
    container.header.flags = 0;

    container.metadata.module_name = "scratchbird";
    container.metadata.module_version = "v3";
    container.metadata.dialect_id = 1;
    container.metadata.target_platform = 0;
    container.metadata.build_id = "";
    container.metadata.source_hash = {};

    scratchbird::sblr::v3::Buffer stream;
    scratchbird::sblr::v3::DecodeError derr;

    // SBLR3_VERSION payload: u16 major, minor, patch
    {
        scratchbird::sblr::v3::Instruction ver;
        ver.opcode = op(Opcode::SBLR3_VERSION);
        ver.flags = 0;
        Value::Bytes bytes;
        bytes.resize(6);
        bytes[0] = 3; bytes[1] = 0;
        bytes[2] = 0; bytes[3] = 0;
        bytes[4] = 0; bytes[5] = 0;
        ver.payload = Value(bytes);
        if (!scratchbird::sblr::v3::encodeInstructionWithSchema(ver, stream, derr)) {
            err = derr.message;
            return false;
        }
    }

    if (!scratchbird::sblr::v3::encodeInstructionWithSchema(root, stream, derr)) {
        err = derr.message;
        return false;
    }

    {
        scratchbird::sblr::v3::Instruction end;
        end.opcode = op(Opcode::SBLR3_END);
        end.flags = 0;
        end.payload = Value(Value::Bytes{});
        if (!scratchbird::sblr::v3::encodeInstructionWithSchema(end, stream, derr)) {
            err = derr.message;
            return false;
        }
    }

    container.bytecode_stream = std::move(stream);
    out = std::move(container);
    return true;
}

void V3Emitter::fail(const std::string& message) {
    if (!ok_) return;
    ok_ = false;
    error_ = message;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitStatement(parser::v2::Statement* stmt) {
    if (!stmt) {
        fail("null statement");
        return {};
    }

    switch (stmt->kind()) {
        case parser::v2::ASTKind::SelectStmt:
            return emitSelect(static_cast<parser::v2::SelectStmt*>(stmt));
        case parser::v2::ASTKind::InsertStmt:
            return emitInsert(static_cast<parser::v2::InsertStmt*>(stmt));
        case parser::v2::ASTKind::UpdateStmt:
            return emitUpdate(static_cast<parser::v2::UpdateStmt*>(stmt));
        case parser::v2::ASTKind::DeleteStmt:
            return emitDelete(static_cast<parser::v2::DeleteStmt*>(stmt));
        case parser::v2::ASTKind::MergeStmt:
            return emitMerge(static_cast<parser::v2::MergeStmt*>(stmt));
        case parser::v2::ASTKind::CopyStmt:
            return emitCopy(static_cast<parser::v2::CopyStmt*>(stmt));
        case parser::v2::ASTKind::ExecuteProcedureStmt:
        case parser::v2::ASTKind::ExecuteStatementStmt:
            return emitPsql(stmt);
        case parser::v2::ASTKind::CreateTableStmt:
        case parser::v2::ASTKind::CreateIndexStmt:
        case parser::v2::ASTKind::CreateViewStmt:
        case parser::v2::ASTKind::CreateSequenceStmt:
        case parser::v2::ASTKind::CreateSchemaStmt:
        case parser::v2::ASTKind::CreateDatabaseStmt:
        case parser::v2::ASTKind::CreateTablespaceStmt:
        case parser::v2::ASTKind::CreateDomainStmt:
        case parser::v2::ASTKind::CreateTypeStmt:
        case parser::v2::ASTKind::CreateFunctionStmt:
        case parser::v2::ASTKind::CreateProcedureStmt:
        case parser::v2::ASTKind::CreateTriggerStmt:
        case parser::v2::ASTKind::CreatePackageStmt:
        case parser::v2::ASTKind::CreateExceptionStmt:
        case parser::v2::ASTKind::CreateUserStmt:
        case parser::v2::ASTKind::CreateRoleStmt:
        case parser::v2::ASTKind::CreateGroupStmt:
        case parser::v2::ASTKind::CreatePolicyStmt:
        case parser::v2::ASTKind::CreateForeignServerStmt:
        case parser::v2::ASTKind::CreateForeignTableStmt:
        case parser::v2::ASTKind::CreateForeignDataWrapperStmt:
        case parser::v2::ASTKind::CreateUserMappingStmt:
        case parser::v2::ASTKind::CreateSynonymStmt:
        case parser::v2::ASTKind::CreateUdrStmt:
        case parser::v2::ASTKind::CreateJobStmt:
        case parser::v2::ASTKind::CreateDomainStmt:
        case parser::v2::ASTKind::CreateTypeStmt:
            return emitDdlCreate(stmt);
        case parser::v2::ASTKind::AlterTableStmt:
        case parser::v2::ASTKind::AlterIndexStmt:
        case parser::v2::ASTKind::AlterSequenceStmt:
        case parser::v2::ASTKind::AlterSchemaStmt:
        case parser::v2::ASTKind::AlterDatabaseStmt:
        case parser::v2::ASTKind::AlterTablespaceStmt:
        case parser::v2::ASTKind::AttachTablespaceStmt:
        case parser::v2::ASTKind::DetachTablespaceStmt:
        case parser::v2::ASTKind::AlterDomainStmt:
        case parser::v2::ASTKind::AlterTypeStmt:
        case parser::v2::ASTKind::AlterPolicyStmt:
        case parser::v2::ASTKind::AlterSystemStmt:
        case parser::v2::ASTKind::AlterJobStmt:
        case parser::v2::ASTKind::RenameObjectStmt:
        case parser::v2::ASTKind::MoveObjectStmt:
            return emitDdlAlter(stmt);
        case parser::v2::ASTKind::DropTableStmt:
        case parser::v2::ASTKind::DropIndexStmt:
        case parser::v2::ASTKind::DropViewStmt:
        case parser::v2::ASTKind::DropSequenceStmt:
        case parser::v2::ASTKind::DropSchemaStmt:
        case parser::v2::ASTKind::DropDatabaseStmt:
        case parser::v2::ASTKind::DropTablespaceStmt:
        case parser::v2::ASTKind::DropDomainStmt:
        case parser::v2::ASTKind::DropTypeStmt:
        case parser::v2::ASTKind::DropFunctionStmt:
        case parser::v2::ASTKind::DropProcedureStmt:
        case parser::v2::ASTKind::DropTriggerStmt:
        case parser::v2::ASTKind::DropPackageStmt:
        case parser::v2::ASTKind::DropRoleStmt:
        case parser::v2::ASTKind::DropGroupStmt:
        case parser::v2::ASTKind::DropExceptionStmt:
        case parser::v2::ASTKind::DropForeignServerStmt:
        case parser::v2::ASTKind::DropForeignTableStmt:
        case parser::v2::ASTKind::DropUserMappingStmt:
        case parser::v2::ASTKind::DropSynonymStmt:
        case parser::v2::ASTKind::DropUdrStmt:
        case parser::v2::ASTKind::DropJobStmt:
        case parser::v2::ASTKind::DropUserStmt:
        case parser::v2::ASTKind::DropPolicyStmt:
            return emitDdlDrop(stmt);
        case parser::v2::ASTKind::TruncateTableStmt:
            return emitDdlTruncate(static_cast<parser::v2::TruncateTableStmt*>(stmt));
        case parser::v2::ASTKind::CommentStmt:
            return emitComment(static_cast<parser::v2::CommentStmt*>(stmt));
        case parser::v2::ASTKind::GrantStmt:
            return emitGrant(static_cast<parser::v2::GrantStmt*>(stmt));
        case parser::v2::ASTKind::RevokeStmt:
            return emitRevoke(static_cast<parser::v2::RevokeStmt*>(stmt));
        case parser::v2::ASTKind::StartTransactionStmt:
        case parser::v2::ASTKind::PrepareTransactionStmt:
        case parser::v2::ASTKind::CommitStmt:
        case parser::v2::ASTKind::RollbackStmt:
        case parser::v2::ASTKind::SavepointStmt:
        case parser::v2::ASTKind::ReleaseSavepointStmt:
            return emitTxn(stmt);
        case parser::v2::ASTKind::SetStmt:
        case parser::v2::ASTKind::ResetStmt:
        case parser::v2::ASTKind::ShowStmt:
        case parser::v2::ASTKind::ExplainStmt:
        case parser::v2::ASTKind::AnalyzeStmt:
            return emitSetShowReset(stmt);
        case parser::v2::ASTKind::ConnectStmt:
        case parser::v2::ASTKind::DisconnectStmt:
        case parser::v2::ASTKind::SweepDatabaseStmt:
        case parser::v2::ASTKind::ExecuteJobStmt:
        case parser::v2::ASTKind::CancelJobRunStmt:
            return emitUtility(stmt);
        case parser::v2::ASTKind::ExecuteBlockStmt:
        case parser::v2::ASTKind::CompoundStmt:
        case parser::v2::ASTKind::DeclareVariableStmt:
        case parser::v2::ASTKind::AssignmentStmt:
        case parser::v2::ASTKind::IfStmt:
        case parser::v2::ASTKind::WhileStmt:
        case parser::v2::ASTKind::ForSelectStmt:
        case parser::v2::ASTKind::ForExecuteStmt:
        case parser::v2::ASTKind::LoopStmt:
        case parser::v2::ASTKind::LeaveStmt:
        case parser::v2::ASTKind::ContinueStmt:
        case parser::v2::ASTKind::ExitStmt:
        case parser::v2::ASTKind::SuspendStmt:
        case parser::v2::ASTKind::ReturnStmt:
        case parser::v2::ASTKind::ExceptionRaiseStmt:
        case parser::v2::ASTKind::WhenExceptionStmt:
        case parser::v2::ASTKind::PostEventStmt:
        case parser::v2::ASTKind::DeclareCursorStmt:
        case parser::v2::ASTKind::OpenCursorStmt:
        case parser::v2::ASTKind::FetchCursorStmt:
        case parser::v2::ASTKind::CloseCursorStmt:
        case parser::v2::ASTKind::ExecuteProcedureStmt:
        case parser::v2::ASTKind::ExecuteStatementStmt:
            return emitPsql(stmt);
        default:
            return emitUtility(stmt);
    }
}

scratchbird::sblr::v3::Instruction V3Emitter::emitSelect(parser::v2::SelectStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_SELECT);
    inst.flags = 0;

    Value::Object payload;
    uint16_t flags = 0;
    if (stmt->distinct) flags |= 0x0001;
    if (stmt->all) flags |= 0x0002;
    if (stmt->for_update) flags |= 0x0004;
    if (stmt->for_share) flags |= 0x0008;
    if (stmt->nowait) flags |= 0x0010;
    if (stmt->skip_locked) flags |= 0x0020;
    payload["flags"] = Value(static_cast<uint64_t>(flags));

    payload["select_items"] = toSelectItems(stmt->items);
    if (stmt->from) {
        payload["from"] = toTableRef(stmt->from);
    }
    payload["joins"] = toJoins(stmt->joins);
    if (stmt->where) {
        payload["where"] = Value(makeInstr(emitExpression(stmt->where)));
    }
    payload["group_by"] = toExprList(stmt->group_by);

    Value::List grouping_sets;
    for (const auto& group : stmt->grouping_sets) {
        grouping_sets.push_back(toExprList(group));
    }
    payload["grouping_sets"] = Value(std::move(grouping_sets));
    payload["grouping_type"] = Value(static_cast<uint64_t>(mapGroupingType(stmt->grouping_type)));

    if (stmt->having) {
        payload["having"] = Value(makeInstr(emitExpression(stmt->having)));
    }
    payload["order_by"] = toOrderBy(stmt->order_by);
    if (stmt->limit) {
        payload["limit"] = Value(makeInstr(emitExpression(stmt->limit)));
    }
    if (stmt->offset) {
        payload["offset"] = Value(makeInstr(emitExpression(stmt->offset)));
    }
    if (stmt->set_op != parser::v2::SetOpType::NONE && stmt->set_op_right) {
        Value::Object setop;
        setop["type"] = Value(static_cast<uint64_t>(stmt->set_op));
        setop["all"] = Value(stmt->set_op_all);
        setop["right"] = Value(makeInstr(emitSelect(stmt->set_op_right)));
        payload["set_op"] = Value(std::move(setop));
    }
    if (stmt->with) {
        Value::List ctes;
        for (const auto& cte : stmt->with->ctes) {
            Value::Object c;
            c["name"] = toIdent(cte.name);
            Value::List cols;
            for (auto id : cte.column_names) {
                cols.push_back(toIdent(id));
            }
            c["column_names"] = Value(std::move(cols));
            if (cte.query) {
                c["query"] = Value(makeInstr(emitStatement(cte.query)));
            }
            c["recursive"] = Value(cte.recursive || stmt->with->recursive);
            ctes.push_back(Value(std::move(c)));
        }
        payload["with"] = Value(std::move(ctes));
    }

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitInsert(parser::v2::InsertStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_INSERT);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->table_path);
    if (stmt->has_alias) {
        payload["alias"] = toIdent(stmt->alias);
    }
    Value::List cols;
    for (auto id : stmt->columns) cols.push_back(toIdent(id));
    payload["columns"] = Value(std::move(cols));

    uint8_t source = 1;
    if (stmt->source == parser::v2::InsertStmt::Source::SELECT) source = 2;
    if (stmt->source == parser::v2::InsertStmt::Source::DEFAULT) source = 3;
    payload["source"] = Value(static_cast<uint64_t>(source));

    if (stmt->source == parser::v2::InsertStmt::Source::VALUES) {
        Value::List rows;
        for (const auto& row : stmt->values_rows) {
            rows.push_back(toExprList(row));
        }
        payload["values"] = Value(std::move(rows));
    }
    if (stmt->source == parser::v2::InsertStmt::Source::SELECT && stmt->select_source) {
        payload["select"] = Value(makeInstr(emitSelect(stmt->select_source)));
    }
    if (stmt->on_conflict) {
        Value::Object oc;
        Value::List target_cols;
        for (auto id : stmt->on_conflict->columns) target_cols.push_back(toIdent(id));
        oc["target_cols"] = Value(std::move(target_cols));
        oc["action"] = Value(static_cast<uint64_t>(stmt->on_conflict->action == parser::v2::ConflictAction::UPDATE ? 2 : 1));
        Value::List assignments;
        for (const auto& item : stmt->on_conflict->set_items) {
            Value::Object a;
            a["column"] = emitColumnRefValue(item.first);
            a["value"] = Value(makeInstr(emitExpression(item.second)));
            assignments.push_back(Value(std::move(a)));
        }
        oc["assignments"] = Value(std::move(assignments));
        if (stmt->on_conflict->where_action) {
            oc["where"] = Value(makeInstr(emitExpression(stmt->on_conflict->where_action)));
        }
        payload["on_conflict"] = Value(std::move(oc));
    }

    Value::List returning;
    for (auto* item : stmt->returning) {
        if (item->item_type == parser::v2::SelectItem::Type::EXPRESSION && item->expr) {
            returning.push_back(Value(makeInstr(emitExpression(item->expr))));
        }
    }
    payload["returning"] = Value(std::move(returning));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitUpdate(parser::v2::UpdateStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_UPDATE);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->table_path);
    if (stmt->has_alias) payload["alias"] = toIdent(stmt->alias);

    Value::List assigns;
    for (const auto& item : stmt->set_items) {
        Value::Object a;
        a["column"] = emitColumnRefValue(item.first);
        a["value"] = Value(makeInstr(emitExpression(item.second)));
        assigns.push_back(Value(std::move(a)));
    }
    payload["set_items"] = Value(std::move(assigns));

    if (stmt->from) payload["from"] = toTableRef(stmt->from);
    payload["joins"] = toJoins(stmt->joins);
    if (stmt->where) payload["where"] = Value(makeInstr(emitExpression(stmt->where)));

    Value::List returning;
    for (auto* item : stmt->returning) {
        if (item->item_type == parser::v2::SelectItem::Type::EXPRESSION && item->expr) {
            returning.push_back(Value(makeInstr(emitExpression(item->expr))));
        }
    }
    payload["returning"] = Value(std::move(returning));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDelete(parser::v2::DeleteStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_DELETE);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->table_path);
    if (stmt->has_alias) payload["alias"] = toIdent(stmt->alias);
    if (stmt->using_clause) payload["using"] = toTableRef(stmt->using_clause);
    payload["using_joins"] = toJoins(stmt->using_joins);
    if (stmt->where) payload["where"] = Value(makeInstr(emitExpression(stmt->where)));

    Value::List returning;
    for (auto* item : stmt->returning) {
        if (item->item_type == parser::v2::SelectItem::Type::EXPRESSION && item->expr) {
            returning.push_back(Value(makeInstr(emitExpression(item->expr))));
        }
    }
    payload["returning"] = Value(std::move(returning));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitMerge(parser::v2::MergeStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_MERGE);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->target_table);
    if (stmt->target_alias != parser::v2::StringPool::INVALID_ID) {
        payload["target_alias"] = toIdent(stmt->target_alias);
    }
    if (stmt->source_query) {
        payload["source_query"] = Value(makeInstr(emitStatement(stmt->source_query)));
    } else if (!stmt->source_table.isEmpty()) {
        payload["source_table"] = toTableRefFromPath(stmt->source_table, stmt->source_alias);
    }
    if (stmt->source_alias != parser::v2::StringPool::INVALID_ID) {
        payload["source_alias"] = toIdent(stmt->source_alias);
    }
    if (stmt->on_condition) {
        payload["on"] = Value(makeInstr(emitExpression(stmt->on_condition)));
    }

    Value::List matched;
    for (const auto& action : stmt->when_matched) {
        Value::Object a;
        a["action"] = Value(static_cast<uint64_t>(action.is_delete ? 2 : 1));
        if (action.and_condition) {
            a["condition"] = Value(makeInstr(emitExpression(action.and_condition)));
        }
        Value::List assignments;
        for (const auto& item : action.assignments) {
            Value::Object as;
            as["column"] = emitColumnRefValue(item.first);
            as["value"] = Value(makeInstr(emitExpression(item.second)));
            assignments.push_back(Value(std::move(as)));
        }
        a["assignments"] = Value(std::move(assignments));
        matched.push_back(Value(std::move(a)));
    }
    payload["when_matched"] = Value(std::move(matched));

    Value::List not_matched;
    for (const auto& action : stmt->when_not_matched) {
        Value::Object a;
        a["action"] = Value(static_cast<uint64_t>(3));
        if (action.and_condition) {
            a["condition"] = Value(makeInstr(emitExpression(action.and_condition)));
        }
        Value::List cols;
        for (auto id : action.columns) cols.push_back(toIdent(id));
        a["insert_columns"] = Value(std::move(cols));
        a["insert_values"] = toExprList(action.values);
        not_matched.push_back(Value(std::move(a)));
    }
    payload["when_not_matched"] = Value(std::move(not_matched));

    Value::List not_matched_src;
    for (const auto& action : stmt->when_not_matched_by_source) {
        Value::Object a;
        a["action"] = Value(static_cast<uint64_t>(action.is_delete ? 2 : 1));
        if (action.and_condition) {
            a["condition"] = Value(makeInstr(emitExpression(action.and_condition)));
        }
        Value::List assignments;
        for (const auto& item : action.assignments) {
            Value::Object as;
            as["column"] = emitColumnRefValue(item.first);
            as["value"] = Value(makeInstr(emitExpression(item.second)));
            assignments.push_back(Value(std::move(as)));
        }
        a["assignments"] = Value(std::move(assignments));
        not_matched_src.push_back(Value(std::move(a)));
    }
    payload["when_not_matched_by_source"] = Value(std::move(not_matched_src));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitCopy(parser::v2::CopyStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_COPY);
    inst.flags = 0;

    Value::Object payload;
    payload["has_query"] = Value(stmt->query != nullptr);
    if (stmt->query) {
        payload["query"] = Value(makeInstr(emitSelect(stmt->query)));
    }
    if (!stmt->table_path.isEmpty()) {
        payload["target_table"] = toSchemaPath(stmt->table_path);
    }
    Value::List cols;
    for (auto id : stmt->columns) cols.push_back(toIdent(id));
    payload["columns"] = Value(std::move(cols));
    payload["direction"] = Value(static_cast<uint64_t>(stmt->direction == parser::v2::CopyStmt::Direction::FROM ? 1 : 2));
    if (!stmt->target_is_stdin && !stmt->target_is_stdout) {
        payload["filename"] = Value(std::string(pool_.get(stmt->target)));
    }
    payload["format"] = Value(static_cast<uint64_t>(stmt->options.format_set ? static_cast<uint8_t>(stmt->options.format) + 1 : 0));
    payload["options"] = Value(Value::Object{
        {"count", Value(uint64_t(0))},
        {"key", Value(std::string())},
        {"value", Value(makeInstr(emitLiteral(nullptr)))}});

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlCreate(parser::v2::Statement* stmt) {
    switch (stmt->kind()) {
        case parser::v2::ASTKind::CreateTableStmt: {
            auto* s = static_cast<parser::v2::CreateTableStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TABLE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->table_path);
            Value::List cols;
            for (auto* col : s->columns) cols.push_back(emitColumnDef(col));
            payload["columns"] = Value(std::move(cols));
            Value::List constraints;
            for (auto* c : s->constraints) constraints.push_back(emitTableConstraint(c));
            payload["constraints"] = Value(std::move(constraints));
            Value::List inherits;
            for (const auto& p : s->inherits) inherits.push_back(toSchemaPath(p));
            payload["inherits"] = Value(std::move(inherits));
            if (s->has_tablespace) payload["tablespace"] = toSchemaPath(s->tablespace);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateIndexStmt: {
            auto* s = static_cast<parser::v2::CreateIndexStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_INDEX);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["index_path"] = toSchemaPath(parser::v2::SchemaPath(parser::v2::PathType::UNQUALIFIED, {s->index_name}));
            payload["table"] = toSchemaPath(s->table_path);
            Value::List keys;
            for (const auto& key : s->columns) {
                Value::Object k;
                k["kind"] = Value(uint64_t(key.expr ? 2 : 1));
                if (key.expr) {
                    k["name_or_expr"] = Value(makeInstr(emitExpression(key.expr)));
                } else {
                    Instruction colref;
                    colref.opcode = op(Opcode::SBLR3_COLUMN_REF);
                    colref.flags = 0;
                    colref.payload = emitColumnRefValue(key.column);
                    k["name_or_expr"] = Value(makeInstr(colref));
                }
                k["order"] = Value(uint64_t(key.ascending ? 0 : 1));
                if (key.nulls_first) k["nulls"] = Value(uint64_t(1));
                else if (key.nulls_last) k["nulls"] = Value(uint64_t(2));
                if (key.opclass != parser::v2::StringPool::INVALID_ID) {
                    k["opclass"] = toIdent(key.opclass);
                }
                keys.push_back(Value(std::move(k)));
            }
            payload["keys"] = Value(std::move(keys));
            Value::List include;
            for (auto id : s->include_columns) include.push_back(toIdent(id));
            payload["include"] = Value(std::move(include));
            if (s->where_clause) payload["predicate"] = Value(makeInstr(emitExpression(s->where_clause)));
            payload["index_type"] = Value(indexTypeName(s->index_type));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateViewStmt: {
            auto* s = static_cast<parser::v2::CreateViewStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_VIEW);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->view_path);
            Value::List cols;
            for (auto id : s->column_names) cols.push_back(toIdent(id));
            payload["columns"] = Value(std::move(cols));
            if (s->query) payload["query"] = Value(makeInstr(emitStatement(s->query)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateSequenceStmt: {
            auto* s = static_cast<parser::v2::CreateSequenceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_SEQUENCE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->sequence_path);
            if (s->start_with) payload["start"] = Value(int64_t(*s->start_with));
            if (s->increment_by) payload["increment"] = Value(int64_t(*s->increment_by));
            if (s->min_value) payload["min_value"] = Value(int64_t(*s->min_value));
            if (s->max_value) payload["max_value"] = Value(int64_t(*s->max_value));
            if (s->cache) payload["cache"] = Value(uint64_t(*s->cache));
            if (s->cycle) payload["cycle"] = Value(true);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateSchemaStmt: {
            auto* s = static_cast<parser::v2::CreateSchemaStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_SCHEMA);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->schema_path);
            if (s->has_owner) payload["owner"] = toIdent(s->owner);
            payload["path_list"] = Value(Value::List{});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateDatabaseStmt: {
            auto* s = static_cast<parser::v2::CreateDatabaseStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_DATABASE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["name"] = toIdent(s->database_path.objectName());
            payload["encrypted"] = Value(false);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateTablespaceStmt: {
            auto* s = static_cast<parser::v2::CreateTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(0));
            payload["name"] = toIdent(s->tablespace_name);
            payload["location"] = Value(std::string(s->location));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateFunctionStmt: {
            auto* s = static_cast<parser::v2::CreateFunctionStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FUNCTION_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->function_path.objectName());
            Value::List params;
            for (const auto& p : s->params) {
                Value::Object param;
                param["name"] = toIdent(p.name);
                param["type"] = Value(buildTypeSpec(p.type));
                param["mode"] = Value(uint64_t(static_cast<uint8_t>(p.mode)));
                if (p.has_default && p.default_value) {
                    param["default_expr"] = Value(makeInstr(emitExpression(p.default_value)));
                }
                params.push_back(Value(std::move(param)));
            }
            payload["params"] = Value(std::move(params));
            payload["return_type"] = Value(buildTypeSpec(s->return_type));
            payload["language"] = Value(std::string("SQL"));
            if (s->body != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateProcedureStmt: {
            auto* s = static_cast<parser::v2::CreateProcedureStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_PROCEDURE_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->procedure_path.objectName());
            Value::List params;
            for (const auto& p : s->params) {
                Value::Object param;
                param["name"] = toIdent(p.name);
                param["type"] = Value(buildTypeSpec(p.type));
                param["mode"] = Value(uint64_t(static_cast<uint8_t>(p.mode)));
                if (p.has_default && p.default_value) {
                    param["default_expr"] = Value(makeInstr(emitExpression(p.default_value)));
                }
                params.push_back(Value(std::move(param)));
            }
            payload["params"] = Value(std::move(params));
            payload["language"] = Value(std::string("SQL"));
            if (s->body != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateTriggerStmt: {
            auto* s = static_cast<parser::v2::CreateTriggerStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TRIGGER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->trigger_name);
            payload["table"] = toSchemaPath(s->table_path);
            payload["timing"] = Value(uint64_t(static_cast<uint8_t>(s->timing)));
            payload["event_mask"] = Value(uint64_t(s->event_mask));
            payload["for_each_row"] = Value(s->granularity == parser::v2::TriggerGranularity::FOR_EACH_ROW);
            if (s->body != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreatePackageStmt: {
            auto* s = static_cast<parser::v2::CreatePackageStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_PACKAGE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->package_path.objectName());
            if (s->header != parser::v2::StringPool::INVALID_ID) {
                std::string header(pool_.get(s->header));
                payload["spec"] = Value(Value::Bytes(header.begin(), header.end()));
            }
            if (s->body != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateUserStmt: {
            auto* s = static_cast<parser::v2::CreateUserStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_USER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->user_name);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateRoleStmt: {
            auto* s = static_cast<parser::v2::CreateRoleStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_ROLE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->role_name);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateGroupStmt: {
            auto* s = static_cast<parser::v2::CreateGroupStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_GROUP);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->group_name);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreatePolicyStmt: {
            auto* s = static_cast<parser::v2::CreatePolicyStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_POLICY);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->policy_name);
            payload["table"] = toSchemaPath(s->table_path);
            payload["event_mask"] = Value(uint64_t(static_cast<uint8_t>(s->policy_type)));
            if (s->using_expr) payload["using_expr"] = Value(makeInstr(emitExpression(s->using_expr)));
            if (s->with_check_expr) payload["check_expr"] = Value(makeInstr(emitExpression(s->with_check_expr)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateForeignServerStmt: {
            auto* s = static_cast<parser::v2::CreateForeignServerStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FOREIGN_SERVER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->server_name);
            if (s->has_server_type) {
                payload["type"] = Value(std::string(s->server_type));
            } else {
                payload["type"] = toIdent(s->fdw_name);
            }
            if (s->has_server_version) {
                payload["host"] = Value(std::string(s->server_version));
            } else {
                payload["host"] = Value(std::string());
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateForeignDataWrapperStmt: {
            auto* s = static_cast<parser::v2::CreateForeignDataWrapperStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FOREIGN_DATA_WRAPPER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->wrapper_name);
            if (s->has_handler && s->handler_name != parser::v2::StringPool::INVALID_ID) {
                payload["handler"] = toIdent(s->handler_name);
            }
            if (s->has_validator && s->validator_name != parser::v2::StringPool::INVALID_ID) {
                payload["validator"] = toIdent(s->validator_name);
            }
            uint64_t opt_count = s->options.size();
            std::string opt_key;
            Instruction opt_value = emitLiteral(nullptr);
            if (!s->options.empty()) {
                const auto& opt = s->options.front();
                opt_key = opt.key;
                opt_value = makeStringLiteralInstr(opt.value);
            }
            payload["options"] = makeOptionKvPlaceholder(opt_count, opt_key, opt_value);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateForeignTableStmt: {
            auto* s = static_cast<parser::v2::CreateForeignTableStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FOREIGN_TABLE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toSchemaPath(s->table_path);
            payload["server"] = toIdent(s->server_name);
            Value::List cols;
            for (const auto& c : s->columns) {
                parser::v2::ColumnDef def;
                def.name = c.name;
                def.type = c.type;
                cols.push_back(emitColumnDef(&def));
            }
            payload["columns"] = Value(std::move(cols));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateUserMappingStmt: {
            auto* s = static_cast<parser::v2::CreateUserMappingStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_USER_MAPPING);
            inst.flags = 0;
            Value::Object payload;
            payload["server"] = toIdent(s->server_name);
            if (s->target == parser::v2::UserMappingTarget::PUBLIC_ROLE) {
                payload["user"] = Value(std::string("PUBLIC"));
            } else {
                payload["user"] = toIdent(s->user_name);
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateSynonymStmt: {
            auto* s = static_cast<parser::v2::CreateSynonymStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_SYNONYM);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toSchemaPath(s->synonym_path);
            payload["target"] = toSchemaPath(s->target_path);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateUdrStmt: {
            auto* s = static_cast<parser::v2::CreateUdrStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_UDR);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->udr_path.objectName());
            payload["library_path"] = Value(std::string(s->library_path));
            payload["entry_point"] = Value(std::string(s->entry_point));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateJobStmt: {
            auto* s = static_cast<parser::v2::CreateJobStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_JOB);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->job_name);
            if (s->schedule_kind == parser::v2::JobScheduleKind::CRON) {
                payload["schedule"] = Value(std::string(pool_.get(s->cron_expression)));
            } else if (s->schedule_kind == parser::v2::JobScheduleKind::AT) {
                payload["schedule"] = Value(std::string(pool_.get(s->at_timestamp)));
            } else {
                payload["schedule"] = Value(std::to_string(s->interval_seconds));
            }
            if (s->job_type == parser::v2::JobType::SQL && s->job_sql != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->job_sql));
                payload["command"] = Value(Value::Bytes(body.begin(), body.end()));
            } else if (s->job_type == parser::v2::JobType::PROCEDURE &&
                       s->procedure_name != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->procedure_name));
                payload["command"] = Value(Value::Bytes(body.begin(), body.end()));
            } else if (s->job_type == parser::v2::JobType::EXTERNAL &&
                       s->external_command != parser::v2::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->external_command));
                payload["command"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateExceptionStmt: {
            auto* s = static_cast<parser::v2::CreateExceptionStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_EXCEPTION_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->exception_path.objectName());
            if (s->message != parser::v2::StringPool::INVALID_ID) {
                payload["message"] = Value(std::string(pool_.get(s->message)));
            } else {
                payload["message"] = Value(std::string());
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateDomainStmt: {
            auto* s = static_cast<parser::v2::CreateDomainStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_DOMAIN);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->domain_path.objectName());
            payload["type"] = Value(buildTypeSpec(s->base_type));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CreateTypeStmt: {
            auto* s = static_cast<parser::v2::CreateTypeStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TYPE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->type_path.objectName());
            payload["type"] = Value(TypeSpec{});
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.flags = 0;
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlAlter(parser::v2::Statement* stmt) {
    auto encodePayload = [&](const scratchbird::sblr::v3::SchemaDef& schema,
                             const scratchbird::sblr::v3::Value::Object& obj) -> Value::Bytes {
        Buffer out;
        DecodeError err;
        if (!scratchbird::sblr::v3::encodePayloadBySchema(schema, Value(obj), out, err)) {
            fail(err.message);
            return Value::Bytes{};
        }
        return out;
    };
    auto makeStringLiteralInstr = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto makeBoolLiteralInstr = [&](bool value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto makeDoubleLiteralInstr = [&](double value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_DOUBLE);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto makeOptionKvPlaceholder = [&](uint64_t count,
                                       const std::string& key,
                                       const Instruction& value_instr) {
        return Value(Value::Object{
            {"count", Value(count)},
            {"key", Value(key)},
            {"value", Value(makeInstr(value_instr))},
        });
    };
    switch (stmt->kind()) {
        case parser::v2::ASTKind::AlterTableStmt: {
            auto* s = static_cast<parser::v2::AlterTableStmt*>(stmt);
            if (s->action == parser::v2::AlterTableAction::SET_TABLESPACE) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_ALTER_TABLE_SET_TABLESPACE);
                inst.flags = 0;
                Value::Object payload;
                payload["table"] = toSchemaPath(s->table_path);
                payload["tablespace"] = toSchemaPath(s->tablespace);
                payload["options"] = Value(Value::Object{
                    {"count", Value(uint64_t(0))},
                    {"key", Value(std::string())},
                    {"value", Value(makeInstr(emitLiteral(nullptr)))}});
                inst.payload = Value(std::move(payload));
                return inst;
            }
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_TABLE);
            inst.flags = 0;
            Value::Object payload;
            payload["table"] = toSchemaPath(s->table_path);
            payload["if_exists"] = Value(s->if_exists);
            payload["only"] = Value(s->only);
            uint8_t action_code = 0;
            switch (s->action) {
                case parser::v2::AlterTableAction::ADD_COLUMN: action_code = 1; break;
                case parser::v2::AlterTableAction::ADD_CONSTRAINT: action_code = 2; break;
                case parser::v2::AlterTableAction::DROP_COLUMN: action_code = 3; break;
                case parser::v2::AlterTableAction::DROP_CONSTRAINT: action_code = 4; break;
                case parser::v2::AlterTableAction::ALTER_COLUMN: action_code = 5; break;
                case parser::v2::AlterTableAction::ALTER_COLUMN_POSITION: action_code = 6; break;
                case parser::v2::AlterTableAction::ALTER_COLUMN_SET_DEFAULT: action_code = 7; break;
                case parser::v2::AlterTableAction::ALTER_COLUMN_DROP_DEFAULT: action_code = 8; break;
                case parser::v2::AlterTableAction::ALTER_COLUMN_SET_NOT_NULL: action_code = 9; break;
                case parser::v2::AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL: action_code = 10; break;
                case parser::v2::AlterTableAction::RENAME_TABLE: action_code = 11; break;
                case parser::v2::AlterTableAction::RENAME_CONSTRAINT: action_code = 12; break;
                case parser::v2::AlterTableAction::SET_SCHEMA: action_code = 13; break;
                case parser::v2::AlterTableAction::RENAME_COLUMN: action_code = 15; break;
                case parser::v2::AlterTableAction::SET_STATISTICS: action_code = 16; break;
                case parser::v2::AlterTableAction::SET_STORAGE: action_code = 17; break;
                case parser::v2::AlterTableAction::INHERIT: action_code = 18; break;
                case parser::v2::AlterTableAction::NO_INHERIT: action_code = 19; break;
                case parser::v2::AlterTableAction::ENABLE_TRIGGER: action_code = 20; break;
                case parser::v2::AlterTableAction::DISABLE_TRIGGER: action_code = 21; break;
                case parser::v2::AlterTableAction::ENABLE_RLS: action_code = 22; break;
                case parser::v2::AlterTableAction::DISABLE_RLS: action_code = 23; break;
                case parser::v2::AlterTableAction::FORCE_RLS: action_code = 24; break;
                case parser::v2::AlterTableAction::NO_FORCE_RLS: action_code = 25; break;
                case parser::v2::AlterTableAction::ATTACH_PARTITION: action_code = 26; break;
                case parser::v2::AlterTableAction::DETACH_PARTITION: action_code = 27; break;
                case parser::v2::AlterTableAction::VALIDATE_CONSTRAINT: action_code = 28; break;
                default:
                    action_code = 0;
                    break;
            }
            payload["action"] = Value(uint64_t(action_code));
            Value::Bytes action_payload;
            switch (s->action) {
                case parser::v2::AlterTableAction::ADD_COLUMN: {
                    if (s->column) {
                        auto col = emitColumnDef(s->column);
                        if (auto obj = std::get_if<Value::Object>(&col.data)) {
                            if (const auto* schema = scratchbird::sblr::v3::lookupSchema("COLUMN_DEF")) {
                                action_payload = encodePayload(*schema, *obj);
                            }
                        }
                    }
                    break;
                }
                case parser::v2::AlterTableAction::DROP_COLUMN: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_COLUMN", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"cascade", scratchbird::sblr::v3::FieldType::BOOL, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["cascade"] = Value(s->cascade);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ALTER_COLUMN: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_ALTER_COLUMN_TYPE", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"type", scratchbird::sblr::v3::FieldType::TYPE_SPEC, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    if (s->column) {
                        obj["type"] = Value(buildTypeSpec(s->column->type));
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ALTER_COLUMN_POSITION: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_ALTER_COLUMN_POSITION", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"position_1_based", scratchbird::sblr::v3::FieldType::U32, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["position_1_based"] = Value(uint64_t(s->position_1_based));
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ALTER_COLUMN_SET_DEFAULT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_DEFAULT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"default", scratchbird::sblr::v3::FieldType::EXPR, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    if (s->default_expr) {
                        obj["default"] = Value(makeInstr(emitExpression(s->default_expr)));
                    } else {
                        obj["default"] = Value(makeInstr(emitLiteral(nullptr)));
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ALTER_COLUMN_DROP_DEFAULT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_DEFAULT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ALTER_COLUMN_SET_NOT_NULL: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_NOT_NULL", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_NOT_NULL", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::RENAME_COLUMN: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_RENAME_COLUMN", {
                        scratchbird::sblr::v3::FieldDef{"old_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"new_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["old_name"] = toIdent(s->column_name);
                    obj["new_name"] = toIdent(s->new_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ADD_CONSTRAINT: {
                    if (s->constraint) {
                        auto cons = emitTableConstraint(s->constraint);
                        if (auto obj = std::get_if<Value::Object>(&cons.data)) {
                            if (const auto* schema = scratchbird::sblr::v3::lookupSchema("TABLE_CONSTRAINT")) {
                                action_payload = encodePayload(*schema, *obj);
                            }
                        }
                    }
                    break;
                }
                case parser::v2::AlterTableAction::DROP_CONSTRAINT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_CONSTRAINT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"cascade", scratchbird::sblr::v3::FieldType::BOOL, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->constraint_name);
                    obj["cascade"] = Value(s->cascade);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::RENAME_CONSTRAINT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_RENAME_CONSTRAINT", {
                        scratchbird::sblr::v3::FieldDef{"old_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"new_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["old_name"] = toIdent(s->constraint_name);
                    obj["new_name"] = toIdent(s->new_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::RENAME_TABLE: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_RENAME_TABLE", {
                        scratchbird::sblr::v3::FieldDef{"new_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["new_name"] = toIdent(s->new_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::SET_SCHEMA: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_SCHEMA", {
                        scratchbird::sblr::v3::FieldDef{"schema", scratchbird::sblr::v3::FieldType::SCHEMA_PATH, ""},
                    }};
                    Value::Object obj;
                    obj["schema"] = toSchemaPath(s->target_schema);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::SET_STATISTICS: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_STATISTICS", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"target", scratchbird::sblr::v3::FieldType::I32, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["target"] = Value(int64_t(s->statistics_target));
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::SET_STORAGE: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_STORAGE", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"storage", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["storage"] = toIdent(s->storage_type);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::INHERIT:
                case parser::v2::AlterTableAction::NO_INHERIT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_INHERIT", {
                        scratchbird::sblr::v3::FieldDef{"parent", scratchbird::sblr::v3::FieldType::OPT, "schema_path"},
                    }};
                    Value::Object obj;
                    if (s->has_inherit_parent) {
                        obj["parent"] = toSchemaPath(s->inherit_parent);
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ENABLE_TRIGGER:
                case parser::v2::AlterTableAction::DISABLE_TRIGGER: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_TRIGGER_TOGGLE", {
                        scratchbird::sblr::v3::FieldDef{"trigger_all", scratchbird::sblr::v3::FieldType::BOOL, ""},
                        scratchbird::sblr::v3::FieldDef{"trigger_name", scratchbird::sblr::v3::FieldType::OPT, "ident"},
                    }};
                    Value::Object obj;
                    obj["trigger_all"] = Value(s->trigger_all);
                    if (!s->trigger_all && s->trigger_name != parser::v2::StringPool::INVALID_ID) {
                        obj["trigger_name"] = toIdent(s->trigger_name);
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::ENABLE_RLS:
                case parser::v2::AlterTableAction::DISABLE_RLS:
                case parser::v2::AlterTableAction::FORCE_RLS:
                case parser::v2::AlterTableAction::NO_FORCE_RLS: {
                    action_payload = Value::Bytes{};
                    break;
                }
                case parser::v2::AlterTableAction::ATTACH_PARTITION: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_ATTACH_PARTITION", {
                        scratchbird::sblr::v3::FieldDef{"partition", scratchbird::sblr::v3::FieldType::SCHEMA_PATH, ""},
                        scratchbird::sblr::v3::FieldDef{"bounds", scratchbird::sblr::v3::FieldType::OPT, "string"},
                    }};
                    Value::Object obj;
                    obj["partition"] = toSchemaPath(s->partition_path);
                    if (s->has_partition_bounds) {
                        obj["bounds"] = Value(stringPool().get(s->partition_bounds));
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::DETACH_PARTITION: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DETACH_PARTITION", {
                        scratchbird::sblr::v3::FieldDef{"partition", scratchbird::sblr::v3::FieldType::SCHEMA_PATH, ""},
                    }};
                    Value::Object obj;
                    obj["partition"] = toSchemaPath(s->partition_path);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v2::AlterTableAction::VALIDATE_CONSTRAINT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_VALIDATE_CONSTRAINT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->constraint_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                default:
                    break;
            }
            payload["payload"] = Value(std::move(action_payload));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterSequenceStmt: {
            auto* s = static_cast<parser::v2::AlterSequenceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_SEQUENCE);
            inst.flags = 0;
            Value::Object payload;
            payload["path"] = toSchemaPath(s->sequence_path);
            if (s->restart_with) payload["start"] = Value(int64_t(*s->restart_with));
            if (s->increment_by) payload["increment"] = Value(int64_t(*s->increment_by));
            if (s->min_value) payload["min_value"] = Value(int64_t(*s->min_value));
            if (s->max_value) payload["max_value"] = Value(int64_t(*s->max_value));
            if (s->cycle) payload["cycle"] = Value(*s->cycle);
            if (s->cache) payload["cache"] = Value(uint64_t(*s->cache));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterIndexStmt: {
            auto* s = static_cast<parser::v2::AlterIndexStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_INDEX);
            inst.flags = 0;
            Value::Object payload;
            payload["index"] = toSchemaPath(s->index_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            uint64_t option_count = 0;
            std::string option_key;
            Instruction option_value = emitLiteral(nullptr);
            if (s->action == parser::v2::AlterIndexAction::SET_OPTIONS) {
                if (s->options.bloom_filter_set) {
                    option_count++;
                    option_key = "bloom_filter";
                    option_value = makeBoolLiteralInstr(s->options.bloom_filter_enabled);
                }
                if (s->options.bloom_fpr_set) {
                    if (option_count == 0) {
                        option_key = "bloom_fpr";
                        option_value = makeDoubleLiteralInstr(s->options.bloom_fpr);
                    }
                    option_count++;
                }
            }
            payload["options"] = makeOptionKvPlaceholder(option_count, option_key, option_value);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterSchemaStmt: {
            auto* s = static_cast<parser::v2::AlterSchemaStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_SCHEMA);
            inst.flags = 0;
            Value::Object payload;
            payload["schema"] = toSchemaPath(s->schema_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (s->new_name != parser::v2::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            if (s->owner != parser::v2::StringPool::INVALID_ID) payload["owner"] = toIdent(s->owner);
            if (!s->new_path.components.empty()) payload["new_path"] = toSchemaPath(s->new_path);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterDatabaseStmt: {
            auto* s = static_cast<parser::v2::AlterDatabaseStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_DATABASE);
            inst.flags = 0;
            Value::Object payload;
            payload["database"] = toSchemaPath(s->database_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (s->new_name != parser::v2::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            if (s->owner != parser::v2::StringPool::INVALID_ID) payload["owner"] = toIdent(s->owner);
            if (s->alias != parser::v2::StringPool::INVALID_ID) payload["alias"] = toIdent(s->alias);
            uint64_t opt_count = s->options.size();
            std::string opt_key;
            Instruction opt_value = emitLiteral(nullptr);
            if (!s->options.empty()) {
                const auto& opt = s->options.front();
                if (opt.key != parser::v2::StringPool::INVALID_ID) {
                    opt_key = std::string(pool_.get(opt.key));
                }
                if (opt.value != parser::v2::StringPool::INVALID_ID) {
                    opt_value = makeStringLiteralInstr(std::string(pool_.get(opt.value)));
                }
            }
            payload["options"] = makeOptionKvPlaceholder(opt_count, opt_key, opt_value);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterTablespaceStmt: {
            auto* s = static_cast<parser::v2::AlterTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            parser::v2::SchemaPath path;
            if (s->tablespace_name != parser::v2::StringPool::INVALID_ID) {
                path.components.push_back(s->tablespace_name);
            }
            payload["tablespace"] = toSchemaPath(path);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AttachTablespaceStmt: {
            auto* s = static_cast<parser::v2::AttachTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ATTACH_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->tablespace_name);
            payload["location"] = Value(std::string(s->location));
            payload["validate"] = Value(s->validate);
            payload["allow_mismatch"] = Value(s->allow_mismatch);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::DetachTablespaceStmt: {
            auto* s = static_cast<parser::v2::DetachTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_DETACH_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->tablespace_name);
            payload["force"] = Value(s->force);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterDomainStmt: {
            auto* s = static_cast<parser::v2::AlterDomainStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_DOMAIN);
            inst.flags = 0;
            Value::Object payload;
            payload["domain"] = toSchemaPath(s->domain_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (!s->value.empty()) payload["value"] = Value(s->value);
            if (s->constraint_name != parser::v2::StringPool::INVALID_ID) payload["constraint"] = toIdent(s->constraint_name);
            if (s->new_name != parser::v2::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterTypeStmt: {
            auto* s = static_cast<parser::v2::AlterTypeStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_TYPE);
            inst.flags = 0;
            Value::Object payload;
            payload["type"] = toSchemaPath(s->type_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (s->new_name != parser::v2::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            if (s->new_schema != parser::v2::StringPool::INVALID_ID) payload["new_schema"] = toIdent(s->new_schema);
            if (s->value_label != parser::v2::StringPool::INVALID_ID) payload["value_label"] = toIdent(s->value_label);
            if (s->before_label != parser::v2::StringPool::INVALID_ID) payload["before_label"] = toIdent(s->before_label);
            if (s->after_label != parser::v2::StringPool::INVALID_ID) payload["after_label"] = toIdent(s->after_label);
            if (s->old_label != parser::v2::StringPool::INVALID_ID) payload["old_label"] = toIdent(s->old_label);
            if (s->new_label != parser::v2::StringPool::INVALID_ID) payload["new_label"] = toIdent(s->new_label);
            payload["is_range_options"] = Value(s->is_range_options);
            payload["is_base_options"] = Value(s->is_base_options);
            if (s->is_range_options) {
                Value::Object range;
                if (s->range_options.has_subtype) {
                    range["subtype"] = Value(buildTypeSpec(s->range_options.subtype));
                }
                if (s->range_options.has_subtype_collation) range["subtype_collation"] = Value(s->range_options.subtype_collation);
                if (s->range_options.has_subtype_opclass) range["subtype_opclass"] = Value(s->range_options.subtype_opclass);
                if (s->range_options.has_canonical) range["canonical"] = Value(s->range_options.canonical);
                if (s->range_options.has_subtype_diff) range["subtype_diff"] = Value(s->range_options.subtype_diff);
                if (s->range_options.has_multirange) range["multirange"] = Value(s->range_options.multirange);
                payload["range_options"] = Value(std::move(range));
            }
            if (s->is_base_options) {
                Value::Object base;
                if (s->base_options.has_storage) base["storage"] = Value(buildTypeSpec(s->base_options.storage));
                if (!s->base_options.input_function.empty()) base["input_function"] = Value(s->base_options.input_function);
                if (!s->base_options.output_function.empty()) base["output_function"] = Value(s->base_options.output_function);
                if (s->base_options.has_receive) base["receive_function"] = Value(s->base_options.receive_function);
                if (s->base_options.has_send) base["send_function"] = Value(s->base_options.send_function);
                if (s->base_options.has_typmod_in) base["typmod_in_function"] = Value(s->base_options.typmod_in_function);
                if (s->base_options.has_typmod_out) base["typmod_out_function"] = Value(s->base_options.typmod_out_function);
                if (s->base_options.has_analyze) base["analyze_function"] = Value(s->base_options.analyze_function);
                if (s->base_options.has_alignment) base["alignment"] = Value(uint64_t(static_cast<uint8_t>(s->base_options.alignment)));
                if (s->base_options.has_storage_mode) base["storage_mode"] = Value(uint64_t(static_cast<uint8_t>(s->base_options.storage_mode)));
                if (s->base_options.has_category) base["category"] = Value(uint64_t(static_cast<uint8_t>(s->base_options.category)));
                if (s->base_options.has_preferred) base["preferred"] = Value(s->base_options.preferred);
                payload["base_options"] = Value(std::move(base));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterPolicyStmt: {
            auto* s = static_cast<parser::v2::AlterPolicyStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_POLICY);
            inst.flags = 0;
            Value::Object payload;
            payload["policy_name"] = toIdent(s->policy_name);
            payload["table"] = toSchemaPath(s->table_path);
            Value::List roles;
            for (auto id : s->roles) roles.push_back(toIdent(id));
            payload["roles"] = Value(std::move(roles));
            if (s->using_expr) payload["using_expr"] = Value(makeInstr(emitExpression(s->using_expr)));
            if (s->with_check_expr) payload["check_expr"] = Value(makeInstr(emitExpression(s->with_check_expr)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterSystemStmt: {
            auto* s = static_cast<parser::v2::AlterSystemStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_SYSTEM);
            inst.flags = 0;
            Value::Object payload;
            payload["key"] = toIdent(s->name);
            if (s->value) payload["value"] = Value(makeInstr(emitExpression(s->value)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterJobStmt: {
            auto* s = static_cast<parser::v2::AlterJobStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_JOB);
            inst.flags = 0;
            Value::Object payload;
            payload["job_name"] = toIdent(s->job_name);
            if (s->has_schedule) {
                payload["schedule_kind"] = Value(uint64_t(static_cast<uint8_t>(s->schedule_kind)));
                if (s->schedule_kind == parser::v2::JobScheduleKind::CRON &&
                    s->cron_expression != parser::v2::StringPool::INVALID_ID) {
                    payload["cron_expression"] = toIdent(s->cron_expression);
                }
                if (s->schedule_kind == parser::v2::JobScheduleKind::AT &&
                    s->at_timestamp != parser::v2::StringPool::INVALID_ID) {
                    payload["at_timestamp"] = toIdent(s->at_timestamp);
                }
                if (s->schedule_kind == parser::v2::JobScheduleKind::EVERY) {
                    payload["interval_seconds"] = Value(int64_t(s->interval_seconds));
                }
                if (s->starts_at != parser::v2::StringPool::INVALID_ID) payload["starts_at"] = toIdent(s->starts_at);
                if (s->ends_at != parser::v2::StringPool::INVALID_ID) payload["ends_at"] = toIdent(s->ends_at);
            }
            if (s->has_job_body) {
                payload["job_type"] = Value(uint64_t(static_cast<uint8_t>(s->job_type)));
                if (s->job_type == parser::v2::JobType::SQL && s->job_sql != parser::v2::StringPool::INVALID_ID) {
                    payload["job_sql"] = Value(std::string(pool_.get(s->job_sql)));
                }
                if (s->job_type == parser::v2::JobType::PROCEDURE &&
                    s->procedure_name != parser::v2::StringPool::INVALID_ID) {
                    payload["procedure_name"] = toIdent(s->procedure_name);
                }
                if (s->job_type == parser::v2::JobType::EXTERNAL &&
                    s->external_command != parser::v2::StringPool::INVALID_ID) {
                    payload["external_command"] = Value(std::string(pool_.get(s->external_command)));
                }
            }
            if (s->has_state) payload["state"] = Value(uint64_t(static_cast<uint8_t>(s->state)));
            if (s->has_max_retries) payload["max_retries"] = Value(uint64_t(s->max_retries));
            if (s->has_retry_backoff) payload["retry_backoff_seconds"] = Value(uint64_t(s->retry_backoff_seconds));
            if (s->has_timeout) payload["timeout_seconds"] = Value(uint64_t(s->timeout_seconds));
            if (s->has_on_completion) payload["on_completion"] = Value(uint64_t(static_cast<uint8_t>(s->on_completion)));
            if (s->has_run_as && s->run_as_role != parser::v2::StringPool::INVALID_ID) payload["run_as_role"] = toIdent(s->run_as_role);
            if (s->has_description && s->description != parser::v2::StringPool::INVALID_ID) {
                payload["description"] = Value(std::string(pool_.get(s->description)));
            }
            if (s->has_job_class && s->job_class != parser::v2::StringPool::INVALID_ID) payload["job_class"] = toIdent(s->job_class);
            if (s->has_partition) {
                if (s->partition_strategy != parser::v2::StringPool::INVALID_ID) payload["partition_strategy"] = toIdent(s->partition_strategy);
                if (s->partition_expression != parser::v2::StringPool::INVALID_ID) payload["partition_expression"] = toIdent(s->partition_expression);
                if (s->partition_shard != parser::v2::StringPool::INVALID_ID) payload["partition_shard"] = toIdent(s->partition_shard);
            }
            Value::List depends;
            for (auto id : s->depends_on) depends.push_back(toIdent(id));
            payload["depends_on"] = Value(std::move(depends));
            payload["clear_depends_on"] = Value(s->clear_depends_on);
            if (s->has_secret) {
                if (s->secret_key != parser::v2::StringPool::INVALID_ID) payload["secret_key"] = toIdent(s->secret_key);
                if (s->secret_value != parser::v2::StringPool::INVALID_ID) {
                    payload["secret_value"] = Value(std::string(pool_.get(s->secret_value)));
                }
            }
            payload["drop_secret"] = Value(s->drop_secret);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::RenameObjectStmt: {
            auto* s = static_cast<parser::v2::RenameObjectStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_RENAME_OBJECT);
            inst.flags = 0;
            Value::Object payload;
            payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(s->object_type)));
            payload["object_path"] = toSchemaPath(s->object_path);
            payload["new_name"] = toIdent(s->new_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::MoveObjectStmt: {
            auto* s = static_cast<parser::v2::MoveObjectStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_MOVE_OBJECT);
            inst.flags = 0;
            Value::Object payload;
            payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(s->object_type)));
            payload["object_path"] = toSchemaPath(s->object_path);
            if (s->has_new_name) {
                payload["new_name"] = toIdent(s->new_name);
            } else if (!s->target_schema.components.empty()) {
                payload["new_name"] = toIdent(s->target_schema.components.back());
            } else {
                payload["new_name"] = Value(std::string());
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.flags = 0;
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlDrop(parser::v2::Statement* stmt) {
    auto makeDrop = [&](Opcode opcode, const parser::v2::SchemaPath& path, uint8_t object_type) {
        Instruction inst;
        inst.opcode = op(opcode);
        inst.flags = 0;
        Value::Object payload;
        payload["flags"] = Value(uint64_t(0));
        payload["object_type"] = Value(uint64_t(object_type));
        payload["path"] = toSchemaPath(path);
        inst.payload = Value(std::move(payload));
        return inst;
    };

    switch (stmt->kind()) {
        case parser::v2::ASTKind::DropTableStmt:
            return makeDrop(Opcode::SBLR3_DROP_TABLE, static_cast<parser::v2::DropTableStmt*>(stmt)->tables.front(), 1);
        case parser::v2::ASTKind::DropIndexStmt:
            return makeDrop(Opcode::SBLR3_DROP_INDEX, static_cast<parser::v2::DropIndexStmt*>(stmt)->indexes.front(), 2);
        case parser::v2::ASTKind::DropViewStmt:
            return makeDrop(Opcode::SBLR3_DROP_VIEW, static_cast<parser::v2::DropViewStmt*>(stmt)->views.front(), 3);
        case parser::v2::ASTKind::DropSequenceStmt:
            return makeDrop(Opcode::SBLR3_DROP_SEQUENCE, static_cast<parser::v2::DropSequenceStmt*>(stmt)->sequences.front(), 4);
        case parser::v2::ASTKind::DropSchemaStmt:
            return makeDrop(Opcode::SBLR3_DROP_SCHEMA, static_cast<parser::v2::DropSchemaStmt*>(stmt)->schemas.front(), 5);
        case parser::v2::ASTKind::DropDatabaseStmt:
            return makeDrop(Opcode::SBLR3_DROP_DATABASE, static_cast<parser::v2::DropDatabaseStmt*>(stmt)->database_path, 6);
        case parser::v2::ASTKind::DropTablespaceStmt:
            return makeDrop(Opcode::SBLR3_DROP_TABLESPACE, static_cast<parser::v2::DropTablespaceStmt*>(stmt)->tablespaces.front(), 16);
        case parser::v2::ASTKind::DropDomainStmt:
            return makeDrop(Opcode::SBLR3_DROP_DOMAIN, static_cast<parser::v2::DropDomainStmt*>(stmt)->domains.front(), 7);
        case parser::v2::ASTKind::DropTypeStmt:
            return makeDrop(Opcode::SBLR3_DROP_TYPE, static_cast<parser::v2::DropTypeStmt*>(stmt)->types.front(), 8);
        case parser::v2::ASTKind::DropFunctionStmt:
            return makeDrop(Opcode::SBLR3_DROP_FUNCTION_STMT, static_cast<parser::v2::DropFunctionStmt*>(stmt)->functions.front(), 9);
        case parser::v2::ASTKind::DropProcedureStmt:
            return makeDrop(Opcode::SBLR3_DROP_PROCEDURE_STMT, static_cast<parser::v2::DropProcedureStmt*>(stmt)->procedures.front(), 10);
        case parser::v2::ASTKind::DropTriggerStmt:
            return makeDrop(Opcode::SBLR3_DROP_TRIGGER, static_cast<parser::v2::DropTriggerStmt*>(stmt)->triggers.front(), 11);
        case parser::v2::ASTKind::DropPackageStmt:
            return makeDrop(Opcode::SBLR3_DROP_PACKAGE_STMT, static_cast<parser::v2::DropPackageStmt*>(stmt)->packages.front(), 12);
        case parser::v2::ASTKind::DropRoleStmt:
            return makeDrop(Opcode::SBLR3_DROP_ROLE, static_cast<parser::v2::DropRoleStmt*>(stmt)->roles.front(), 13);
        case parser::v2::ASTKind::DropGroupStmt:
            return makeDrop(Opcode::SBLR3_DROP_GROUP, static_cast<parser::v2::DropGroupStmt*>(stmt)->groups.front(), 15);
        case parser::v2::ASTKind::DropUserStmt:
            return makeDrop(Opcode::SBLR3_DROP_USER, static_cast<parser::v2::DropUserStmt*>(stmt)->users.front(), 14);
        case parser::v2::ASTKind::DropExceptionStmt:
            return makeDrop(Opcode::SBLR3_DROP_EXCEPTION_STMT, static_cast<parser::v2::DropExceptionStmt*>(stmt)->exceptions.front(), 24);
        case parser::v2::ASTKind::DropPolicyStmt:
            return makeDrop(Opcode::SBLR3_DROP_POLICY, static_cast<parser::v2::DropPolicyStmt*>(stmt)->table_path, 0);
        case parser::v2::ASTKind::DropForeignServerStmt:
            {
                auto* s = static_cast<parser::v2::DropForeignServerStmt*>(stmt);
                parser::v2::SchemaPath path;
                path.components.push_back(s->server_name);
                return makeDrop(Opcode::SBLR3_DROP_FOREIGN_SERVER, path, 31);
            }
        case parser::v2::ASTKind::DropForeignTableStmt:
            return makeDrop(Opcode::SBLR3_DROP_FOREIGN_TABLE, static_cast<parser::v2::DropForeignTableStmt*>(stmt)->tables.front(), 32);
        case parser::v2::ASTKind::DropUserMappingStmt:
            {
                auto* s = static_cast<parser::v2::DropUserMappingStmt*>(stmt);
                parser::v2::SchemaPath path;
                if (s->server_name != parser::v2::StringPool::INVALID_ID) {
                    path.components.push_back(s->server_name);
                }
                if (s->user_name != parser::v2::StringPool::INVALID_ID) {
                    path.components.push_back(s->user_name);
                }
                return makeDrop(Opcode::SBLR3_DROP_USER_MAPPING, path, 33);
            }
        case parser::v2::ASTKind::DropSynonymStmt:
            return makeDrop(Opcode::SBLR3_DROP_SYNONYM, static_cast<parser::v2::DropSynonymStmt*>(stmt)->synonyms.front(), 38);
        case parser::v2::ASTKind::DropUdrStmt:
            return makeDrop(Opcode::SBLR3_DROP_UDR, static_cast<parser::v2::DropUdrStmt*>(stmt)->udrs.front(), 23);
        case parser::v2::ASTKind::DropJobStmt:
            {
                auto* s = static_cast<parser::v2::DropJobStmt*>(stmt);
                parser::v2::SchemaPath path;
                path.components.push_back(s->job_name);
                return makeDrop(Opcode::SBLR3_DROP_JOB, path, 0);
            }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.flags = 0;
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlTruncate(parser::v2::TruncateTableStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_TRUNCATE_TABLE);
    inst.flags = 0;
    Value::Object payload;
    payload["flags"] = Value(uint64_t(0));
    Value::List tables;
    for (const auto& path : stmt->tables) tables.push_back(toSchemaPath(path));
    payload["tables"] = Value(std::move(tables));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitComment(parser::v2::CommentStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_COMMENT);
    inst.flags = 0;
    Value::Object payload;
    payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(stmt->object_type)));
    payload["object_path"] = toSchemaPath(stmt->object_path);
    payload["text"] = Value(stmt->is_null ? std::string() : std::string(pool_.get(stmt->comment_text)));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitGrant(parser::v2::GrantStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_GRANT);
    inst.flags = 0;
    Value::Object payload;
    payload["is_grant"] = Value(true);
    uint64_t privs = 0;
    for (auto p : stmt->privileges) {
        privs |= (1ull << static_cast<uint8_t>(p));
    }
    payload["privileges"] = Value(privs);
    payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(stmt->object_type)));
    if (!stmt->objects.empty()) {
        payload["object_path"] = toSchemaPath(stmt->objects.front());
    }
    Value::List grantees;
    if (stmt->is_public) {
        grantees.push_back(Value(std::string("PUBLIC")));
    } else {
        for (auto id : stmt->grantees) grantees.push_back(toIdent(id));
    }
    payload["grantees"] = Value(std::move(grantees));
    payload["with_grant_option"] = Value(stmt->with_grant_option);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitRevoke(parser::v2::RevokeStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_REVOKE);
    inst.flags = 0;
    Value::Object payload;
    payload["is_grant"] = Value(false);
    uint64_t privs = 0;
    for (auto p : stmt->privileges) {
        privs |= (1ull << static_cast<uint8_t>(p));
    }
    payload["privileges"] = Value(privs);
    payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(stmt->object_type)));
    if (!stmt->objects.empty()) {
        payload["object_path"] = toSchemaPath(stmt->objects.front());
    }
    Value::List grantees;
    if (stmt->is_public) {
        grantees.push_back(Value(std::string("PUBLIC")));
    } else {
        for (auto id : stmt->grantees) grantees.push_back(toIdent(id));
    }
    payload["grantees"] = Value(std::move(grantees));
    payload["with_grant_option"] = Value(stmt->grant_option_for);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitTxn(parser::v2::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;
    Value::Object payload;

    switch (stmt->kind()) {
        case parser::v2::ASTKind::StartTransactionStmt:
            inst.opcode = op(Opcode::SBLR3_START_TRANSACTION);
            payload["action"] = Value(uint64_t(1));
            inst.payload = Value(std::move(payload));
            return inst;
        case parser::v2::ASTKind::PrepareTransactionStmt:
            inst.opcode = op(Opcode::SBLR3_PREPARE_TRANSACTION);
            break;
        case parser::v2::ASTKind::CommitStmt: {
            auto* s = static_cast<parser::v2::CommitStmt*>(stmt);
            if (s->is_prepared) {
                inst.opcode = op(Opcode::SBLR3_COMMIT_PREPARED);
            } else if (s->retaining) {
                inst.opcode = op(Opcode::SBLR3_COMMIT_RETAINING);
            } else {
                inst.opcode = op(Opcode::SBLR3_COMMIT);
            }
            payload["action"] = Value(uint64_t(2));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::RollbackStmt: {
            auto* s = static_cast<parser::v2::RollbackStmt*>(stmt);
            if (s->is_prepared) {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK_PREPARED);
            } else if (s->retaining) {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK_RETAINING);
            } else if (s->to_savepoint) {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK_TO_SAVEPOINT);
                payload["action"] = Value(uint64_t(6));
                payload["name"] = toIdent(s->savepoint_name);
                inst.payload = Value(std::move(payload));
                return inst;
            } else {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK);
            }
            payload["action"] = Value(uint64_t(3));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::SavepointStmt:
            inst.opcode = op(Opcode::SBLR3_SAVEPOINT);
            payload["action"] = Value(uint64_t(4));
            payload["name"] = toIdent(static_cast<parser::v2::SavepointStmt*>(stmt)->name);
            inst.payload = Value(std::move(payload));
            return inst;
        case parser::v2::ASTKind::ReleaseSavepointStmt:
            inst.opcode = op(Opcode::SBLR3_RELEASE_SAVEPOINT);
            payload["action"] = Value(uint64_t(5));
            payload["name"] = toIdent(static_cast<parser::v2::ReleaseSavepointStmt*>(stmt)->name);
            inst.payload = Value(std::move(payload));
            return inst;
        default:
            break;
    }

    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitSetShowReset(parser::v2::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;
    Value::Object payload;
    auto makeStringLiteral = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto makeIntLiteral = [&](int64_t value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_INT64);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto makeBoolLiteral = [&](bool value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto setValueInstr = [&](Instruction value) {
        payload["value"] = Value(makeInstr(std::move(value)));
    };
    auto setValueStringId = [&](parser::v2::StringPool::StringId id) {
        if (id != parser::v2::StringPool::INVALID_ID) {
            setValueInstr(makeStringLiteral(std::string(pool_.get(id))));
        }
    };
    auto normalizeKey = [&](parser::v2::StringPool::StringId id) -> Value {
        if (id == parser::v2::StringPool::INVALID_ID) return Value(std::string());
        std::string key(pool_.get(id));
        if (key == "TIME_ZONE") key = "TIME ZONE";
        if (key == "SESSION_AUTHORIZATION") key = "SESSION AUTHORIZATION";
        return Value(std::move(key));
    };

    if (stmt->kind() == parser::v2::ASTKind::AnalyzeStmt) {
        inst.opcode = op(Opcode::SBLR3_ANALYZE);
        auto* s = static_cast<parser::v2::AnalyzeStmt*>(stmt);
        payload["table_path"] = toSchemaPath(s->table_path);
        if (s->has_column) payload["column"] = toIdent(s->column_name);
        if (s->has_sample) payload["sample_rate"] = Value(s->sample_rate);
        payload["verbose"] = Value(s->verbose);
        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v2::ASTKind::ExplainStmt) {
        inst.opcode = op(Opcode::SBLR3_EXPLAIN_PLAN);
        auto* s = static_cast<parser::v2::ExplainStmt*>(stmt);
        payload["analyze"] = Value(s->analyze);
        if (s->format != parser::v2::StringPool::INVALID_ID) {
            payload["format"] = toIdent(s->format);
        }
        if (s->query) payload["query"] = Value(makeInstr(emitStatement(s->query)));
        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v2::ASTKind::SetStmt) {
        auto* s = static_cast<parser::v2::SetStmt*>(stmt);
        payload["action"] = Value(uint64_t(1));
        payload["scope"] = Value(uint64_t(s->scope == parser::v2::SetStmt::Scope::LOCAL ? 1 : 0));

        switch (s->set_type) {
            case parser::v2::SetStmt::SetType::TIME_ZONE:
                inst.opcode = op(Opcode::SBLR3_SET_TIME_ZONE);
                payload["key"] = Value(std::string("TIME ZONE"));
                if (!s->is_default && s->value) setValueInstr(emitExpression(s->value));
                break;
            case parser::v2::SetStmt::SetType::AUTOCOMMIT:
                inst.opcode = op(Opcode::SBLR3_SET_AUTOCOMMIT);
                payload["key"] = Value(std::string("AUTOCOMMIT"));
                if (s->has_autocommit) {
                    bool on = (s->autocommit_mode == parser::v2::AutocommitMode::ON);
                    setValueInstr(makeBoolLiteral(on));
                }
                break;
            case parser::v2::SetStmt::SetType::TRANSACTION:
                inst.opcode = op(Opcode::SBLR3_SET_TRANSACTION);
                payload["key"] = Value(std::string("TRANSACTION"));
                break;
            case parser::v2::SetStmt::SetType::SQL_DIALECT:
                inst.opcode = op(Opcode::SBLR3_SET_SQL_DIALECT);
                payload["key"] = Value(std::string("SQL DIALECT"));
                if (s->sql_dialect != 0) setValueInstr(makeIntLiteral(s->sql_dialect));
                break;
            case parser::v2::SetStmt::SetType::NAMES:
                inst.opcode = op(Opcode::SBLR3_SET_NAMES);
                payload["key"] = Value(std::string("NAMES"));
                setValueStringId(s->name);
                break;
            case parser::v2::SetStmt::SetType::LOCAL_TIMEOUT:
                inst.opcode = op(Opcode::SBLR3_SET_LOCAL_TIMEOUT);
                payload["key"] = Value(std::string("LOCAL_TIMEOUT"));
                setValueInstr(makeIntLiteral(static_cast<int64_t>(s->local_timeout_seconds)));
                break;
            case parser::v2::SetStmt::SetType::SESSION_AUTHORIZATION:
                inst.opcode = op(Opcode::SBLR3_SET_SESSION_AUTH);
                payload["key"] = Value(std::string("SESSION AUTHORIZATION"));
                if (!s->is_default) setValueStringId(s->name);
                break;
            case parser::v2::SetStmt::SetType::ROLE:
                inst.opcode = op(Opcode::SBLR3_SET_ROLE);
                payload["key"] = Value(std::string("ROLE"));
                if (!s->is_default) setValueStringId(s->name);
                break;
            case parser::v2::SetStmt::SetType::VARIABLE:
            case parser::v2::SetStmt::SetType::TERM:
            case parser::v2::SetStmt::SetType::STATISTICS_INDEX:
            case parser::v2::SetStmt::SetType::GENERATOR:
            default:
                inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
                payload["key"] = normalizeKey(s->name);
                if (!s->is_default && s->value) {
                    setValueInstr(emitExpression(s->value));
                }
                break;
        }

        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v2::ASTKind::ResetStmt) {
        auto* s = static_cast<parser::v2::ResetStmt*>(stmt);
        payload["action"] = Value(uint64_t(3));
        payload["scope"] = Value(uint64_t(0));

        if (s->reset_all) {
            inst.opcode = op(Opcode::SBLR3_RESET_ALL);
            payload["key"] = Value(std::string("ALL"));
        } else {
            std::string key = (s->name == parser::v2::StringPool::INVALID_ID)
                                  ? std::string()
                                  : std::string(pool_.get(s->name));
            if (key == "ROLE") {
                inst.opcode = op(Opcode::SBLR3_RESET_ROLE);
                payload["key"] = Value(std::string("ROLE"));
            } else if (key == "SESSION_AUTHORIZATION") {
                inst.opcode = op(Opcode::SBLR3_RESET_SESSION_AUTH);
                payload["key"] = Value(std::string("SESSION AUTHORIZATION"));
            } else if (key == "TIME_ZONE") {
                inst.opcode = op(Opcode::SBLR3_RESET_TIME_ZONE);
                payload["key"] = Value(std::string("TIME ZONE"));
            } else {
                inst.opcode = op(Opcode::SBLR3_RESET);
                payload["key"] = normalizeKey(s->name);
            }
        }
        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v2::ASTKind::ShowStmt) {
        auto* s = static_cast<parser::v2::ShowStmt*>(stmt);
        payload["action"] = Value(uint64_t(2));
        payload["scope"] = Value(uint64_t(0));

        auto setKeyFromName = [&]() { payload["key"] = normalizeKey(s->name); };
        auto setKeyFromFrom = [&]() { payload["key"] = normalizeKey(s->from_name); };
        auto setValueFromLike = [&]() { setValueStringId(s->like_pattern); };

        switch (s->show_type) {
            case parser::v2::ShowStmt::ShowType::ALL:
                inst.opcode = op(Opcode::SBLR3_SHOW_ALL);
                payload["key"] = Value(std::string("ALL"));
                break;
            case parser::v2::ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL:
                inst.opcode = op(Opcode::SBLR3_SHOW_TRANSACTION_LEVEL);
                payload["key"] = Value(std::string("TRANSACTION ISOLATION LEVEL"));
                break;
            case parser::v2::ShowStmt::ShowType::TABLES:
                inst.opcode = op(Opcode::SBLR3_SHOW_TABLES);
                setKeyFromFrom();
                setValueFromLike();
                break;
            case parser::v2::ShowStmt::ShowType::DATABASES:
                inst.opcode = op(Opcode::SBLR3_SHOW_DATABASES);
                payload["key"] = Value(std::string());
                setValueFromLike();
                break;
            case parser::v2::ShowStmt::ShowType::COLUMNS:
                inst.opcode = op(Opcode::SBLR3_SHOW_COLUMNS);
                setKeyFromFrom();
                setValueFromLike();
                break;
            case parser::v2::ShowStmt::ShowType::INDEXES:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEXES);
                setKeyFromFrom();
                break;
            case parser::v2::ShowStmt::ShowType::CREATE_TABLE:
                inst.opcode = op(Opcode::SBLR3_SHOW_CREATE_TABLE);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::TABLE:
                inst.opcode = op(Opcode::SBLR3_SHOW_TABLE);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::INDEX:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::TRIGGER:
                inst.opcode = op(Opcode::SBLR3_SHOW_TRIGGER);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::VIEW:
                inst.opcode = op(Opcode::SBLR3_SHOW_VIEW);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::PROCEDURE:
                inst.opcode = op(Opcode::SBLR3_SHOW_PROCEDURE);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::FUNCTION:
                inst.opcode = op(Opcode::SBLR3_SHOW_FUNCTION);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::DOMAIN:
                inst.opcode = op(Opcode::SBLR3_SHOW_DOMAIN);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::GENERATOR:
                inst.opcode = op(Opcode::SBLR3_SHOW_GENERATOR);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::SCHEMA:
                inst.opcode = op(Opcode::SBLR3_SHOW_SCHEMA);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::ROLE:
                inst.opcode = op(Opcode::SBLR3_SHOW_ROLE);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::GRANTS:
                inst.opcode = op(Opcode::SBLR3_SHOW_GRANTS);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::JOBS:
                inst.opcode = op(Opcode::SBLR3_SHOW_JOBS);
                payload["key"] = Value(std::string());
                setValueFromLike();
                break;
            case parser::v2::ShowStmt::ShowType::JOB:
                inst.opcode = op(Opcode::SBLR3_SHOW_JOB);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::JOB_RUNS:
                inst.opcode = op(Opcode::SBLR3_SHOW_JOB_RUNS);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::CHECKS:
                inst.opcode = op(Opcode::SBLR3_SHOW_CHECKS);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::COLLATIONS:
                inst.opcode = op(Opcode::SBLR3_SHOW_COLLATIONS);
                payload["key"] = Value(std::string());
                setValueFromLike();
                break;
            case parser::v2::ShowStmt::ShowType::COMMENTS:
                inst.opcode = op(Opcode::SBLR3_SHOW_COMMENTS);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::DEPENDENCIES:
                inst.opcode = op(Opcode::SBLR3_SHOW_DEPENDENCIES);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::PACKAGE:
                inst.opcode = op(Opcode::SBLR3_SHOW_PACKAGE);
                setKeyFromName();
                break;
            case parser::v2::ShowStmt::ShowType::SQL_DIALECT:
                inst.opcode = op(Opcode::SBLR3_SHOW_SQL_DIALECT);
                payload["key"] = Value(std::string("SQL DIALECT"));
                break;
            case parser::v2::ShowStmt::ShowType::VERSION:
                inst.opcode = op(Opcode::SBLR3_SHOW_VERSION);
                payload["key"] = Value(std::string());
                break;
            case parser::v2::ShowStmt::ShowType::DATABASE:
                inst.opcode = op(Opcode::SBLR3_SHOW_DATABASE);
                payload["key"] = Value(std::string());
                break;
            case parser::v2::ShowStmt::ShowType::SYSTEM:
                inst.opcode = op(Opcode::SBLR3_SHOW_SYSTEM);
                payload["key"] = Value(std::string());
                break;
            case parser::v2::ShowStmt::ShowType::METRICS:
                inst.opcode = op(Opcode::SBLR3_SHOW_METRICS);
                payload["key"] = Value(std::string());
                break;
            case parser::v2::ShowStmt::ShowType::VARIABLE:
            default:
                inst.opcode = op(Opcode::SBLR3_SHOW_VARIABLE);
                setKeyFromName();
                break;
        }

        inst.payload = Value(std::move(payload));
        return inst;
    }

    inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
    payload["action"] = Value(uint64_t(1));
    payload["key"] = Value(std::string(""));
    payload["scope"] = Value(uint64_t(0));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitUtility(parser::v2::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;

    switch (stmt->kind()) {
        case parser::v2::ASTKind::ConnectStmt:
            inst.opcode = op(Opcode::SBLR3_CONNECT);
            {
                auto* s = static_cast<parser::v2::ConnectStmt*>(stmt);
                Value::Object payload;
                payload["database"] = toIdent(s->database);
                if (s->user != parser::v2::StringPool::INVALID_ID) payload["user"] = toIdent(s->user);
                if (s->password != parser::v2::StringPool::INVALID_ID) payload["password"] = toIdent(s->password);
                if (s->role != parser::v2::StringPool::INVALID_ID) payload["role"] = toIdent(s->role);
                if (s->charset != parser::v2::StringPool::INVALID_ID) payload["charset"] = toIdent(s->charset);
                inst.payload = Value(std::move(payload));
            }
            return inst;
        case parser::v2::ASTKind::DisconnectStmt:
            inst.opcode = op(Opcode::SBLR3_DISCONNECT);
            {
                auto* s = static_cast<parser::v2::DisconnectStmt*>(stmt);
                Value::Object payload;
                payload["target"] = Value(uint64_t(static_cast<uint8_t>(s->target)));
                if (s->connection_name != parser::v2::StringPool::INVALID_ID) {
                    payload["connection_name"] = toIdent(s->connection_name);
                }
                inst.payload = Value(std::move(payload));
            }
            return inst;
        case parser::v2::ASTKind::SweepDatabaseStmt:
            inst.opcode = op(Opcode::SBLR3_SWEEP);
            inst.payload = Value(Value::Object{});
            return inst;
        case parser::v2::ASTKind::ExecuteJobStmt: {
            auto* s = static_cast<parser::v2::ExecuteJobStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_EXECUTE_JOB);
            Value::Object payload;
            payload["job_name"] = toIdent(s->job_name);
            payload["params"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CancelJobRunStmt: {
            auto* s = static_cast<parser::v2::CancelJobRunStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CANCEL_JOB_RUN);
            uint64_t run_id = 0;
            if (s->job_run_uuid != parser::v2::StringPool::INVALID_ID) {
                std::string v(pool_.get(s->job_run_uuid));
                try {
                    run_id = std::stoull(v);
                } catch (...) {
                    run_id = 0;
                }
            }
            Value::Object payload;
            payload["run_id"] = Value(run_id);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }

    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitPsql(parser::v2::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;

    switch (stmt->kind()) {
        case parser::v2::ASTKind::ExecuteBlockStmt: {
            auto* s = static_cast<parser::v2::ExecuteBlockStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_BLOCK);
            Value::Object payload;
            Value::List decls;
            for (const auto& decl : s->input_params) {
                Value::Object d;
                d["name"] = toIdent(decl.name);
                d["type"] = Value(buildTypeSpec(decl.type));
                d["constant"] = Value(false);
                if (decl.default_value) d["default"] = Value(makeInstr(emitExpression(decl.default_value)));
                decls.push_back(Value(std::move(d)));
            }
            for (const auto& decl : s->output_params) {
                Value::Object d;
                d["name"] = toIdent(decl.name);
                d["type"] = Value(buildTypeSpec(decl.type));
                d["constant"] = Value(false);
                decls.push_back(Value(std::move(d)));
            }
            for (const auto& decl : s->variables) {
                Value::Object d;
                d["name"] = toIdent(decl.name);
                d["type"] = Value(buildTypeSpec(decl.type));
                d["constant"] = Value(false);
                if (decl.default_value) d["default"] = Value(makeInstr(emitExpression(decl.default_value)));
                decls.push_back(Value(std::move(d)));
            }
            payload["decls"] = Value(std::move(decls));
            payload["body"] = s->body ? toStmtList({s->body}) : Value(Value::List{});
            payload["exception_handlers"] = Value(Value::List{});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CompoundStmt: {
            auto* s = static_cast<parser::v2::CompoundStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_BLOCK);
            Value::Object payload;
            payload["decls"] = Value(Value::List{});
            payload["body"] = toStmtList(s->statements);
            Value::List handlers;
            for (auto* h : s->exception_handlers) {
                if (!h || h->kind() != parser::v2::ASTKind::WhenExceptionStmt) continue;
                auto* wh = static_cast<parser::v2::WhenExceptionStmt*>(h);
                Value::Object ex;
                std::string cond = "ANY";
                if (wh->type == parser::v2::WhenExceptionStmt::ExceptionType::SQLCODE) {
                    cond = "SQLCODE " + std::to_string(wh->sqlcode);
                } else if (wh->type == parser::v2::WhenExceptionStmt::ExceptionType::GDSCODE) {
                    cond = std::string(pool_.get(wh->gdscode));
                } else if (wh->type == parser::v2::WhenExceptionStmt::ExceptionType::EXCEPTION) {
                    cond = std::string(pool_.get(wh->exception_name));
                }
                ex["condition"] = Value(cond);
                if (wh->handler) {
                    ex["handler"] = toStmtList({wh->handler});
                }
                handlers.push_back(Value(std::move(ex)));
            }
            payload["exception_handlers"] = Value(std::move(handlers));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::DeclareVariableStmt: {
            auto* s = static_cast<parser::v2::DeclareVariableStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_DECLARE);
            Value::Object decl;
            decl["name"] = toIdent(s->name);
            decl["type"] = Value(buildTypeSpec(s->type));
            decl["constant"] = Value(false);
            if (s->default_value) decl["default"] = Value(makeInstr(emitExpression(s->default_value)));
            Value::Object payload;
            payload["decls"] = Value(Value::List{Value(std::move(decl))});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AssignmentStmt: {
            auto* s = static_cast<parser::v2::AssignmentStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_ASSIGN);
            Value::Object payload;
            payload["target"] = emitVarRefValue(s->variable);
            payload["value"] = Value(makeInstr(emitExpression(s->value)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::IfStmt: {
            auto* s = static_cast<parser::v2::IfStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_IF);
            Value::Object payload;
            payload["condition"] = Value(makeInstr(emitExpression(s->condition)));
            payload["then_body"] = toStmtList({s->then_branch});
            Value::List elsif;
            if (s->else_branch && s->else_branch->kind() == parser::v2::ASTKind::IfStmt) {
                auto* else_if = static_cast<parser::v2::IfStmt*>(s->else_branch);
                Value::Object e;
                e["condition"] = Value(makeInstr(emitExpression(else_if->condition)));
                e["body"] = toStmtList({else_if->then_branch});
                elsif.push_back(Value(std::move(e)));
                if (else_if->else_branch) {
                    payload["else_body"] = toStmtList({else_if->else_branch});
                }
            } else if (s->else_branch) {
                payload["else_body"] = toStmtList({s->else_branch});
            }
            payload["elsif"] = Value(std::move(elsif));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::WhileStmt: {
            auto* s = static_cast<parser::v2::WhileStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_WHILE);
            Value::Object payload;
            payload["condition"] = Value(makeInstr(emitExpression(s->condition)));
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ForSelectStmt: {
            auto* s = static_cast<parser::v2::ForSelectStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_FOR_SELECT);
            Value::Object payload;
            if (!s->into_variables.empty()) {
                payload["record"] = emitVarRefValue(s->into_variables.front());
            } else {
                payload["record"] = emitVarRefValue(parser::v2::StringPool::INVALID_ID);
            }
            if (s->select_stmt) {
                payload["query"] = Value(makeInstr(emitStatement(s->select_stmt)));
            }
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ForExecuteStmt: {
            auto* s = static_cast<parser::v2::ForExecuteStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_FOR_EXECUTE);
            Value::Object payload;
            if (!s->into_variables.empty()) {
                payload["record"] = emitVarRefValue(s->into_variables.front());
            } else {
                payload["record"] = emitVarRefValue(parser::v2::StringPool::INVALID_ID);
            }
            if (s->sql) payload["sql"] = Value(makeInstr(emitExpression(s->sql)));
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LoopStmt: {
            auto* s = static_cast<parser::v2::LoopStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_LOOP);
            Value::Object payload;
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ExitStmt: {
            inst.opcode = op(Opcode::SBLR3_EXIT);
            inst.payload = Value(Value::Object{});
            return inst;
        }
        case parser::v2::ASTKind::LeaveStmt: {
            auto* s = static_cast<parser::v2::LeaveStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_LEAVE);
            Value::Object payload;
            if (s->label != parser::v2::StringPool::INVALID_ID) payload["label"] = toIdent(s->label);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ContinueStmt: {
            auto* s = static_cast<parser::v2::ContinueStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_CONTINUE);
            Value::Object payload;
            if (s->label != parser::v2::StringPool::INVALID_ID) payload["label"] = toIdent(s->label);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::SuspendStmt:
            inst.opcode = op(Opcode::SBLR3_SUSPEND);
            inst.payload = Value(Value::Object{});
            return inst;
        case parser::v2::ASTKind::ReturnStmt: {
            auto* s = static_cast<parser::v2::ReturnStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_RETURN);
            Value::Object payload;
            if (s->value) payload["value"] = Value(makeInstr(emitExpression(s->value)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ExceptionRaiseStmt: {
            auto* s = static_cast<parser::v2::ExceptionRaiseStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_RAISE);
            Value::Object payload;
            if (s->exception_name != parser::v2::StringPool::INVALID_ID) {
                payload["message"] = Value(std::string(pool_.get(s->exception_name)));
            }
            if (s->message) payload["params"] = Value(Value::List{Value(makeInstr(emitExpression(s->message)))});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::PostEventStmt: {
            auto* s = static_cast<parser::v2::PostEventStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_POST_EVENT);
            Value::Object payload;
            if (s->event_name && s->event_name->kind() == parser::v2::ASTKind::LiteralExpr) {
                auto* lit = static_cast<parser::v2::LiteralExpr*>(s->event_name);
                if (lit->literal_type == parser::v2::LiteralType::STRING) {
                    payload["event_name"] = Value(std::string(pool_.get(lit->string_value)));
                } else {
                    fail("POST_EVENT requires a string literal event name");
                }
            } else if (s->event_name) {
                fail("POST_EVENT requires a string literal event name");
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ExecuteProcedureStmt: {
            auto* s = static_cast<parser::v2::ExecuteProcedureStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CALL);
            Value::Object payload;
            payload["proc_name"] = toIdent(s->procedure_path.objectName());
            payload["args"] = toExprList(s->arguments);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::ExecuteStatementStmt: {
            auto* s = static_cast<parser::v2::ExecuteStatementStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_FOR_EXECUTE);
            Value::Object payload;
            if (!s->into_variables.empty()) {
                payload["record"] = emitVarRefValue(s->into_variables.front());
            } else {
                payload["record"] = emitVarRefValue(parser::v2::StringPool::INVALID_ID);
            }
            if (s->sql) payload["sql"] = Value(makeInstr(emitExpression(s->sql)));
            payload["body"] = Value(Value::List{});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::DeclareCursorStmt: {
            auto* s = static_cast<parser::v2::DeclareCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_DECLARE);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            payload["scroll"] = Value(s->scroll);
            if (s->select_stmt) payload["query"] = Value(makeInstr(emitStatement(s->select_stmt)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::OpenCursorStmt: {
            auto* s = static_cast<parser::v2::OpenCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_OPEN);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::FetchCursorStmt: {
            auto* s = static_cast<parser::v2::FetchCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_FETCH);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            payload["direction"] = Value(uint64_t(static_cast<uint8_t>(s->direction)));
            if (s->offset) payload["offset"] = Value(makeInstr(emitExpression(s->offset)));
            if (!s->into_variables.empty()) {
                payload["target"] = emitVarRefValue(s->into_variables.front());
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CloseCursorStmt: {
            auto* s = static_cast<parser::v2::CloseCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_CLOSE);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitExpression(parser::v2::Expression* expr) {
    if (!expr) {
        Instruction inst;
        inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
        inst.flags = 0;
        inst.payload = Value(Value::Object{{"value", Value() }});
        return inst;
    }
    auto makeStringLiteral = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto selectorToInstr = [&](const parser::v2::ElementSelector& selector) {
        switch (selector.kind) {
            case parser::v2::ElementSelector::Kind::IDENTIFIER:
                return makeStringLiteral(std::string(pool_.get(selector.identifier)));
            case parser::v2::ElementSelector::Kind::STRING_LITERAL:
                return makeStringLiteral(std::string(pool_.get(selector.string_literal)));
            case parser::v2::ElementSelector::Kind::INTEGER_EXPR:
                return emitExpression(selector.expr);
        }
        return makeStringLiteral(std::string());
    };
    auto encodeExprBytes = [&](parser::v2::Expression* value) -> Value::Bytes {
        if (!value) {
            return encodeInstructionBytes(emitLiteral(nullptr));
        }
        return encodeInstructionBytes(emitExpression(value));
    };
    auto appendU128 = [](const parser::v2::U128& v, Value::Bytes& out) {
        out.insert(out.end(), v.begin(), v.end());
    };
    switch (expr->kind()) {
        case parser::v2::ASTKind::LiteralExpr:
            return emitLiteral(static_cast<parser::v2::LiteralExpr*>(expr));
        case parser::v2::ASTKind::LiteralEnumExpr: {
            auto* s = static_cast<parser::v2::LiteralEnumExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_ENUM);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->enum_catalog_id, bytes);
            uint8_t flags = 0;
            if (s->has_label) flags |= 0x01;
            if (s->has_ordinal) flags |= 0x02;
            bytes.push_back(flags);
            if (s->has_label && s->label != parser::v2::StringPool::INVALID_ID) {
                appendStringWithLen(std::string(pool_.get(s->label)), bytes);
            } else {
                appendVaruint(0, bytes);
            }
            if (s->has_ordinal) {
                appendLE32(static_cast<uint32_t>(s->ordinal), bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralSetExpr: {
            auto* s = static_cast<parser::v2::LiteralSetExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_SET);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->set_catalog_id, bytes);
            appendVaruint(s->elements.size(), bytes);
            for (auto* elem : s->elements) {
                Value::Bytes ebytes;
                if (elem) {
                    appendU128(elem->enum_catalog_id, ebytes);
                    uint8_t flags = 0;
                    if (elem->has_label) flags |= 0x01;
                    if (elem->has_ordinal) flags |= 0x02;
                    ebytes.push_back(flags);
                    if (elem->has_label && elem->label != parser::v2::StringPool::INVALID_ID) {
                        appendStringWithLen(std::string(pool_.get(elem->label)), ebytes);
                    } else {
                        appendVaruint(0, ebytes);
                    }
                    if (elem->has_ordinal) {
                        appendLE32(static_cast<uint32_t>(elem->ordinal), ebytes);
                    }
                }
                appendBytesWithLen(ebytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralRowExpr: {
            auto* s = static_cast<parser::v2::LiteralRowExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_ROW);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->row_catalog_id, bytes);
            appendVaruint(s->fields.size(), bytes);
            for (const auto& field : s->fields) {
                if (field.name != parser::v2::StringPool::INVALID_ID) {
                    appendStringWithLen(std::string(pool_.get(field.name)), bytes);
                } else {
                    appendVaruint(0, bytes);
                }
                Value::Bytes vbytes = encodeExprBytes(field.value);
                appendBytesWithLen(vbytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralCompositeExpr: {
            auto* s = static_cast<parser::v2::LiteralCompositeExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_COMPOSITE);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->composite_catalog_id, bytes);
            appendVaruint(s->fields.size(), bytes);
            for (const auto& field : s->fields) {
                if (field.name != parser::v2::StringPool::INVALID_ID) {
                    appendStringWithLen(std::string(pool_.get(field.name)), bytes);
                } else {
                    appendVaruint(0, bytes);
                }
                Value::Bytes vbytes = encodeExprBytes(field.value);
                appendBytesWithLen(vbytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralDomainExpr: {
            auto* s = static_cast<parser::v2::LiteralDomainExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_DOMAIN);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->domain_id, bytes);
            Value::Bytes vbytes = encodeExprBytes(s->value);
            appendBytesWithLen(vbytes, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralBitExpr: {
            auto* s = static_cast<parser::v2::LiteralBitExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_BIT);
            inst.flags = 0;
            Value::Bytes bytes;
            appendLE16(s->bit_length, bytes);
            bytes.insert(bytes.end(), s->bytes.begin(), s->bytes.end());
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralYearExpr: {
            auto* s = static_cast<parser::v2::LiteralYearExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_YEAR);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralDateTimeExpr: {
            auto* s = static_cast<parser::v2::LiteralDateTimeExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_DATETIME);
            inst.flags = 0;
            Value::Bytes bytes;
            appendLE64(static_cast<uint64_t>(s->epoch_usec), bytes);
            bytes.push_back(s->with_timezone ? 1 : 0);
            bytes.push_back(s->precision);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralMediumIntExpr: {
            auto* s = static_cast<parser::v2::LiteralMediumIntExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_MEDIUMINT);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralGeometryExpr: {
            auto* s = static_cast<parser::v2::LiteralGeometryExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_GEOMETRY);
            inst.flags = 0;
            Value::Bytes bytes;
            bytes.push_back(s->format);
            appendLE32(s->srid, bytes);
            bytes.insert(bytes.end(), s->bytes.begin(), s->bytes.end());
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralJsonPathExpr: {
            auto* s = static_cast<parser::v2::LiteralJsonPathExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_JSONPATH);
            inst.flags = 0;
            Value::Bytes bytes;
            bytes.push_back(s->dialect);
            if (s->text != parser::v2::StringPool::INVALID_ID) {
                appendStringWithLen(std::string(pool_.get(s->text)), bytes);
            } else {
                appendVaruint(0, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralInt8Expr: {
            auto* s = static_cast<parser::v2::LiteralInt8Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT8);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralInt16Expr: {
            auto* s = static_cast<parser::v2::LiteralInt16Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT16);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralUInt8Expr: {
            auto* s = static_cast<parser::v2::LiteralUInt8Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT8);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralUInt16Expr: {
            auto* s = static_cast<parser::v2::LiteralUInt16Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT16);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralUInt32Expr: {
            auto* s = static_cast<parser::v2::LiteralUInt32Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT32);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralUInt64Expr: {
            auto* s = static_cast<parser::v2::LiteralUInt64Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT64);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralUInt128Expr: {
            auto* s = static_cast<parser::v2::LiteralUInt128Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT128);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->value, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralInt128Expr: {
            auto* s = static_cast<parser::v2::LiteralInt128Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT128);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->value, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralFloat32Expr: {
            auto* s = static_cast<parser::v2::LiteralFloat32Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_FLOAT32);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(static_cast<double>(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralTimeTzExpr: {
            auto* s = static_cast<parser::v2::LiteralTimeTzExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TIME_TZ);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->time_usec));
            payload["offset_seconds"] = Value(int64_t(s->tz_offset_minutes) * 60);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralTimestampTzExpr: {
            auto* s = static_cast<parser::v2::LiteralTimestampTzExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TIMESTAMP_TZ);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->epoch_usec));
            payload["offset_seconds"] = Value(int64_t(s->tz_offset_minutes) * 60);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::LiteralRangeExpr: {
            auto* s = static_cast<parser::v2::LiteralRangeExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_RANGE);
            inst.flags = 0;
            Value::Bytes bytes;
            TypeSpec spec = buildTypeSpec(s->range_base_type);
            appendLE16(spec.type_opcode, bytes);
            appendLE32(static_cast<uint32_t>(spec.type_payload.size()), bytes);
            bytes.insert(bytes.end(), spec.type_payload.begin(), spec.type_payload.end());
            bytes.push_back(s->flags);
            bytes.push_back(s->lower_present ? 1 : 0);
            bytes.push_back(s->upper_present ? 1 : 0);
            if (s->lower_present) {
                Value::Bytes lower_bytes = encodeExprBytes(s->lower);
                appendBytesWithLen(lower_bytes, bytes);
            }
            if (s->upper_present) {
                Value::Bytes upper_bytes = encodeExprBytes(s->upper);
                appendBytesWithLen(upper_bytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralArrayExpr: {
            auto* s = static_cast<parser::v2::LiteralArrayExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_ARRAY);
            inst.flags = 0;
            Value::Bytes bytes;
            TypeSpec spec = buildTypeSpec(s->element_type);
            appendLE16(spec.type_opcode, bytes);
            appendLE32(static_cast<uint32_t>(spec.type_payload.size()), bytes);
            bytes.insert(bytes.end(), spec.type_payload.begin(), spec.type_payload.end());
            bytes.push_back(s->dimensions);
            appendVaruint(s->dim_lengths.size(), bytes);
            for (auto len : s->dim_lengths) {
                appendLE32(len, bytes);
            }
            appendVaruint(s->elements.size(), bytes);
            for (auto* elem : s->elements) {
                Value::Bytes ebytes = encodeExprBytes(elem);
                appendBytesWithLen(ebytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralVariantExpr: {
            auto* s = static_cast<parser::v2::LiteralVariantExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_VARIANT);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->variant_type_id, bytes);
            if (s->tag_name != parser::v2::StringPool::INVALID_ID) {
                appendStringWithLen(std::string(pool_.get(s->tag_name)), bytes);
            } else {
                appendVaruint(0, bytes);
            }
            Value::Bytes vbytes = encodeExprBytes(s->value);
            appendBytesWithLen(vbytes, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralTsVectorExpr: {
            auto* s = static_cast<parser::v2::LiteralTsVectorExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TSVECTOR);
            inst.flags = 0;
            Value::Bytes bytes;
            if (s->text != parser::v2::StringPool::INVALID_ID) {
                auto text = std::string(pool_.get(s->text));
                bytes.assign(text.begin(), text.end());
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralTsQueryExpr: {
            auto* s = static_cast<parser::v2::LiteralTsQueryExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TSQUERY);
            inst.flags = 0;
            Value::Bytes bytes;
            if (s->text != parser::v2::StringPool::INVALID_ID) {
                auto text = std::string(pool_.get(s->text));
                bytes.assign(text.begin(), text.end());
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::LiteralBlobLocatorExpr: {
            auto* s = static_cast<parser::v2::LiteralBlobLocatorExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_BLOB_LOCATOR);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->blob_id, bytes);
            appendLE16(static_cast<uint16_t>(s->blob_subtype), bytes);
            appendLE64(static_cast<uint64_t>(s->blob_length), bytes);
            bytes.push_back(s->compression);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v2::ASTKind::ColumnRefExpr:
            return emitColumnRef(static_cast<parser::v2::ColumnRefExpr*>(expr));
        case parser::v2::ASTKind::ParameterExpr: {
            auto* p = static_cast<parser::v2::ParameterExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_VAR_LOAD);
            inst.flags = 0;
            std::string name;
            if (p->is_named && p->name != parser::v2::StringPool::INVALID_ID) {
                name = std::string(pool_.get(p->name));
            } else {
                name = "$" + std::to_string(p->index);
            }
            Value::Object var;
            var["name"] = Value(name);
            Value::Object payload;
            payload["var"] = Value(std::move(var));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::BinaryExpr:
            return emitBinary(static_cast<parser::v2::BinaryExpr*>(expr));
        case parser::v2::ASTKind::UnaryExpr:
            return emitUnary(static_cast<parser::v2::UnaryExpr*>(expr));
        case parser::v2::ASTKind::FunctionCallExpr:
            return emitFunctionCall(static_cast<parser::v2::FunctionCallExpr*>(expr));
        case parser::v2::ASTKind::CastExpr:
            return emitCast(static_cast<parser::v2::CastExpr*>(expr));
        case parser::v2::ASTKind::ExtractExpr: {
            auto* e = static_cast<parser::v2::ExtractExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXTRACT);
            inst.flags = 0;
            Value::Object payload;
            Value::List args;
            args.push_back(Value(makeInstr(selectorToInstr(e->selector))));
            for (auto* arg : e->selector.args) {
                args.push_back(Value(makeInstr(emitExpression(arg))));
            }
            if (e->source) args.push_back(Value(makeInstr(emitExpression(e->source))));
            payload["args"] = Value(std::move(args));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::AlterElementExpr: {
            auto* e = static_cast<parser::v2::AlterElementExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_ELEMENT);
            inst.flags = 0;
            Value::Object payload;
            Value::List args;
            args.push_back(Value(makeInstr(selectorToInstr(e->selector))));
            for (auto* arg : e->selector.args) {
                args.push_back(Value(makeInstr(emitExpression(arg))));
            }
            if (e->source) args.push_back(Value(makeInstr(emitExpression(e->source))));
            if (e->new_value) args.push_back(Value(makeInstr(emitExpression(e->new_value))));
            payload["args"] = Value(std::move(args));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::ASTKind::CaseExpr:
            return emitCase(static_cast<parser::v2::CaseExpr*>(expr));
        case parser::v2::ASTKind::InExpr:
            return emitIn(static_cast<parser::v2::InExpr*>(expr));
        case parser::v2::ASTKind::BetweenExpr:
            return emitBetween(static_cast<parser::v2::BetweenExpr*>(expr));
        case parser::v2::ASTKind::LikeExpr:
            return emitLike(static_cast<parser::v2::LikeExpr*>(expr));
        case parser::v2::ASTKind::ExistsExpr:
            return emitExists(static_cast<parser::v2::ExistsExpr*>(expr));
        case parser::v2::ASTKind::SubqueryExpr:
            return emitSubquery(static_cast<parser::v2::SubqueryExpr*>(expr));
        case parser::v2::ASTKind::IsNullExpr: {
            auto* s = static_cast<parser::v2::IsNullExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_IS_NULL);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(s->expr)));
            inst.payload = Value(std::move(payload));
            if (s->negated) {
                Instruction not_inst;
                not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
                not_inst.flags = 0;
                Value::Object not_payload;
                not_payload["value"] = Value(makeInstr(inst));
                not_inst.payload = Value(std::move(not_payload));
                return not_inst;
            }
            return inst;
        }
        case parser::v2::ASTKind::ArrayExpr: {
            auto* s = static_cast<parser::v2::ArrayExpr*>(expr);
            Instruction inst;
            if (s->has_subquery && s->subquery) {
                inst.opcode = op(Opcode::SBLR3_SUBQUERY_ARRAY);
                inst.flags = 0;
                Value::Object payload;
                payload["query"] = Value(makeInstr(emitSelect(s->subquery)));
                inst.payload = Value(std::move(payload));
                return inst;
            }
            inst.opcode = op(Opcode::SBLR3_ARRAY_CONSTRUCT);
            inst.flags = 0;
            Value::Object payload;
            payload["args"] = toExprList(s->elements);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
    inst.flags = 0;
    inst.payload = Value(Value::Object{{"value", Value()}});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitLiteral(parser::v2::LiteralExpr* lit) {
    Instruction inst;
    inst.flags = 0;
    Value::Object payload;
    if (!lit) {
        inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
        payload["value"] = Value();
        inst.payload = Value(std::move(payload));
        return inst;
    }
    switch (lit->literal_type) {
        case parser::v2::LiteralType::INTEGER:
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT64);
            payload["value"] = Value(int64_t(lit->int_value));
            break;
        case parser::v2::LiteralType::FLOAT:
            inst.opcode = op(Opcode::SBLR3_LITERAL_DOUBLE);
            payload["value"] = Value(double(lit->float_value));
            break;
        case parser::v2::LiteralType::STRING:
            inst.opcode = op(Opcode::SBLR3_LITERAL_STRING);
            payload["value"] = Value(std::string(pool_.get(lit->string_value)));
            break;
        case parser::v2::LiteralType::BLOB:
            inst.opcode = op(Opcode::SBLR3_LITERAL_BINARY);
            {
                std::string s(pool_.get(lit->string_value));
                Value::Bytes b(s.begin(), s.end());
                payload["value"] = Value(std::move(b));
            }
            break;
        case parser::v2::LiteralType::BOOLEAN:
            inst.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
            payload["value"] = Value(lit->bool_value);
            break;
        case parser::v2::LiteralType::NULL_VALUE:
            inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
            payload["value"] = Value();
            break;
        case parser::v2::LiteralType::DEFAULT:
            inst.opcode = op(Opcode::SBLR3_DEFAULT_VALUE);
            inst.payload = Value(Value::Bytes{});
            return inst;
    }
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitColumnRef(parser::v2::ColumnRefExpr* ref) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_COLUMN_REF);
    inst.flags = 0;
    Value::Object payload;
    if (ref->column.has_table_qualifier) {
        payload["path"] = toSchemaPath(ref->column.table_path);
    } else {
        payload["path"] = Value(Value::List{});
    }
    payload["column"] = toIdent(ref->column.column_name);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitBinary(parser::v2::BinaryExpr* expr) {
    if (!expr) {
        return emitLiteral(nullptr);
    }
    Instruction inst;
    inst.flags = 0;
    bool mapped = true;
    Value::Object payload;
    payload["lhs"] = Value(makeInstr(emitExpression(expr->left)));
    payload["rhs"] = Value(makeInstr(emitExpression(expr->right)));

    switch (expr->op) {
        case parser::v2::BinaryOp::ADD: inst.opcode = op(Opcode::SBLR3_EXPR_ADD); break;
        case parser::v2::BinaryOp::SUB: inst.opcode = op(Opcode::SBLR3_EXPR_SUBTRACT); break;
        case parser::v2::BinaryOp::MUL: inst.opcode = op(Opcode::SBLR3_EXPR_MULTIPLY); break;
        case parser::v2::BinaryOp::DIV: inst.opcode = op(Opcode::SBLR3_EXPR_DIVIDE); break;
        case parser::v2::BinaryOp::DIV_INT: inst.opcode = op(Opcode::SBLR3_EXPR_DIV_INT); break;
        case parser::v2::BinaryOp::MOD: inst.opcode = op(Opcode::SBLR3_EXPR_MODULO); break;
        case parser::v2::BinaryOp::POWER: {
            inst.opcode = op(Opcode::SBLR3_FUNC_POWER);
            Value::Object f;
            Value::List args;
            args.push_back(Value(makeInstr(emitExpression(expr->left))));
            args.push_back(Value(makeInstr(emitExpression(expr->right))));
            f["args"] = Value(std::move(args));
            inst.payload = Value(std::move(f));
            return inst;
        }
        case parser::v2::BinaryOp::EQ: inst.opcode = op(Opcode::SBLR3_EXPR_EQ); break;
        case parser::v2::BinaryOp::NE: inst.opcode = op(Opcode::SBLR3_EXPR_NE); break;
        case parser::v2::BinaryOp::LT: inst.opcode = op(Opcode::SBLR3_EXPR_LT); break;
        case parser::v2::BinaryOp::LE: inst.opcode = op(Opcode::SBLR3_EXPR_LE); break;
        case parser::v2::BinaryOp::GT: inst.opcode = op(Opcode::SBLR3_EXPR_GT); break;
        case parser::v2::BinaryOp::GE: inst.opcode = op(Opcode::SBLR3_EXPR_GE); break;
        case parser::v2::BinaryOp::AND: inst.opcode = op(Opcode::SBLR3_EXPR_AND); break;
        case parser::v2::BinaryOp::OR: inst.opcode = op(Opcode::SBLR3_EXPR_OR); break;
        case parser::v2::BinaryOp::BIT_AND: inst.opcode = op(Opcode::SBLR3_BIT_AND); break;
        case parser::v2::BinaryOp::BIT_OR: inst.opcode = op(Opcode::SBLR3_BIT_OR); break;
        case parser::v2::BinaryOp::BIT_XOR: inst.opcode = op(Opcode::SBLR3_BIT_XOR); break;
        case parser::v2::BinaryOp::SHIFT_LEFT: inst.opcode = op(Opcode::SBLR3_BIT_SHIFT_LEFT); break;
        case parser::v2::BinaryOp::SHIFT_RIGHT: inst.opcode = op(Opcode::SBLR3_BIT_SHIFT_RIGHT); break;
        case parser::v2::BinaryOp::REGEX_MATCH: inst.opcode = op(Opcode::SBLR3_REGEX_MATCH); break;
        case parser::v2::BinaryOp::REGEX_MATCH_CI: inst.opcode = op(Opcode::SBLR3_REGEX_MATCH_CI); break;
        case parser::v2::BinaryOp::REGEX_NOT_MATCH: inst.opcode = op(Opcode::SBLR3_REGEX_NOT_MATCH); break;
        case parser::v2::BinaryOp::REGEX_NOT_MATCH_CI: inst.opcode = op(Opcode::SBLR3_REGEX_NOT_MATCH_CI); break;
        case parser::v2::BinaryOp::JSON_EXTRACT: inst.opcode = op(Opcode::SBLR3_JSON_EXTRACT); break;
        case parser::v2::BinaryOp::JSON_EXTRACT_TEXT: inst.opcode = op(Opcode::SBLR3_JSON_DOUBLE_ARROW); break;
        case parser::v2::BinaryOp::JSON_HASH_EXTRACT: inst.opcode = op(Opcode::SBLR3_JSON_HASH_ARROW); break;
        case parser::v2::BinaryOp::JSON_HASH_EXTRACT_TEXT: inst.opcode = op(Opcode::SBLR3_JSON_HASH_DOUBLE_ARROW); break;
        case parser::v2::BinaryOp::JSON_EXISTS:
        case parser::v2::BinaryOp::JSON_EXISTS_ANY:
        case parser::v2::BinaryOp::JSON_EXISTS_ALL: {
            inst.opcode = op(Opcode::SBLR3_FUNC_JSON_EXISTS);
            Value::Object f;
            Value::List args;
            args.push_back(Value(makeInstr(emitExpression(expr->left))));
            args.push_back(Value(makeInstr(emitExpression(expr->right))));
            f["args"] = Value(std::move(args));
            inst.payload = Value(std::move(f));
            return inst;
        }
        case parser::v2::BinaryOp::ARRAY_CONTAINS: inst.opcode = op(Opcode::SBLR3_ARRAY_CONTAINS); break;
        case parser::v2::BinaryOp::ARRAY_CONTAINED_BY: inst.opcode = op(Opcode::SBLR3_ARRAY_CONTAINED_BY); break;
        case parser::v2::BinaryOp::ARRAY_OVERLAP: inst.opcode = op(Opcode::SBLR3_ARRAY_OVERLAP); break;
        case parser::v2::BinaryOp::CONCAT: {
            inst.opcode = op(Opcode::SBLR3_FUNC_CONCAT);
            Value::Object f;
            Value::List args;
            args.push_back(Value(makeInstr(emitExpression(expr->left))));
            args.push_back(Value(makeInstr(emitExpression(expr->right))));
            f["args"] = Value(std::move(args));
            inst.payload = Value(std::move(f));
            return inst;
        }
        default:
            mapped = false;
            break;
    }
    if (!mapped) {
        return emitLiteral(nullptr);
    }
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitUnary(parser::v2::UnaryExpr* expr) {
    if (!expr) return emitLiteral(nullptr);
    switch (expr->op) {
        case parser::v2::UnaryOp::NOT: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::UnaryOp::BIT_NOT: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_BIT_NOT);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::UnaryOp::NEGATE: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_SUBTRACT);
            inst.flags = 0;
            Value::Object payload;
            payload["lhs"] = Value(makeInstr(emitLiteralZero()));
            payload["rhs"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v2::UnaryOp::IS_NULL:
        case parser::v2::UnaryOp::IS_NOT_NULL: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_IS_NULL);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            if (expr->op == parser::v2::UnaryOp::IS_NOT_NULL) {
                Instruction not_inst;
                not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
                not_inst.flags = 0;
                Value::Object not_payload;
                not_payload["value"] = Value(makeInstr(inst));
                not_inst.payload = Value(std::move(not_payload));
                return not_inst;
            }
            return inst;
        }
    }
    return emitLiteral(nullptr);
}

scratchbird::sblr::v3::Instruction V3Emitter::emitFunctionCall(parser::v2::FunctionCallExpr* expr) {
    Instruction inst;
    inst.flags = 0;

    std::string name = toUpper(pool_.get(expr->function_path.objectName()));
    static const std::unordered_map<std::string, Opcode> kFuncMap = {
        {"COALESCE", Opcode::SBLR3_COALESCE},
        {"NULLIF", Opcode::SBLR3_NULLIF},
        {"POWER", Opcode::SBLR3_FUNC_POWER},
        {"ABS", Opcode::SBLR3_FUNC_ABS},
        {"SIN", Opcode::SBLR3_FUNC_SIN},
        {"COS", Opcode::SBLR3_FUNC_COS},
        {"TAN", Opcode::SBLR3_FUNC_TAN},
        {"CONCAT", Opcode::SBLR3_FUNC_CONCAT},
        {"ARRAY_AGG", Opcode::SBLR3_ARRAY_AGG},
    };

    auto it = kFuncMap.find(name);
    if (it != kFuncMap.end()) {
        inst.opcode = op(it->second);
    } else {
        inst.opcode = op(Opcode::SBLR3_EXPR_FUNCTION_CALL);
    }

    Value::Object payload;
    payload["args"] = toExprList(expr->arguments);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitCast(parser::v2::CastExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXPR_CAST);
    inst.flags = 0;
    Value::Object payload;
    payload["value"] = Value(makeInstr(emitExpression(expr->expr)));
    payload["type"] = Value(buildTypeSpec(expr->target_type));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitCase(parser::v2::CaseExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_CASE_WHEN);
    inst.flags = 0;
    Value::Object payload;
    if (expr->operand) payload["base"] = Value(makeInstr(emitExpression(expr->operand)));
    payload["when_count"] = Value(uint64_t(expr->when_clauses.size()));
    if (!expr->when_clauses.empty()) {
        payload["when"] = Value(makeInstr(emitExpression(expr->when_clauses.front().when_expr)));
        payload["then"] = Value(makeInstr(emitExpression(expr->when_clauses.front().then_expr)));
    }
    if (expr->else_expr) payload["else"] = Value(makeInstr(emitExpression(expr->else_expr)));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitIn(parser::v2::InExpr* expr) {
    Instruction inst;
    if (expr->has_subquery) {
        inst.opcode = op(expr->negated ? Opcode::SBLR3_SUBQUERY_NOT_IN : Opcode::SBLR3_SUBQUERY_IN);
    } else {
        inst.opcode = op(Opcode::SBLR3_IN_LIST);
    }
    inst.flags = 0;
    Value::Object payload;
    payload["value"] = Value(makeInstr(emitExpression(expr->expr)));
    Value::List list;
    if (expr->has_subquery && expr->subquery) {
        Instruction sub;
        sub.opcode = op(Opcode::SBLR3_SUBQUERY_SCALAR);
        sub.flags = 0;
        sub.payload = Value(Value::Object{{"query", Value(makeInstr(emitSelect(expr->subquery)))}}); 
        list.push_back(Value(makeInstr(sub)));
    } else {
        for (auto* v : expr->values) list.push_back(Value(makeInstr(emitExpression(v))));
    }
    payload["list"] = Value(std::move(list));
    payload["negated"] = Value(expr->negated);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitBetween(parser::v2::BetweenExpr* expr) {
    // Emit as (expr >= low AND expr <= high)
    auto left = emitExpression(expr->expr);
    Instruction ge;
    ge.opcode = op(Opcode::SBLR3_EXPR_GE);
    ge.flags = 0;
    ge.payload = Value(Value::Object{
        {"lhs", Value(makeInstr(left))},
        {"rhs", Value(makeInstr(emitExpression(expr->low)))}});

    Instruction le;
    le.opcode = op(Opcode::SBLR3_EXPR_LE);
    le.flags = 0;
    le.payload = Value(Value::Object{
        {"lhs", Value(makeInstr(left))},
        {"rhs", Value(makeInstr(emitExpression(expr->high)))}});

    Instruction and_inst;
    and_inst.opcode = op(Opcode::SBLR3_EXPR_AND);
    and_inst.flags = 0;
    and_inst.payload = Value(Value::Object{
        {"lhs", Value(makeInstr(ge))},
        {"rhs", Value(makeInstr(le))}});

    if (expr->negated) {
        Instruction not_inst;
        not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
        not_inst.flags = 0;
        not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(and_inst))}});
        return not_inst;
    }
    return and_inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitLike(parser::v2::LikeExpr* expr) {
    Instruction inst;
    inst.flags = 0;

    if (expr->match_kind == parser::v2::LikeMatchKind::CONTAINING ||
        expr->match_kind == parser::v2::LikeMatchKind::STARTING) {
        inst.opcode = op(expr->match_kind == parser::v2::LikeMatchKind::CONTAINING
                             ? Opcode::SBLR3_PRED_CONTAINING
                             : Opcode::SBLR3_PRED_STARTING_WITH);
        Value::Object payload;
        payload["lhs"] = Value(makeInstr(emitExpression(expr->expr)));
        payload["rhs"] = Value(makeInstr(emitExpression(expr->pattern)));
        inst.payload = Value(std::move(payload));
        if (expr->negated) {
            Instruction not_inst;
            not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
            not_inst.flags = 0;
            not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(inst))}});
            return not_inst;
        }
        return inst;
    }

    if (expr->match_kind == parser::v2::LikeMatchKind::SIMILAR) {
        inst.opcode = op(expr->case_insensitive ? Opcode::SBLR3_REGEX_MATCH_CI : Opcode::SBLR3_REGEX_MATCH);
        Value::Object payload;
        payload["lhs"] = Value(makeInstr(emitExpression(expr->expr)));
        payload["rhs"] = Value(makeInstr(emitExpression(expr->pattern)));
        inst.payload = Value(std::move(payload));
        if (expr->negated) {
            Instruction not_inst;
            not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
            not_inst.flags = 0;
            not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(inst))}});
            return not_inst;
        }
        return inst;
    }

    inst.opcode = op(expr->case_insensitive ? Opcode::SBLR3_EXPR_ILIKE : Opcode::SBLR3_EXPR_LIKE);
    Value::Object payload;
    payload["value"] = Value(makeInstr(emitExpression(expr->expr)));
    payload["pattern"] = Value(makeInstr(emitExpression(expr->pattern)));
    if (expr->escape) payload["escape"] = Value(makeInstr(emitExpression(expr->escape)));
    payload["negated"] = Value(expr->negated);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitExists(parser::v2::ExistsExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_SUBQUERY_EXISTS);
    inst.flags = 0;
    Value::Object payload;
    if (expr->subquery) payload["query"] = Value(makeInstr(emitSelect(expr->subquery)));
    inst.payload = Value(std::move(payload));
    if (expr->negated) {
        Instruction not_inst;
        not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
        not_inst.flags = 0;
        not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(inst))}});
        return not_inst;
    }
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitSubquery(parser::v2::SubqueryExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_SUBQUERY_SCALAR);
    inst.flags = 0;
    Value::Object payload;
    if (expr->subquery) payload["query"] = Value(makeInstr(emitSelect(expr->subquery)));
    inst.payload = Value(std::move(payload));
    return inst;
}

Value V3Emitter::toIdent(parser::v2::StringPool::StringId id) {
    if (id == parser::v2::StringPool::INVALID_ID) return Value(std::string());
    return Value(std::string(pool_.get(id)));
}

Value V3Emitter::toSchemaPath(const parser::v2::SchemaPath& path) {
    Value::List parts;
    for (auto id : path.components) {
        parts.push_back(toIdent(id));
    }
    return Value(std::move(parts));
}

Value V3Emitter::toExprList(const std::vector<parser::v2::Expression*>& exprs) {
    Value::List list;
    list.reserve(exprs.size());
    for (auto* expr : exprs) {
        list.push_back(Value(makeInstr(emitExpression(expr))));
    }
    return Value(std::move(list));
}

Value V3Emitter::toSelectItems(const std::vector<parser::v2::SelectItem*>& items) {
    Value::List list;
    for (auto* item : items) {
        if (item->item_type == parser::v2::SelectItem::Type::EXPRESSION && item->expr) {
            list.push_back(Value(makeInstr(emitExpression(item->expr))));
        } else if (item->item_type == parser::v2::SelectItem::Type::STAR) {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_SELECT_STAR);
            inst.flags = 0;
            inst.payload = Value(Value::Bytes{});
            list.push_back(Value(makeInstr(inst)));
        } else if (item->item_type == parser::v2::SelectItem::Type::TABLE_STAR) {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_SELECT_TABLE_STAR);
            inst.flags = 0;
            inst.payload = Value(Value::Bytes{});
            list.push_back(Value(makeInstr(inst)));
        }
    }
    return Value(std::move(list));
}

Value V3Emitter::toOrderBy(const std::vector<parser::v2::OrderByItem*>& items) {
    Value::List list;
    for (auto* item : items) {
        Value::Object o;
        o["expr"] = Value(makeInstr(emitExpression(item->expr)));
        o["order"] = Value(uint64_t(mapSortOrder(item->ascending)));
        o["nulls"] = Value(uint64_t(mapNullsOrder(item)));
        list.push_back(Value(std::move(o)));
    }
    return Value(std::move(list));
}

Value V3Emitter::toTableRef(parser::v2::TableRefNode* node) {
    Value::Object o;
    if (node->ref_type == parser::v2::TableRefNode::Type::TABLE) {
        o["table_path"] = toSchemaPath(node->table_path);
    } else {
        // Subquery/function references are encoded as empty table_path with alias only (placeholder).
        o["table_path"] = Value(Value::List{});
    }
    if (node->has_alias) o["alias"] = toIdent(node->alias);
    o["table_flags"] = Value(uint64_t(0));
    return Value(std::move(o));
}

Value V3Emitter::toTableRefFromPath(const parser::v2::SchemaPath& path, parser::v2::StringPool::StringId alias) {
    Value::Object o;
    o["table_path"] = toSchemaPath(path);
    if (alias != parser::v2::StringPool::INVALID_ID) o["alias"] = toIdent(alias);
    o["table_flags"] = Value(uint64_t(0));
    return Value(std::move(o));
}

Value V3Emitter::toJoins(const std::vector<parser::v2::JoinNode*>& joins) {
    Value::List list;
    for (auto* join : joins) {
        Value::Object j;
        j["type"] = Value(uint64_t(mapJoinType(join->join_type)));
        if (join->right) j["right"] = toTableRef(join->right);
        if (join->on_condition) j["condition"] = Value(makeInstr(emitExpression(join->on_condition)));
        Value::List using_cols;
        for (auto id : join->using_columns) using_cols.push_back(toIdent(id));
        j["using"] = Value(std::move(using_cols));
        list.push_back(Value(std::move(j)));
    }
    return Value(std::move(list));
}

Value V3Emitter::toStmtList(const std::vector<parser::v2::Statement*>& stmts) {
    Value::List list;
    for (auto* stmt : stmts) {
        if (!stmt) continue;
        list.push_back(Value(makeInstr(emitStatement(stmt))));
    }
    return Value(std::move(list));
}

Value V3Emitter::emitColumnDef(parser::v2::ColumnDef* col) {
    Value::Object payload;
    payload["name"] = toIdent(col->name);
    payload["type"] = Value(buildTypeSpec(col->type));

    uint16_t flags = 0;
    Expression* default_expr = nullptr;
    Expression* generated_expr = nullptr;
    Value identity;
    Value::List checks;
    parser::v2::StringPool::StringId collation = parser::v2::StringPool::INVALID_ID;

    for (const auto& c : col->constraints) {
        switch (c.type) {
            case parser::v2::ConstraintType::NOT_NULL:
                flags |= 0x0001;
                break;
            case parser::v2::ConstraintType::NULL_ALLOWED:
                flags |= 0x0002;
                break;
            case parser::v2::ConstraintType::DEFAULT:
                default_expr = c.default_expr;
                break;
            case parser::v2::ConstraintType::GENERATED:
                generated_expr = c.generated_expr;
                if (c.generated_always) flags |= 0x0004;
                break;
            case parser::v2::ConstraintType::CHECK:
                if (c.check_expr) checks.push_back(Value(makeInstr(emitExpression(c.check_expr))));
                break;
            case parser::v2::ConstraintType::COLLATE:
                collation = c.collation;
                break;
            default:
                break;
        }
    }

    if (col->is_computed) {
        generated_expr = col->computed_expr;
        flags |= col->computed_stored ? 0x0004 : 0x0008;
    }

    payload["flags"] = Value(uint64_t(flags));
    if (default_expr) payload["default_expr"] = Value(makeInstr(emitExpression(default_expr)));
    if (generated_expr) payload["generated_expr"] = Value(makeInstr(emitExpression(generated_expr)));
    if (!identity.isNull()) payload["identity"] = identity;
    if (collation != parser::v2::StringPool::INVALID_ID) payload["collation"] = toIdent(collation);
    payload["check_count"] = Value(uint64_t(checks.size()));
    if (!checks.empty()) payload["check_expr"] = checks.front();
    return Value(std::move(payload));
}

Value V3Emitter::emitTableConstraint(parser::v2::TableConstraint* c) {
    Value::Object payload;
    uint8_t type = 4;
    if (c->type == parser::v2::TableConstraintType::PRIMARY_KEY) type = 1;
    if (c->type == parser::v2::TableConstraintType::UNIQUE) type = 2;
    if (c->type == parser::v2::TableConstraintType::FOREIGN_KEY) type = 3;
    payload["type"] = Value(uint64_t(type));
    if (c->name != parser::v2::StringPool::INVALID_ID) payload["name"] = toIdent(c->name);
    Value::List cols;
    for (auto id : c->columns) cols.push_back(toIdent(id));
    payload["columns"] = Value(std::move(cols));
    if (c->type == parser::v2::TableConstraintType::FOREIGN_KEY) {
        payload["ref_table"] = toSchemaPath(c->ref_table);
        Value::List refcols;
        for (auto id : c->ref_columns) refcols.push_back(toIdent(id));
        payload["ref_columns"] = Value(std::move(refcols));
        payload["on_update"] = Value(uint64_t(static_cast<uint8_t>(c->on_update)));
        payload["on_delete"] = Value(uint64_t(static_cast<uint8_t>(c->on_delete)));
    }
    if (c->type == parser::v2::TableConstraintType::CHECK && c->check_expr) {
        payload["check_expr"] = Value(makeInstr(emitExpression(c->check_expr)));
    }
    return Value(std::move(payload));
}

Value V3Emitter::emitColumnRefValue(parser::v2::StringPool::StringId column_id) {
    Value::Object payload;
    payload["path"] = Value(Value::List{});
    payload["column"] = toIdent(column_id);
    return Value(std::move(payload));
}

Value V3Emitter::emitVarRefValue(parser::v2::StringPool::StringId name) {
    Value::Object payload;
    payload["name"] = toIdent(name);
    return Value(std::move(payload));
}

scratchbird::sblr::v3::Instruction V3Emitter::emitLiteralZero() {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_LITERAL_INT64);
    inst.flags = 0;
    inst.payload = Value(Value::Object{{"value", Value(int64_t(0))}});
    return inst;
}

TypeSpec V3Emitter::buildTypeSpec(const parser::v2::TypeName& type) {
    std::string name;
    if (type.has_schema_path) {
        if (!type.schema_path.components.empty()) {
            name = std::string(pool_.get(type.schema_path.components.back()));
        }
    } else if (type.name != parser::v2::StringPool::INVALID_ID) {
        name = std::string(pool_.get(type.name));
    }
    std::string upper = toUpper(name);
    if (upper == "TIME" && type.with_time_zone) upper = "TIME_TZ";
    if (upper == "TIMESTAMP" && type.with_time_zone) upper = "TIMESTAMP_TZ";

    static const std::unordered_map<std::string, Opcode> kTypeMap = {
        {"INT", Opcode::SBLR3_TYPE_INTEGER},
        {"INTEGER", Opcode::SBLR3_TYPE_INTEGER},
        {"BIGINT", Opcode::SBLR3_TYPE_BIGINT},
        {"SMALLINT", Opcode::SBLR3_TYPE_INT16},
        {"TINYINT", Opcode::SBLR3_TYPE_INT8},
        {"INT128", Opcode::SBLR3_TYPE_INT128},
        {"INT2", Opcode::SBLR3_TYPE_INT16},
        {"INT4", Opcode::SBLR3_TYPE_INTEGER},
        {"INT8", Opcode::SBLR3_TYPE_INT8},
        {"UINT8", Opcode::SBLR3_TYPE_UINT8},
        {"UINT16", Opcode::SBLR3_TYPE_UINT16},
        {"UINT32", Opcode::SBLR3_TYPE_UINT32},
        {"UINT64", Opcode::SBLR3_TYPE_UINT64},
        {"UINT128", Opcode::SBLR3_TYPE_UINT128},
        {"DECIMAL", Opcode::SBLR3_TYPE_DECIMAL},
        {"NUMERIC", Opcode::SBLR3_TYPE_DECIMAL},
        {"REAL", Opcode::SBLR3_TYPE_FLOAT32},
        {"FLOAT", Opcode::SBLR3_TYPE_FLOAT32},
        {"DOUBLE", Opcode::SBLR3_TYPE_DOUBLE},
        {"DOUBLE PRECISION", Opcode::SBLR3_TYPE_DOUBLE},
        {"BOOLEAN", Opcode::SBLR3_TYPE_BOOLEAN},
        {"BOOL", Opcode::SBLR3_TYPE_BOOLEAN},
        {"CHAR", Opcode::SBLR3_TYPE_CHAR},
        {"CHARACTER", Opcode::SBLR3_TYPE_CHAR},
        {"VARCHAR", Opcode::SBLR3_TYPE_VARCHAR},
        {"TEXT", Opcode::SBLR3_TYPE_TEXT},
        {"DATE", Opcode::SBLR3_TYPE_DATE},
        {"TIME", Opcode::SBLR3_TYPE_TIME},
        {"TIMESTAMP", Opcode::SBLR3_TYPE_TIMESTAMP},
        {"TIME_TZ", Opcode::SBLR3_TYPE_TIME_TZ},
        {"TIMESTAMP_TZ", Opcode::SBLR3_TYPE_TIMESTAMP_TZ},
        {"UUID", Opcode::SBLR3_TYPE_UUID},
        {"JSON", Opcode::SBLR3_TYPE_JSON},
        {"JSONB", Opcode::SBLR3_TYPE_JSONB},
        {"JSONPATH", Opcode::SBLR3_TYPE_JSONPATH},
        {"BLOB", Opcode::SBLR3_TYPE_BLOB},
        {"BLOB_TEXT", Opcode::SBLR3_TYPE_BLOB_TEXT},
        {"BYTEA", Opcode::SBLR3_TYPE_BYTEA},
        {"VARBINARY", Opcode::SBLR3_TYPE_VARBINARY},
        {"BINARY", Opcode::SBLR3_TYPE_BINARY},
        {"XML", Opcode::SBLR3_TYPE_XML},
        {"MONEY", Opcode::SBLR3_TYPE_MONEY},
        {"INTERVAL", Opcode::SBLR3_TYPE_INTERVAL},
        {"INET", Opcode::SBLR3_TYPE_INET},
        {"CIDR", Opcode::SBLR3_TYPE_CIDR},
        {"MACADDR", Opcode::SBLR3_TYPE_MACADDR},
        {"MACADDR8", Opcode::SBLR3_TYPE_MACADDR8},
        {"BIT", Opcode::SBLR3_TYPE_BIT},
        {"YEAR", Opcode::SBLR3_TYPE_YEAR},
        {"DATETIME", Opcode::SBLR3_TYPE_DATETIME},
        {"MEDIUMINT", Opcode::SBLR3_TYPE_MEDIUMINT},
        {"TSVECTOR", Opcode::SBLR3_TYPE_TSVECTOR},
        {"TSQUERY", Opcode::SBLR3_TYPE_TSQUERY},
        {"VECTOR", Opcode::SBLR3_TYPE_VECTOR},
        {"GEOMETRY", Opcode::SBLR3_TYPE_GEOMETRY},
        {"POINT", Opcode::SBLR3_TYPE_POINT},
        {"LINESTRING", Opcode::SBLR3_TYPE_LINESTRING},
        {"POLYGON", Opcode::SBLR3_TYPE_POLYGON},
        {"MULTIPOINT", Opcode::SBLR3_TYPE_MULTIPOINT},
        {"MULTILINESTRING", Opcode::SBLR3_TYPE_MULTILINESTRING},
        {"MULTIPOLYGON", Opcode::SBLR3_TYPE_MULTIPOLYGON},
        {"GEOMETRYCOLLECTION", Opcode::SBLR3_TYPE_GEOMETRYCOLLECTION},
        {"ENUM", Opcode::SBLR3_TYPE_ENUM},
        {"SET", Opcode::SBLR3_TYPE_SET},
        {"ROW", Opcode::SBLR3_TYPE_ROW},
        {"COMPOSITE", Opcode::SBLR3_TYPE_COMPOSITE},
        {"DOMAIN", Opcode::SBLR3_TYPE_DOMAIN},
        {"VARIANT", Opcode::SBLR3_TYPE_VARIANT},
        {"ARRAY", Opcode::SBLR3_TYPE_ARRAY},
        {"INT4RANGE", Opcode::SBLR3_TYPE_INT4RANGE},
        {"INT8RANGE", Opcode::SBLR3_TYPE_INT8RANGE},
        {"NUMRANGE", Opcode::SBLR3_TYPE_NUMRANGE},
        {"DATERANGE", Opcode::SBLR3_TYPE_DATERANGE},
        {"TSRANGE", Opcode::SBLR3_TYPE_TSRANGE},
        {"TSTZRANGE", Opcode::SBLR3_TYPE_TSTZRANGE},
    };

    TypeSpec spec;
    auto it = kTypeMap.find(upper);
    if (it != kTypeMap.end()) {
        spec.type_opcode = op(it->second);
    } else if (type.has_schema_path) {
        spec.type_opcode = op(Opcode::SBLR3_TYPE_DOMAIN);
        spec.type_payload = std::vector<uint8_t>(16, 0);
    } else {
        spec.type_opcode = op(Opcode::SBLR3_TYPE_UNKNOWN);
    }
    return spec;
}

Value::Bytes V3Emitter::encodeInstructionBytes(const Instruction& inst) {
    Buffer out;
    DecodeError err;
    if (!scratchbird::sblr::v3::encodeInstructionWithSchema(inst, out, err)) {
        scratchbird::sblr::v3::encodeInstruction(inst, out);
    }
    return out;
}

}  // namespace scratchbird::parser::v3
