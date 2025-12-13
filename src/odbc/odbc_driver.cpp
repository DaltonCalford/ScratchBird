/**
 * @file odbc_driver.cpp
 * @brief ScratchBird ODBC Driver API Implementation
 *
 * Implements the standard ODBC API functions exported by the driver.
 *
 * Part of Phase 3.8: ODBC Driver
 */

#include "scratchbird/odbc/odbc_driver.h"

#include <cstring>

using namespace scratchbird::odbc;

// =============================================================================
// Handle Allocation and Freeing
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLAllocHandle(
    SQLSMALLINT HandleType,
    SQLHANDLE InputHandle,
    SQLHANDLE* OutputHandlePtr) {

    if (!OutputHandlePtr) {
        return SQL_ERROR;
    }

    switch (HandleType) {
        case SQL_HANDLE_ENV: {
            auto* env = new OdbcEnvironment();
            *OutputHandlePtr = static_cast<SQLHANDLE>(env);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DBC: {
            auto* env = asEnvironment(InputHandle);
            if (!env) {
                return SQL_INVALID_HANDLE;
            }
            auto* conn = env->createConnection();
            *OutputHandlePtr = static_cast<SQLHANDLE>(conn);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_STMT: {
            auto* conn = asConnection(InputHandle);
            if (!conn) {
                return SQL_INVALID_HANDLE;
            }
            auto* stmt = conn->createStatement();
            *OutputHandlePtr = static_cast<SQLHANDLE>(stmt);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DESC: {
            auto* conn = asConnection(InputHandle);
            if (!conn) {
                return SQL_INVALID_HANDLE;
            }
            auto* desc = new OdbcDescriptor(conn, OdbcDescriptor::DescriptorType::APD, false);
            *OutputHandlePtr = static_cast<SQLHANDLE>(desc);
            return SQL_SUCCESS;
        }

        default:
            return SQL_ERROR;
    }
}

extern "C" SQLRETURN ODBC_API SQLFreeHandle(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle) {

    if (!Handle) {
        return SQL_INVALID_HANDLE;
    }

    switch (HandleType) {
        case SQL_HANDLE_ENV: {
            auto* env = asEnvironment(Handle);
            if (!env) return SQL_INVALID_HANDLE;
            if (env->getConnectionCount() > 0) {
                env->setError("HY010", 0, "Function sequence error");
                return SQL_ERROR;
            }
            delete env;
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DBC: {
            auto* conn = asConnection(Handle);
            if (!conn) return SQL_INVALID_HANDLE;
            if (conn->isConnected()) {
                conn->disconnect();
            }
            conn->getEnvironment()->removeConnection(conn);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_STMT: {
            auto* stmt = asStatement(Handle);
            if (!stmt) return SQL_INVALID_HANDLE;
            stmt->getConnection()->removeStatement(stmt);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DESC: {
            auto* desc = asDescriptor(Handle);
            if (!desc) return SQL_INVALID_HANDLE;
            if (!desc->isImplicit()) {
                delete desc;
            }
            return SQL_SUCCESS;
        }

        default:
            return SQL_ERROR;
    }
}

// =============================================================================
// Connection Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLConnect(
    SQLHDBC ConnectionHandle,
    SQLCHAR* ServerName,
    SQLSMALLINT NameLength1,
    SQLCHAR* UserName,
    SQLSMALLINT NameLength2,
    SQLCHAR* Authentication,
    SQLSMALLINT NameLength3) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->connect(ServerName, NameLength1, UserName, NameLength2,
                         Authentication, NameLength3);
}

extern "C" SQLRETURN ODBC_API SQLDriverConnect(
    SQLHDBC ConnectionHandle,
    HWND WindowHandle,
    SQLCHAR* InConnectionString,
    SQLSMALLINT StringLength1,
    SQLCHAR* OutConnectionString,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLength2Ptr,
    SQLUSMALLINT DriverCompletion) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->driverConnect(WindowHandle, InConnectionString, StringLength1,
                               OutConnectionString, BufferLength, StringLength2Ptr,
                               DriverCompletion);
}

extern "C" SQLRETURN ODBC_API SQLBrowseConnect(
    SQLHDBC ConnectionHandle,
    SQLCHAR* InConnectionString,
    SQLSMALLINT StringLength1,
    SQLCHAR* OutConnectionString,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLength2Ptr) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->browseConnect(InConnectionString, StringLength1,
                               OutConnectionString, BufferLength, StringLength2Ptr);
}

extern "C" SQLRETURN ODBC_API SQLDisconnect(
    SQLHDBC ConnectionHandle) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->disconnect();
}

// =============================================================================
// Attribute Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLSetEnvAttr(
    SQLHENV EnvironmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength) {

    // Special case: SQL_ATTR_CONNECTION_POOLING can be set before env allocation
    if (EnvironmentHandle == SQL_NULL_HENV) {
        if (Attribute == SQL_ATTR_CONNECTION_POOLING) {
            // Global pooling setting - not implemented
            return SQL_SUCCESS;
        }
        return SQL_INVALID_HANDLE;
    }

    auto* env = asEnvironment(EnvironmentHandle);
    if (!env) return SQL_INVALID_HANDLE;

    return env->setAttribute(Attribute, ValuePtr, StringLength);
}

