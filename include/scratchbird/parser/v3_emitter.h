#pragma once

#include <memory>
#include <string>
#include <vector>

#include "scratchbird/parser/ast_v2.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"

namespace scratchbird::parser::v3 {

class V3Emitter {
public:
    explicit V3Emitter(parser::v2::StringPool& pool);

    // Build a complete V3 container for a single statement
    bool emitStatementToContainer(parser::v2::Statement* stmt,
                                  scratchbird::sblr::v3::Container& out,
                                  std::string& err);

private:
    parser::v2::StringPool& pool_;
    bool ok_ = true;
    std::string error_;

    void fail(const std::string& message);

    scratchbird::sblr::v3::Instruction emitStatement(parser::v2::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitSelect(parser::v2::SelectStmt* stmt);
    scratchbird::sblr::v3::Instruction emitInsert(parser::v2::InsertStmt* stmt);
    scratchbird::sblr::v3::Instruction emitUpdate(parser::v2::UpdateStmt* stmt);
    scratchbird::sblr::v3::Instruction emitDelete(parser::v2::DeleteStmt* stmt);
    scratchbird::sblr::v3::Instruction emitMerge(parser::v2::MergeStmt* stmt);
    scratchbird::sblr::v3::Instruction emitCopy(parser::v2::CopyStmt* stmt);

    scratchbird::sblr::v3::Instruction emitDdlCreate(parser::v2::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitDdlAlter(parser::v2::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitDdlDrop(parser::v2::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitDdlTruncate(parser::v2::TruncateTableStmt* stmt);
    scratchbird::sblr::v3::Instruction emitComment(parser::v2::CommentStmt* stmt);
    scratchbird::sblr::v3::Instruction emitGrant(parser::v2::GrantStmt* stmt);
    scratchbird::sblr::v3::Instruction emitRevoke(parser::v2::RevokeStmt* stmt);
    scratchbird::sblr::v3::Instruction emitTxn(parser::v2::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitSetShowReset(parser::v2::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitUtility(parser::v2::Statement* stmt);

    scratchbird::sblr::v3::Instruction emitPsql(parser::v2::Statement* stmt);

    scratchbird::sblr::v3::Instruction emitExpression(parser::v2::Expression* expr);
    scratchbird::sblr::v3::Instruction emitLiteral(parser::v2::LiteralExpr* lit);
    scratchbird::sblr::v3::Instruction emitColumnRef(parser::v2::ColumnRefExpr* ref);
    scratchbird::sblr::v3::Instruction emitBinary(parser::v2::BinaryExpr* expr);
    scratchbird::sblr::v3::Instruction emitUnary(parser::v2::UnaryExpr* expr);
    scratchbird::sblr::v3::Instruction emitFunctionCall(parser::v2::FunctionCallExpr* expr);
    scratchbird::sblr::v3::Instruction emitCast(parser::v2::CastExpr* expr);
    scratchbird::sblr::v3::Instruction emitCase(parser::v2::CaseExpr* expr);
    scratchbird::sblr::v3::Instruction emitIn(parser::v2::InExpr* expr);
    scratchbird::sblr::v3::Instruction emitBetween(parser::v2::BetweenExpr* expr);
    scratchbird::sblr::v3::Instruction emitLike(parser::v2::LikeExpr* expr);
    scratchbird::sblr::v3::Instruction emitExists(parser::v2::ExistsExpr* expr);
    scratchbird::sblr::v3::Instruction emitSubquery(parser::v2::SubqueryExpr* expr);

    scratchbird::sblr::v3::Value toIdent(parser::v2::StringPool::StringId id);
    scratchbird::sblr::v3::Value toSchemaPath(const parser::v2::SchemaPath& path);
    scratchbird::sblr::v3::Value toExprList(const std::vector<parser::v2::Expression*>& exprs);
    scratchbird::sblr::v3::Value toSelectItems(const std::vector<parser::v2::SelectItem*>& items);
    scratchbird::sblr::v3::Value toOrderBy(const std::vector<parser::v2::OrderByItem*>& items);
    scratchbird::sblr::v3::Value toTableRef(parser::v2::TableRefNode* node);
    scratchbird::sblr::v3::Value toTableRefFromPath(const parser::v2::SchemaPath& path,
                                                     parser::v2::StringPool::StringId alias);
    scratchbird::sblr::v3::Value toJoins(const std::vector<parser::v2::JoinNode*>& joins);
    scratchbird::sblr::v3::Value toStmtList(const std::vector<parser::v2::Statement*>& stmts);

    scratchbird::sblr::v3::Value emitColumnDef(parser::v2::ColumnDef* col);
    scratchbird::sblr::v3::Value emitTableConstraint(parser::v2::TableConstraint* c);
    scratchbird::sblr::v3::Value emitColumnRefValue(parser::v2::StringPool::StringId column_id);
    scratchbird::sblr::v3::Value emitVarRefValue(parser::v2::StringPool::StringId name);
    scratchbird::sblr::v3::Instruction emitLiteralZero();
    scratchbird::sblr::v3::TypeSpec buildTypeSpec(const parser::v2::TypeName& type);
    scratchbird::sblr::v3::Value::Bytes encodeInstructionBytes(const scratchbird::sblr::v3::Instruction& inst);
};

}  // namespace scratchbird::parser::v3
