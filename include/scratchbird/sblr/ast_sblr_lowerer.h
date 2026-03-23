#pragma once

#include <memory>
#include <string>
#include <vector>

#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"

namespace scratchbird::parser::v3 {

class AstSblrLowerer {
public:
    explicit AstSblrLowerer(parser::v3::StringPool& pool);

    // Build a complete V3 container for a single statement
    bool emitStatementToContainer(parser::v3::Statement* stmt,
                                  scratchbird::sblr::v3::Container& out,
                                  std::string& err);

    // vNext section-04 contract shim used by gate tests. This does not parse SQL.
    static bool emitVNextContractInstruction(
        parser::v3::ASTKind node_kind,
        const scratchbird::sblr::v3::Value::Object& fields,
        scratchbird::sblr::v3::Instruction& out,
        std::string& err);

private:
    parser::v3::StringPool& pool_;
    bool ok_ = true;
    std::string error_;

    void fail(const std::string& message);

    scratchbird::sblr::v3::Instruction emitStatement(parser::v3::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitSelect(parser::v3::SelectStmt* stmt);
    scratchbird::sblr::v3::Instruction emitInsert(parser::v3::InsertStmt* stmt);
    scratchbird::sblr::v3::Instruction emitUpdate(parser::v3::UpdateStmt* stmt);
    scratchbird::sblr::v3::Instruction emitDelete(parser::v3::DeleteStmt* stmt);
    scratchbird::sblr::v3::Instruction emitMerge(parser::v3::MergeStmt* stmt);
    scratchbird::sblr::v3::Instruction emitCopy(parser::v3::CopyStmt* stmt);

    scratchbird::sblr::v3::Instruction emitDdlCreate(parser::v3::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitDdlAlter(parser::v3::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitDdlDrop(parser::v3::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitDdlTruncate(parser::v3::TruncateTableStmt* stmt);
    scratchbird::sblr::v3::Instruction emitComment(parser::v3::CommentStmt* stmt);
    scratchbird::sblr::v3::Instruction emitGrant(parser::v3::GrantStmt* stmt);
    scratchbird::sblr::v3::Instruction emitRevoke(parser::v3::RevokeStmt* stmt);
    scratchbird::sblr::v3::Instruction emitTxn(parser::v3::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitSetShowReset(parser::v3::Statement* stmt);
    scratchbird::sblr::v3::Instruction emitUtility(parser::v3::Statement* stmt);

    scratchbird::sblr::v3::Instruction emitPsql(parser::v3::Statement* stmt);

    scratchbird::sblr::v3::Instruction emitExpression(parser::v3::Expression* expr);
    scratchbird::sblr::v3::Instruction emitLiteral(parser::v3::LiteralExpr* lit);
    scratchbird::sblr::v3::Instruction emitColumnRef(parser::v3::ColumnRefExpr* ref);
    scratchbird::sblr::v3::Instruction emitBinary(parser::v3::BinaryExpr* expr);
    scratchbird::sblr::v3::Instruction emitUnary(parser::v3::UnaryExpr* expr);
    scratchbird::sblr::v3::Instruction emitFunctionCall(parser::v3::FunctionCallExpr* expr);
    scratchbird::sblr::v3::Instruction emitCast(parser::v3::CastExpr* expr);
    scratchbird::sblr::v3::Instruction emitCase(parser::v3::CaseExpr* expr);
    scratchbird::sblr::v3::Instruction emitIn(parser::v3::InExpr* expr);
    scratchbird::sblr::v3::Instruction emitBetween(parser::v3::BetweenExpr* expr);
    scratchbird::sblr::v3::Instruction emitLike(parser::v3::LikeExpr* expr);
    scratchbird::sblr::v3::Instruction emitExists(parser::v3::ExistsExpr* expr);
    scratchbird::sblr::v3::Instruction emitSubquery(parser::v3::SubqueryExpr* expr);

    scratchbird::sblr::v3::Value toIdent(parser::v3::StringPool::StringId id);
    scratchbird::sblr::v3::Value toSchemaPath(const parser::v3::SchemaPath& path);
    scratchbird::sblr::v3::Value toExprList(const std::vector<parser::v3::Expression*>& exprs);
    scratchbird::sblr::v3::Value toSelectItems(const std::vector<parser::v3::SelectItem*>& items);
    scratchbird::sblr::v3::Value toSelectItemAliases(const std::vector<parser::v3::SelectItem*>& items);
    scratchbird::sblr::v3::Value toOrderBy(const std::vector<parser::v3::OrderByItem*>& items);
    scratchbird::sblr::v3::Value toWindowSpec(parser::v3::WindowSpec* spec);
    scratchbird::sblr::v3::Value toTableRef(parser::v3::TableRefNode* node);
    scratchbird::sblr::v3::Value toTableRefFromPath(const parser::v3::SchemaPath& path,
                                                     parser::v3::StringPool::StringId alias);
    scratchbird::sblr::v3::Value toJoins(const std::vector<parser::v3::JoinNode*>& joins);
    scratchbird::sblr::v3::Value toStmtList(const std::vector<parser::v3::Statement*>& stmts);

    scratchbird::sblr::v3::Value emitColumnDef(parser::v3::ColumnDef* col);
    scratchbird::sblr::v3::Value emitTableConstraint(parser::v3::TableConstraint* c);
    scratchbird::sblr::v3::Value emitColumnRefValue(parser::v3::StringPool::StringId column_id);
    scratchbird::sblr::v3::Value emitVarRefValue(parser::v3::StringPool::StringId name);
    scratchbird::sblr::v3::Instruction emitLiteralZero();
    scratchbird::sblr::v3::TypeSpec buildTypeSpec(const parser::v3::TypeName& type);
    scratchbird::sblr::v3::Value::Bytes encodeInstructionBytes(const scratchbird::sblr::v3::Instruction& inst);
};

}  // namespace scratchbird::parser::v3