extern "C" SQLRETURN ODBC_API SQLGetEnvAttr(
    SQLHENV EnvironmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    SQLINTEGER* StringLengthPtr) {

    auto* env = asEnvironment(EnvironmentHandle);
    if (!env) return SQL_INVALID_HANDLE;

    return env->getAttribute(Attribute, ValuePtr, BufferLength, StringLengthPtr);
}

extern "C" SQLRETURN ODBC_API SQLSetConnectAttr(
    SQLHDBC ConnectionHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->setAttribute(Attribute, ValuePtr, StringLength);
}

extern "C" SQLRETURN ODBC_API SQLGetConnectAttr(
    SQLHDBC ConnectionHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    SQLINTEGER* StringLengthPtr) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->getAttribute(Attribute, ValuePtr, BufferLength, StringLengthPtr);
}

extern "C" SQLRETURN ODBC_API SQLSetStmtAttr(
    SQLHSTMT StatementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER StringLength) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->setAttribute(Attribute, ValuePtr, StringLength);
}

extern "C" SQLRETURN ODBC_API SQLGetStmtAttr(
    SQLHSTMT StatementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    SQLINTEGER* StringLengthPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->getAttribute(Attribute, ValuePtr, BufferLength, StringLengthPtr);
}

// =============================================================================
// Information Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLGetInfo(
    SQLHDBC ConnectionHandle,
    SQLUSMALLINT InfoType,
    SQLPOINTER InfoValuePtr,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLengthPtr) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->getInfo(InfoType, InfoValuePtr, BufferLength, StringLengthPtr);
}

extern "C" SQLRETURN ODBC_API SQLGetFunctions(
    SQLHDBC ConnectionHandle,
    SQLUSMALLINT FunctionId,
    SQLUSMALLINT* SupportedPtr) {

    auto* conn = asConnection(ConnectionHandle);
    if (!conn) return SQL_INVALID_HANDLE;

    return conn->getFunctions(FunctionId, SupportedPtr);
}

extern "C" SQLRETURN ODBC_API SQLGetTypeInfo(
    SQLHSTMT StatementHandle,
    SQLSMALLINT DataType) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->getConnection()->getTypeInfo(DataType, stmt);
}

// =============================================================================
// Statement Execution Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLPrepare(
    SQLHSTMT StatementHandle,
    SQLCHAR* StatementText,
    SQLINTEGER TextLength) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->prepare(StatementText, TextLength);
}

extern "C" SQLRETURN ODBC_API SQLExecute(
    SQLHSTMT StatementHandle) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->execute();
}

extern "C" SQLRETURN ODBC_API SQLExecDirect(
    SQLHSTMT StatementHandle,
    SQLCHAR* StatementText,
    SQLINTEGER TextLength) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->execDirect(StatementText, TextLength);
}

