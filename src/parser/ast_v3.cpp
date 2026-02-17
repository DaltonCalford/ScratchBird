/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Parser v3.0 - AST Implementation
 *
 * See: include/scratchbird/parser/ast_v3.h
 */

#include "scratchbird/parser/ast_v3.h"
#include <cstdlib>
#include <cstring>
#include <new>

namespace scratchbird::parser::v3 {

// =============================================================================
// Statement accept() implementations
// =============================================================================

// DDL statements
void CreateTableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateIndexStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateViewStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateSequenceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterSequenceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateSchemaStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropSchemaStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterSchemaStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateDatabaseStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateTablespaceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterTablespaceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropTablespaceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AttachTablespaceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DetachTablespaceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateFunctionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateProcedureStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateTriggerStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreatePackageStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateUserStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateRoleStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateGroupStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreatePolicyStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateExceptionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateJobStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateTypeStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateDomainStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateForeignServerStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateForeignTableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateForeignDataWrapperStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateUserMappingStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateSynonymStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CreateUdrStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterTypeStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropTypeStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterDomainStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropDomainStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropDatabaseStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterDatabaseStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterTableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterPolicyStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterIndexStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void RenameObjectStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void MoveObjectStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropTableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropPolicyStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropIndexStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropViewStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropSequenceStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropFunctionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropProcedureStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropTriggerStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropPackageStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropRoleStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropUserStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropGroupStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropExceptionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropForeignServerStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropForeignTableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropUserMappingStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropSynonymStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropUdrStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DropJobStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void TruncateTableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterJobStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExecuteJobStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CancelJobRunStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// DML statements
void SelectStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void InsertStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void UpdateStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DeleteStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CopyStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void MergeStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExecuteProcedureStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExecuteStatementStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Transaction statements
void StartTransactionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void PrepareTransactionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CommitStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void RollbackStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void SavepointStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ReleaseSavepointStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Session statements
void SetStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterSystemStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ResetStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ShowStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExplainStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AnalyzeStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void SweepDatabaseStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// DCL statements
void GrantStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void RevokeStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Connection statements
void ConnectStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DisconnectStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Metadata statements
void CommentStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// =============================================================================
// PSQL accept() implementations
// =============================================================================

void ExecuteBlockStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CompoundStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DeclareVariableStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AssignmentStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void IfStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void WhileStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ForSelectStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ForExecuteStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LoopStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LeaveStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ContinueStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExitStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void SuspendStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ReturnStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExceptionRaiseStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void WhenExceptionStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void PostEventStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DeclareCursorStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void OpenCursorStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void FetchCursorStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CloseCursorStmt::accept(ASTVisitor& visitor) { visitor.visit(this); }

// =============================================================================
// Expression accept() implementations
// =============================================================================

void LiteralExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralEnumExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralSetExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralRowExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralCompositeExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralDomainExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralBitExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralYearExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralDateTimeExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralMediumIntExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralGeometryExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralJsonPathExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralInt8Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralInt16Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralUInt8Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralUInt16Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralUInt32Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralUInt64Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralUInt128Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralInt128Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralFloat32Expr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralTimeTzExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralTimestampTzExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralRangeExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralArrayExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralVariantExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralTsVectorExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralTsQueryExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LiteralBlobLocatorExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ColumnRefExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ParameterExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void BinaryExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void UnaryExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void FunctionCallExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CastExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExtractExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void AlterElementExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CaseExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void SubqueryExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExistsExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void InExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void BetweenExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void LikeExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void IsNullExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ArrayExpr::accept(ASTVisitor& visitor) { visitor.visit(this); }

// =============================================================================
// ASTArena Implementation
// =============================================================================

ASTArena::ASTArena(size_t block_size)
    : current_block_(nullptr)
    , block_size_(block_size)
    , total_allocated_(0)
{
    current_block_ = allocateBlock(block_size_);
}

ASTArena::~ASTArena() {
    // Call all tracked destructors first (in reverse order)
    callDestructors();

    // Free all blocks
    reset();

    // Free the current block too
    if (current_block_) {
        std::free(current_block_->data);
        delete current_block_;
    }
}

ASTArena::Block* ASTArena::allocateBlock(size_t size) {
    Block* block = new Block;
    block->data = static_cast<char*>(std::malloc(size));
    block->size = size;
    block->used = 0;
    block->next = nullptr;
    return block;
}

void* ASTArena::allocate(size_t size, size_t alignment) {
    // Align the current position
    size_t current = reinterpret_cast<size_t>(current_block_->data + current_block_->used);
    size_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;

    // Check if we need a new block
    if (current_block_->used + padding + size > current_block_->size) {
        // Allocate new block (at least big enough for this allocation)
        size_t new_size = std::max(block_size_, size + alignment);
        Block* new_block = allocateBlock(new_size);
        new_block->next = current_block_;
        current_block_ = new_block;

        // Recalculate alignment for new block
        current = reinterpret_cast<size_t>(current_block_->data);
        aligned = (current + alignment - 1) & ~(alignment - 1);
        padding = aligned - current;
    }

    void* result = current_block_->data + current_block_->used + padding;
    current_block_->used += padding + size;
    total_allocated_ += size;

    return result;
}

void ASTArena::trackDestructor(std::function<void()> dtor) {
    destructors_.push_back(std::move(dtor));
}

void ASTArena::callDestructors() {
    // Call destructors in reverse order (LIFO)
    for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it) {
        (*it)();
    }
    destructors_.clear();
}