extern "C" SQLRETURN ODBC_API SQLCancel(
    SQLHSTMT StatementHandle) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->cancel();
}

extern "C" SQLRETURN ODBC_API SQLCloseCursor(
    SQLHSTMT StatementHandle) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->closeCursor();
}

extern "C" SQLRETURN ODBC_API SQLFreeStmt(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT Option) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    if (Option == SQL_DROP) {
        stmt->getConnection()->removeStatement(stmt);
        return SQL_SUCCESS;
    }

    return stmt->freeStmt(Option);
}

// =============================================================================
// Parameter Binding Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLBindParameter(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT ParameterNumber,
    SQLSMALLINT InputOutputType,
    SQLSMALLINT ValueType,
    SQLSMALLINT ParameterType,
    SQLULEN ColumnSize,
    SQLSMALLINT DecimalDigits,
    SQLPOINTER ParameterValuePtr,
    SQLLEN BufferLength,
    SQLLEN* StrLen_or_IndPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->bindParameter(ParameterNumber, InputOutputType, ValueType,
                               ParameterType, ColumnSize, DecimalDigits,
                               ParameterValuePtr, BufferLength, StrLen_or_IndPtr);
}

extern "C" SQLRETURN ODBC_API SQLNumParams(
    SQLHSTMT StatementHandle,
    SQLSMALLINT* ParameterCountPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->numParams(ParameterCountPtr);
}

extern "C" SQLRETURN ODBC_API SQLDescribeParam(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT ParameterNumber,
    SQLSMALLINT* DataTypePtr,
    SQLULEN* ParameterSizePtr,
    SQLSMALLINT* DecimalDigitsPtr,
    SQLSMALLINT* NullablePtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->describeParam(ParameterNumber, DataTypePtr, ParameterSizePtr,
                               DecimalDigitsPtr, NullablePtr);
}

// =============================================================================
// Column Binding and Retrieval Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLBindCol(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLSMALLINT TargetType,
    SQLPOINTER TargetValuePtr,
    SQLLEN BufferLength,
    SQLLEN* StrLen_or_IndPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->bindCol(ColumnNumber, TargetType, TargetValuePtr,
                         BufferLength, StrLen_or_IndPtr);
}

extern "C" SQLRETURN ODBC_API SQLNumResultCols(
    SQLHSTMT StatementHandle,
    SQLSMALLINT* ColumnCountPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->numResultCols(ColumnCountPtr);
}

extern "C" SQLRETURN ODBC_API SQLDescribeCol(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLCHAR* ColumnName,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* NameLengthPtr,
    SQLSMALLINT* DataTypePtr,
    SQLULEN* ColumnSizePtr,
    SQLSMALLINT* DecimalDigitsPtr,
    SQLSMALLINT* NullablePtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->describeCol(ColumnNumber, ColumnName, BufferLength, NameLengthPtr,
                             DataTypePtr, ColumnSizePtr, DecimalDigitsPtr, NullablePtr);
}

extern "C" SQLRETURN ODBC_API SQLColAttribute(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLUSMALLINT FieldIdentifier,
    SQLPOINTER CharacterAttributePtr,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLengthPtr,
    SQLLEN* NumericAttributePtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->colAttribute(ColumnNumber, FieldIdentifier, CharacterAttributePtr,
                              BufferLength, StringLengthPtr, NumericAttributePtr);
}

extern "C" SQLRETURN ODBC_API SQLFetch(
    SQLHSTMT StatementHandle) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->fetch();
}

extern "C" SQLRETURN ODBC_API SQLFetchScroll(
    SQLHSTMT StatementHandle,
    SQLSMALLINT FetchOrientation,
    SQLLEN FetchOffset) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->fetchScroll(FetchOrientation, FetchOffset);
}

extern "C" SQLRETURN ODBC_API SQLGetData(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLSMALLINT TargetType,
    SQLPOINTER TargetValuePtr,
    SQLLEN BufferLength,
    SQLLEN* StrLen_or_IndPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->getData(ColumnNumber, TargetType, TargetValuePtr,
                         BufferLength, StrLen_or_IndPtr);
}

extern "C" SQLRETURN ODBC_API SQLRowCount(
    SQLHSTMT StatementHandle,
    SQLLEN* RowCountPtr) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->rowCount(RowCountPtr);
}

extern "C" SQLRETURN ODBC_API SQLMoreResults(
    SQLHSTMT StatementHandle) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->moreResults();
}

// =============================================================================
// Positioned Operations Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLSetPos(
    SQLHSTMT StatementHandle,
    SQLSETPOSIROW RowNumber,
    SQLUSMALLINT Operation,
    SQLUSMALLINT LockType) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->setPos(RowNumber, Operation, LockType);
}

extern "C" SQLRETURN ODBC_API SQLBulkOperations(
    SQLHSTMT StatementHandle,
    SQLSMALLINT Operation) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->bulkOperations(Operation);
}

// =============================================================================
// Catalog Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLTables(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3,
    SQLCHAR* TableType,
    SQLSMALLINT NameLength4) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->tables(CatalogName, NameLength1, SchemaName, NameLength2,
                        TableName, NameLength3, TableType, NameLength4);
}

extern "C" SQLRETURN ODBC_API SQLColumns(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3,
    SQLCHAR* ColumnName,
    SQLSMALLINT NameLength4) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->columns(CatalogName, NameLength1, SchemaName, NameLength2,
                         TableName, NameLength3, ColumnName, NameLength4);
}

extern "C" SQLRETURN ODBC_API SQLPrimaryKeys(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->primaryKeys(CatalogName, NameLength1, SchemaName, NameLength2,
                             TableName, NameLength3);
}

extern "C" SQLRETURN ODBC_API SQLForeignKeys(
    SQLHSTMT StatementHandle,
    SQLCHAR* PKCatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* PKSchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* PKTableName,
    SQLSMALLINT NameLength3,
    SQLCHAR* FKCatalogName,
    SQLSMALLINT NameLength4,
    SQLCHAR* FKSchemaName,
    SQLSMALLINT NameLength5,
    SQLCHAR* FKTableName,
    SQLSMALLINT NameLength6) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->foreignKeys(PKCatalogName, NameLength1, PKSchemaName, NameLength2,
                             PKTableName, NameLength3, FKCatalogName, NameLength4,
                             FKSchemaName, NameLength5, FKTableName, NameLength6);
}

extern "C" SQLRETURN ODBC_API SQLStatistics(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3,
    SQLUSMALLINT Unique,
    SQLUSMALLINT Reserved) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->statistics(CatalogName, NameLength1, SchemaName, NameLength2,
                            TableName, NameLength3, Unique, Reserved);
}

extern "C" SQLRETURN ODBC_API SQLSpecialColumns(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT IdentifierType,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3,
    SQLUSMALLINT Scope,
    SQLUSMALLINT Nullable) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->specialColumns(IdentifierType, CatalogName, NameLength1,
                                SchemaName, NameLength2, TableName, NameLength3,
                                Scope, Nullable);
}

extern "C" SQLRETURN ODBC_API SQLProcedures(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* ProcName,
    SQLSMALLINT NameLength3) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->procedures(CatalogName, NameLength1, SchemaName, NameLength2,
                            ProcName, NameLength3);
}

extern "C" SQLRETURN ODBC_API SQLProcedureColumns(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* ProcName,
    SQLSMALLINT NameLength3,
    SQLCHAR* ColumnName,
    SQLSMALLINT NameLength4) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->procedureColumns(CatalogName, NameLength1, SchemaName, NameLength2,
                                  ProcName, NameLength3, ColumnName, NameLength4);
}

extern "C" SQLRETURN ODBC_API SQLTablePrivileges(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->tablePrivileges(CatalogName, NameLength1, SchemaName, NameLength2,
                                 TableName, NameLength3);
}