void ASTArena::reset() {
    // Call all tracked destructors first
    callDestructors();

    // Free all blocks except the current one
    while (current_block_ && current_block_->next) {
        Block* next = current_block_->next;
        std::free(current_block_->data);
        delete current_block_;
        current_block_ = next;
    }

    // Reset the current block
    if (current_block_) {
        current_block_->used = 0;
    }

    total_allocated_ = 0;
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* astKindToString(ASTKind kind) {
    switch (kind) {
        // DDL
        case ASTKind::CreateTableStmt: return "CreateTableStmt";
        case ASTKind::CreateIndexStmt: return "CreateIndexStmt";
        case ASTKind::CreateViewStmt: return "CreateViewStmt";
        case ASTKind::CreateSequenceStmt: return "CreateSequenceStmt";
        case ASTKind::AlterSequenceStmt: return "AlterSequenceStmt";
        case ASTKind::CreateSchemaStmt: return "CreateSchemaStmt";
        case ASTKind::DropSchemaStmt: return "DropSchemaStmt";
        case ASTKind::AlterSchemaStmt: return "AlterSchemaStmt";
        case ASTKind::CreateDatabaseStmt: return "CreateDatabaseStmt";
        case ASTKind::DropDatabaseStmt: return "DropDatabaseStmt";
        case ASTKind::AlterDatabaseStmt: return "AlterDatabaseStmt";
        case ASTKind::CreateFunctionStmt: return "CreateFunctionStmt";
        case ASTKind::CreateProcedureStmt: return "CreateProcedureStmt";
        case ASTKind::CreateTriggerStmt: return "CreateTriggerStmt";
        case ASTKind::CreatePackageStmt: return "CreatePackageStmt";
        case ASTKind::CreateUserStmt: return "CreateUserStmt";
        case ASTKind::CreateRoleStmt: return "CreateRoleStmt";
        case ASTKind::CreateExceptionStmt: return "CreateExceptionStmt";
        case ASTKind::CreateTypeStmt: return "CreateTypeStmt";
        case ASTKind::CreateDomainStmt: return "CreateDomainStmt";
        case ASTKind::CreateForeignDataWrapperStmt: return "CreateForeignDataWrapperStmt";
        case ASTKind::AlterTypeStmt: return "AlterTypeStmt";
        case ASTKind::DropTypeStmt: return "DropTypeStmt";
        case ASTKind::AlterTableStmt: return "AlterTableStmt";
        case ASTKind::AlterIndexStmt: return "AlterIndexStmt";
        case ASTKind::RenameObjectStmt: return "RenameObjectStmt";
        case ASTKind::MoveObjectStmt: return "MoveObjectStmt";
        case ASTKind::DropTableStmt: return "DropTableStmt";
        case ASTKind::DropIndexStmt: return "DropIndexStmt";
        case ASTKind::DropViewStmt: return "DropViewStmt";
        case ASTKind::DropSequenceStmt: return "DropSequenceStmt";
        case ASTKind::TruncateTableStmt: return "TruncateTableStmt";

        // DML
        case ASTKind::SelectStmt: return "SelectStmt";
        case ASTKind::InsertStmt: return "InsertStmt";
        case ASTKind::UpdateStmt: return "UpdateStmt";
        case ASTKind::DeleteStmt: return "DeleteStmt";
        case ASTKind::CopyStmt: return "CopyStmt";
        case ASTKind::MergeStmt: return "MergeStmt";
        case ASTKind::ExecuteProcedureStmt: return "ExecuteProcedureStmt";
        case ASTKind::ExecuteStatementStmt: return "ExecuteStatementStmt";

        // Transaction
        case ASTKind::StartTransactionStmt: return "StartTransactionStmt";
        case ASTKind::PrepareTransactionStmt: return "PrepareTransactionStmt";
        case ASTKind::CommitStmt: return "CommitStmt";
        case ASTKind::RollbackStmt: return "RollbackStmt";
        case ASTKind::SavepointStmt: return "SavepointStmt";
        case ASTKind::ReleaseSavepointStmt: return "ReleaseSavepointStmt";

        // Session
        case ASTKind::SetStmt: return "SetStmt";
        case ASTKind::AlterSystemStmt: return "AlterSystemStmt";
        case ASTKind::ResetStmt: return "ResetStmt";
        case ASTKind::ShowStmt: return "ShowStmt";
        case ASTKind::ExplainStmt: return "ExplainStmt";

        // DCL
        case ASTKind::GrantStmt: return "GrantStmt";
        case ASTKind::RevokeStmt: return "RevokeStmt";

        // Connection
        case ASTKind::ConnectStmt: return "ConnectStmt";
        case ASTKind::DisconnectStmt: return "DisconnectStmt";

        // Metadata
        case ASTKind::CommentStmt: return "CommentStmt";

        case ASTKind::ExecuteBlockStmt: return "ExecuteBlockStmt";
        case ASTKind::CompoundStmt: return "CompoundStmt";
        case ASTKind::DeclareVariableStmt: return "DeclareVariableStmt";
        case ASTKind::AssignmentStmt: return "AssignmentStmt";
        case ASTKind::IfStmt: return "IfStmt";
        case ASTKind::WhileStmt: return "WhileStmt";
        case ASTKind::ForSelectStmt: return "ForSelectStmt";
        case ASTKind::ForExecuteStmt: return "ForExecuteStmt";
        case ASTKind::LoopStmt: return "LoopStmt";
        case ASTKind::LeaveStmt: return "LeaveStmt";
        case ASTKind::ContinueStmt: return "ContinueStmt";
        case ASTKind::ExitStmt: return "ExitStmt";
        case ASTKind::SuspendStmt: return "SuspendStmt";
        case ASTKind::ReturnStmt: return "ReturnStmt";
        case ASTKind::ExceptionRaiseStmt: return "ExceptionRaiseStmt";
        case ASTKind::WhenExceptionStmt: return "WhenExceptionStmt";
        case ASTKind::PostEventStmt: return "PostEventStmt";
        case ASTKind::DeclareCursorStmt: return "DeclareCursorStmt";
        case ASTKind::OpenCursorStmt: return "OpenCursorStmt";
        case ASTKind::FetchCursorStmt: return "FetchCursorStmt";
        case ASTKind::CloseCursorStmt: return "CloseCursorStmt";

        // Expressions
        case ASTKind::LiteralExpr: return "LiteralExpr";
        case ASTKind::LiteralEnumExpr: return "LiteralEnumExpr";
        case ASTKind::LiteralSetExpr: return "LiteralSetExpr";
        case ASTKind::LiteralRowExpr: return "LiteralRowExpr";
        case ASTKind::LiteralCompositeExpr: return "LiteralCompositeExpr";
        case ASTKind::LiteralDomainExpr: return "LiteralDomainExpr";
        case ASTKind::LiteralBitExpr: return "LiteralBitExpr";
        case ASTKind::LiteralYearExpr: return "LiteralYearExpr";
        case ASTKind::LiteralDateTimeExpr: return "LiteralDateTimeExpr";
        case ASTKind::LiteralMediumIntExpr: return "LiteralMediumIntExpr";
        case ASTKind::LiteralGeometryExpr: return "LiteralGeometryExpr";
        case ASTKind::LiteralJsonPathExpr: return "LiteralJsonPathExpr";
        case ASTKind::LiteralInt8Expr: return "LiteralInt8Expr";
        case ASTKind::LiteralInt16Expr: return "LiteralInt16Expr";
        case ASTKind::LiteralUInt8Expr: return "LiteralUInt8Expr";
        case ASTKind::LiteralUInt16Expr: return "LiteralUInt16Expr";
        case ASTKind::LiteralUInt32Expr: return "LiteralUInt32Expr";
        case ASTKind::LiteralUInt64Expr: return "LiteralUInt64Expr";
        case ASTKind::LiteralUInt128Expr: return "LiteralUInt128Expr";
        case ASTKind::LiteralInt128Expr: return "LiteralInt128Expr";
        case ASTKind::LiteralFloat32Expr: return "LiteralFloat32Expr";
        case ASTKind::LiteralTimeTzExpr: return "LiteralTimeTzExpr";
        case ASTKind::LiteralTimestampTzExpr: return "LiteralTimestampTzExpr";
        case ASTKind::LiteralRangeExpr: return "LiteralRangeExpr";
        case ASTKind::LiteralArrayExpr: return "LiteralArrayExpr";
        case ASTKind::LiteralVariantExpr: return "LiteralVariantExpr";
        case ASTKind::LiteralTsVectorExpr: return "LiteralTsVectorExpr";
        case ASTKind::LiteralTsQueryExpr: return "LiteralTsQueryExpr";
        case ASTKind::LiteralBlobLocatorExpr: return "LiteralBlobLocatorExpr";
        case ASTKind::ColumnRefExpr: return "ColumnRefExpr";
        case ASTKind::BinaryExpr: return "BinaryExpr";
        case ASTKind::UnaryExpr: return "UnaryExpr";
        case ASTKind::FunctionCallExpr: return "FunctionCallExpr";
        case ASTKind::CastExpr: return "CastExpr";
        case ASTKind::CaseExpr: return "CaseExpr";
        case ASTKind::SubqueryExpr: return "SubqueryExpr";
        case ASTKind::ExistsExpr: return "ExistsExpr";
        case ASTKind::InExpr: return "InExpr";
        case ASTKind::BetweenExpr: return "BetweenExpr";
        case ASTKind::LikeExpr: return "LikeExpr";
        case ASTKind::IsNullExpr: return "IsNullExpr";
        case ASTKind::ArrayExpr: return "ArrayExpr";

        // Other
        case ASTKind::ColumnDef: return "ColumnDef";
        case ASTKind::TableConstraint: return "TableConstraint";
        case ASTKind::TypeName: return "TypeName";
        case ASTKind::SelectItem: return "SelectItem";
        case ASTKind::FromClause: return "FromClause";
        case ASTKind::JoinClause: return "JoinClause";
        case ASTKind::WindowSpec: return "WindowSpec";
        case ASTKind::OrderByItem: return "OrderByItem";
        case ASTKind::GroupByClause: return "GroupByClause";
        case ASTKind::AST_DOC_PATH_FILTER: return "AST_DOC_PATH_FILTER";
        case ASTKind::AST_TS_BUCKET_AGG: return "AST_TS_BUCKET_AGG";
        case ASTKind::AST_COL_SCAN_HINT: return "AST_COL_SCAN_HINT";
        case ASTKind::AST_SEARCH_QUERY_DSL: return "AST_SEARCH_QUERY_DSL";
        case ASTKind::AST_VECTOR_ANN_QUERY: return "AST_VECTOR_ANN_QUERY";
        case ASTKind::AST_HYBRID_BRIDGE: return "AST_HYBRID_BRIDGE";
    }
    return "UNKNOWN";
}

const char* binaryOpToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD: return "+";
        case BinaryOp::SUB: return "-";
        case BinaryOp::MUL: return "*";
        case BinaryOp::DIV: return "/";
        case BinaryOp::MOD: return "%";
        case BinaryOp::POWER: return "^";
        case BinaryOp::EQ: return "=";
        case BinaryOp::NE: return "<>";
        case BinaryOp::LT: return "<";
        case BinaryOp::LE: return "<=";
        case BinaryOp::GT: return ">";
        case BinaryOp::GE: return ">=";
        case BinaryOp::NULL_SAFE_EQ: return "<=>";
        case BinaryOp::AND: return "AND";
        case BinaryOp::OR: return "OR";
        case BinaryOp::CONCAT: return "||";
        case BinaryOp::REGEX_MATCH: return "~";
        case BinaryOp::REGEX_MATCH_CI: return "~*";
        case BinaryOp::REGEX_NOT_MATCH: return "!~";
        case BinaryOp::REGEX_NOT_MATCH_CI: return "!~*";
        case BinaryOp::BIT_AND: return "&";
        case BinaryOp::BIT_OR: return "|";
        case BinaryOp::BIT_XOR: return "^";
        case BinaryOp::SHIFT_LEFT: return "<<";
        case BinaryOp::SHIFT_RIGHT: return ">>";
        case BinaryOp::JSON_EXTRACT: return "->";
        case BinaryOp::JSON_EXTRACT_TEXT: return "->>";
        case BinaryOp::JSON_HASH_EXTRACT: return "#>";
        case BinaryOp::JSON_HASH_EXTRACT_TEXT: return "#>>";
        case BinaryOp::JSON_EXISTS: return "?";
        case BinaryOp::JSON_EXISTS_ANY: return "?|";
        case BinaryOp::JSON_EXISTS_ALL: return "?&";
        case BinaryOp::ARRAY_CONTAINS: return "@>";
        case BinaryOp::ARRAY_CONTAINED_BY: return "<@";
        case BinaryOp::ARRAY_OVERLAP: return "&&";
    }
    return "?";
}

const char* unaryOpToString(UnaryOp op) {
    switch (op) {
        case UnaryOp::NEGATE: return "-";
        case UnaryOp::NOT: return "NOT";
        case UnaryOp::BIT_NOT: return "~";
        case UnaryOp::IS_NULL: return "IS NULL";
        case UnaryOp::IS_NOT_NULL: return "IS NOT NULL";
    }
    return "?";
}

} // namespace scratchbird::parser::v3