extern "C" SQLRETURN ODBC_API SQLColumnPrivileges(
    SQLHSTMT StatementHandle,
    SQLCHAR* CatalogName,
    SQLSMALLINT NameLength1,
    SQLCHAR* SchemaName,
    SQLSMALLINT NameLength2,
    SQLCHAR* TableName,
    SQLSMALLINT NameLength3,
    SQLCHAR* ColumnName,
    SQLSMALLINT NameLength4) {

    auto* stmt = asStatement(StatementHandle);
    if (!stmt) return SQL_INVALID_HANDLE;

    return stmt->columnPrivileges(CatalogName, NameLength1, SchemaName, NameLength2,
                                  TableName, NameLength3, ColumnName, NameLength4);
}

// =============================================================================
// Transaction Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLEndTran(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle,
    SQLSMALLINT CompletionType) {

    if (HandleType == SQL_HANDLE_ENV) {
        auto* env = asEnvironment(Handle);
        if (!env) return SQL_INVALID_HANDLE;

        // Commit/rollback all connections
        // For now, just return success
        return SQL_SUCCESS;
    } else if (HandleType == SQL_HANDLE_DBC) {
        auto* conn = asConnection(Handle);
        if (!conn) return SQL_INVALID_HANDLE;

        return conn->endTransaction(CompletionType);
    }

    return SQL_INVALID_HANDLE;
}

// =============================================================================
// Diagnostic Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLGetDiagRec(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle,
    SQLSMALLINT RecNumber,
    SQLCHAR* SQLState,
    SQLINTEGER* NativeErrorPtr,
    SQLCHAR* MessageText,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* TextLengthPtr) {

    if (!Handle) {
        return SQL_INVALID_HANDLE;
    }

    OdbcHandle* handle = nullptr;
    switch (HandleType) {
        case SQL_HANDLE_ENV:
            handle = asEnvironment(Handle);
            break;
        case SQL_HANDLE_DBC:
            handle = asConnection(Handle);
            break;
        case SQL_HANDLE_STMT:
            handle = asStatement(Handle);
            break;
        case SQL_HANDLE_DESC:
            handle = asDescriptor(Handle);
            break;
        default:
            return SQL_INVALID_HANDLE;
    }

    if (!handle) return SQL_INVALID_HANDLE;

    const DiagnosticRecord* rec = handle->getDiagnostic(RecNumber);
    if (!rec) {
        return SQL_NO_DATA;
    }

    // Copy SQLSTATE
    if (SQLState) {
        std::memcpy(SQLState, rec->sqlstate.c_str(), 5);
        SQLState[5] = '\0';
    }

    // Copy native error
    if (NativeErrorPtr) {
        *NativeErrorPtr = rec->native_error;
    }

    // Copy message
    SQLRETURN result = SQL_SUCCESS;
    if (TextLengthPtr) {
        *TextLengthPtr = static_cast<SQLSMALLINT>(rec->message.size());
    }
    if (MessageText && BufferLength > 0) {
        size_t copy_len = std::min(static_cast<size_t>(BufferLength - 1), rec->message.size());
        std::memcpy(MessageText, rec->message.c_str(), copy_len);
        MessageText[copy_len] = '\0';
        if (rec->message.size() >= static_cast<size_t>(BufferLength)) {
            result = SQL_SUCCESS_WITH_INFO;
        }
    }

    return result;
}

extern "C" SQLRETURN ODBC_API SQLGetDiagField(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT DiagIdentifier,
    SQLPOINTER DiagInfoPtr,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLengthPtr) {

    if (!Handle) {
        return SQL_INVALID_HANDLE;
    }

    OdbcHandle* handle = nullptr;
    switch (HandleType) {
        case SQL_HANDLE_ENV:
            handle = asEnvironment(Handle);
            break;
        case SQL_HANDLE_DBC:
            handle = asConnection(Handle);
            break;
        case SQL_HANDLE_STMT:
            handle = asStatement(Handle);
            break;
        case SQL_HANDLE_DESC:
            handle = asDescriptor(Handle);
            break;
        default:
            return SQL_INVALID_HANDLE;
    }

    if (!handle) return SQL_INVALID_HANDLE;

    // Header fields (RecNumber == 0)
    if (RecNumber == 0) {
        switch (DiagIdentifier) {
            case SQL_DIAG_NUMBER:
                if (DiagInfoPtr) {
                    *static_cast<SQLINTEGER*>(DiagInfoPtr) = handle->getDiagnosticCount();
                }
                break;
            case SQL_DIAG_RETURNCODE:
                if (DiagInfoPtr) {
                    *static_cast<SQLRETURN*>(DiagInfoPtr) = handle->getReturnCode();
                }
                break;
            case SQL_DIAG_ROW_COUNT:
                if (HandleType == SQL_HANDLE_STMT) {
                    auto* stmt = asStatement(Handle);
                    if (DiagInfoPtr) {
                        *static_cast<SQLLEN*>(DiagInfoPtr) = stmt ? stmt->getRowCount() : 0;
                    }
                }
                break;
            default:
                return SQL_ERROR;
        }
        return SQL_SUCCESS;
    }

    // Record fields
    const DiagnosticRecord* rec = handle->getDiagnostic(RecNumber);
    if (!rec) {
        return SQL_NO_DATA;
    }

    auto copyString = [&](const std::string& str) -> SQLRETURN {
        if (StringLengthPtr) {
            *StringLengthPtr = static_cast<SQLSMALLINT>(str.size());
        }
        if (DiagInfoPtr && BufferLength > 0) {
            size_t copy_len = std::min(static_cast<size_t>(BufferLength - 1), str.size());
            std::memcpy(DiagInfoPtr, str.c_str(), copy_len);
            static_cast<char*>(DiagInfoPtr)[copy_len] = '\0';
            if (str.size() >= static_cast<size_t>(BufferLength)) {
                return SQL_SUCCESS_WITH_INFO;
            }
        }
        return SQL_SUCCESS;
    };

    switch (DiagIdentifier) {
        case SQL_DIAG_SQLSTATE:
            return copyString(rec->sqlstate);
        case SQL_DIAG_NATIVE:
            if (DiagInfoPtr) {
                *static_cast<SQLINTEGER*>(DiagInfoPtr) = rec->native_error;
            }
            break;
        case SQL_DIAG_MESSAGE_TEXT:
            return copyString(rec->message);
        case SQL_DIAG_CLASS_ORIGIN:
            return copyString(rec->class_origin);
        case SQL_DIAG_SUBCLASS_ORIGIN:
            return copyString(rec->subclass_origin);
        case SQL_DIAG_CONNECTION_NAME:
            return copyString(rec->connection_name);
        case SQL_DIAG_SERVER_NAME:
            return copyString(rec->server_name);
        case SQL_DIAG_ROW_NUMBER:
            if (DiagInfoPtr) {
                *static_cast<SQLINTEGER*>(DiagInfoPtr) = rec->row_number;
            }
            break;
        case SQL_DIAG_COLUMN_NUMBER:
            if (DiagInfoPtr) {
                *static_cast<SQLINTEGER*>(DiagInfoPtr) = rec->column_number;
            }
            break;
        default:
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

// =============================================================================
// Descriptor Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLSetDescField(
    SQLHDESC DescriptorHandle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT FieldIdentifier,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength) {

    auto* desc = asDescriptor(DescriptorHandle);
    if (!desc) return SQL_INVALID_HANDLE;

    return desc->setField(RecNumber, FieldIdentifier, ValuePtr, BufferLength);
}

extern "C" SQLRETURN ODBC_API SQLGetDescField(
    SQLHDESC DescriptorHandle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT FieldIdentifier,
    SQLPOINTER ValuePtr,
    SQLINTEGER BufferLength,
    SQLINTEGER* StringLengthPtr) {

    auto* desc = asDescriptor(DescriptorHandle);
    if (!desc) return SQL_INVALID_HANDLE;

    return desc->getField(RecNumber, FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);
}

extern "C" SQLRETURN ODBC_API SQLSetDescRec(
    SQLHDESC DescriptorHandle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT Type,
    SQLSMALLINT SubType,
    SQLLEN Length,
    SQLSMALLINT Precision,
    SQLSMALLINT Scale,
    SQLPOINTER DataPtr,
    SQLLEN* StringLengthPtr,
    SQLLEN* IndicatorPtr) {

    auto* desc = asDescriptor(DescriptorHandle);
    if (!desc) return SQL_INVALID_HANDLE;

    return desc->setRec(RecNumber, Type, SubType, Length, Precision, Scale,
                        DataPtr, StringLengthPtr, IndicatorPtr);
}

extern "C" SQLRETURN ODBC_API SQLGetDescRec(
    SQLHDESC DescriptorHandle,
    SQLSMALLINT RecNumber,
    SQLCHAR* Name,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLengthPtr,
    SQLSMALLINT* TypePtr,
    SQLSMALLINT* SubTypePtr,
    SQLLEN* LengthPtr,
    SQLSMALLINT* PrecisionPtr,
    SQLSMALLINT* ScalePtr,
    SQLSMALLINT* NullablePtr) {

    auto* desc = asDescriptor(DescriptorHandle);
    if (!desc) return SQL_INVALID_HANDLE;

    return desc->getRec(RecNumber, Name, BufferLength, StringLengthPtr, TypePtr,
                        SubTypePtr, LengthPtr, PrecisionPtr, ScalePtr, NullablePtr);
}

extern "C" SQLRETURN ODBC_API SQLCopyDesc(
    SQLHDESC SourceDescHandle,
    SQLHDESC TargetDescHandle) {

    auto* source = asDescriptor(SourceDescHandle);
    auto* target = asDescriptor(TargetDescHandle);

    if (!source || !target) return SQL_INVALID_HANDLE;

    return source->copyDesc(target);
}

// =============================================================================
// ODBC 2.x Compatibility Functions
// =============================================================================

extern "C" SQLRETURN ODBC_API SQLError(
    SQLHENV EnvironmentHandle,
    SQLHDBC ConnectionHandle,
    SQLHSTMT StatementHandle,
    SQLCHAR* SQLState,
    SQLINTEGER* NativeErrorPtr,
    SQLCHAR* MessageText,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* TextLengthPtr) {

    // Use the most specific handle provided
    SQLSMALLINT handle_type;
    SQLHANDLE handle;

    if (StatementHandle) {
        handle_type = SQL_HANDLE_STMT;
        handle = StatementHandle;
    } else if (ConnectionHandle) {
        handle_type = SQL_HANDLE_DBC;
        handle = ConnectionHandle;
    } else if (EnvironmentHandle) {
        handle_type = SQL_HANDLE_ENV;
        handle = EnvironmentHandle;
    } else {
        return SQL_INVALID_HANDLE;
    }

    // SQLError retrieves records one at a time and removes them
    static int rec_number = 1;  // Not thread-safe, but this is deprecated anyway
    return SQLGetDiagRec(handle_type, handle, rec_number++, SQLState,
                         NativeErrorPtr, MessageText, BufferLength, TextLengthPtr);
}

extern "C" SQLRETURN ODBC_API SQLAllocEnv(
    SQLHENV* EnvironmentHandlePtr) {

    return SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, EnvironmentHandlePtr);
}

extern "C" SQLRETURN ODBC_API SQLAllocConnect(
    SQLHENV EnvironmentHandle,
    SQLHDBC* ConnectionHandlePtr) {

    return SQLAllocHandle(SQL_HANDLE_DBC, EnvironmentHandle, ConnectionHandlePtr);
}

extern "C" SQLRETURN ODBC_API SQLAllocStmt(
    SQLHDBC ConnectionHandle,
    SQLHSTMT* StatementHandlePtr) {

    return SQLAllocHandle(SQL_HANDLE_STMT, ConnectionHandle, StatementHandlePtr);
}

extern "C" SQLRETURN ODBC_API SQLFreeEnv(
    SQLHENV EnvironmentHandle) {

    return SQLFreeHandle(SQL_HANDLE_ENV, EnvironmentHandle);
}

extern "C" SQLRETURN ODBC_API SQLFreeConnect(
    SQLHDBC ConnectionHandle) {

    return SQLFreeHandle(SQL_HANDLE_DBC, ConnectionHandle);
}
