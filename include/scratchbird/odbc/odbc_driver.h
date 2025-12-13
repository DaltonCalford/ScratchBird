/**
 * @file odbc_driver.h
 * @brief ScratchBird ODBC Driver Public API
 *
 * This header defines the ODBC API functions exported by the ScratchBird
 * ODBC driver. Applications should include the standard sql.h/sqlext.h
 * headers; this file is for driver implementation reference.
 *
 * Part of Phase 3.8: ODBC Driver
 */

#ifndef SCRATCHBIRD_ODBC_DRIVER_H
#define SCRATCHBIRD_ODBC_DRIVER_H

#include "scratchbird/odbc/odbc_types.h"
#include "scratchbird/odbc/odbc_handles.h"

// Export macros for shared library
#ifdef _WIN32
    #ifdef SCRATCHBIRD_ODBC_EXPORTS
        #define ODBC_EXPORT __declspec(dllexport)
    #else
        #define ODBC_EXPORT __declspec(dllimport)
    #endif
#else
    #define ODBC_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

// =============================================================================
// Handle Allocation and Freeing
// =============================================================================

/**
 * @brief Allocate an ODBC handle
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLAllocHandle(
    scratchbird::odbc::SQLSMALLINT HandleType,
    scratchbird::odbc::SQLHANDLE InputHandle,
    scratchbird::odbc::SQLHANDLE* OutputHandlePtr);

/**
 * @brief Free an ODBC handle
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLFreeHandle(
    scratchbird::odbc::SQLSMALLINT HandleType,
    scratchbird::odbc::SQLHANDLE Handle);

// =============================================================================
// Connection Functions
// =============================================================================

/**
 * @brief Connect using DSN
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLConnect(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLCHAR* ServerName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* UserName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* Authentication,
    scratchbird::odbc::SQLSMALLINT NameLength3);

/**
 * @brief Connect using connection string
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLDriverConnect(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    HWND WindowHandle,
    scratchbird::odbc::SQLCHAR* InConnectionString,
    scratchbird::odbc::SQLSMALLINT StringLength1,
    scratchbird::odbc::SQLCHAR* OutConnectionString,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* StringLength2Ptr,
    scratchbird::odbc::SQLUSMALLINT DriverCompletion);

/**
 * @brief Browse connect (iterative)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLBrowseConnect(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLCHAR* InConnectionString,
    scratchbird::odbc::SQLSMALLINT StringLength1,
    scratchbird::odbc::SQLCHAR* OutConnectionString,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* StringLength2Ptr);

/**
 * @brief Disconnect from server
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLDisconnect(
    scratchbird::odbc::SQLHDBC ConnectionHandle);

// =============================================================================
// Attribute Functions
// =============================================================================

/**
 * @brief Set environment attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSetEnvAttr(
    scratchbird::odbc::SQLHENV EnvironmentHandle,
    scratchbird::odbc::SQLINTEGER Attribute,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER StringLength);

/**
 * @brief Get environment attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetEnvAttr(
    scratchbird::odbc::SQLHENV EnvironmentHandle,
    scratchbird::odbc::SQLINTEGER Attribute,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER BufferLength,
    scratchbird::odbc::SQLINTEGER* StringLengthPtr);

/**
 * @brief Set connection attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSetConnectAttr(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLINTEGER Attribute,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER StringLength);

/**
 * @brief Get connection attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetConnectAttr(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLINTEGER Attribute,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER BufferLength,
    scratchbird::odbc::SQLINTEGER* StringLengthPtr);

/**
 * @brief Set statement attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSetStmtAttr(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLINTEGER Attribute,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER StringLength);

/**
 * @brief Get statement attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetStmtAttr(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLINTEGER Attribute,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER BufferLength,
    scratchbird::odbc::SQLINTEGER* StringLengthPtr);

// =============================================================================
// Information Functions
// =============================================================================

/**
 * @brief Get driver/database information
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetInfo(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLUSMALLINT InfoType,
    scratchbird::odbc::SQLPOINTER InfoValuePtr,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* StringLengthPtr);

/**
 * @brief Get supported functions
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetFunctions(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLUSMALLINT FunctionId,
    scratchbird::odbc::SQLUSMALLINT* SupportedPtr);

/**
 * @brief Get type information
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetTypeInfo(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLSMALLINT DataType);

// =============================================================================
// Statement Execution Functions
// =============================================================================

/**
 * @brief Prepare SQL statement
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLPrepare(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* StatementText,
    scratchbird::odbc::SQLINTEGER TextLength);

/**
 * @brief Execute prepared statement
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLExecute(
    scratchbird::odbc::SQLHSTMT StatementHandle);

/**
 * @brief Execute SQL directly
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLExecDirect(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* StatementText,
    scratchbird::odbc::SQLINTEGER TextLength);

/**
 * @brief Cancel statement execution
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLCancel(
    scratchbird::odbc::SQLHSTMT StatementHandle);

/**
 * @brief Close cursor
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLCloseCursor(
    scratchbird::odbc::SQLHSTMT StatementHandle);

/**
 * @brief Free statement resources
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLFreeStmt(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT Option);

// =============================================================================
// Parameter Binding Functions
// =============================================================================

/**
 * @brief Bind parameter
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLBindParameter(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT ParameterNumber,
    scratchbird::odbc::SQLSMALLINT InputOutputType,
    scratchbird::odbc::SQLSMALLINT ValueType,
    scratchbird::odbc::SQLSMALLINT ParameterType,
    scratchbird::odbc::SQLULEN ColumnSize,
    scratchbird::odbc::SQLSMALLINT DecimalDigits,
    scratchbird::odbc::SQLPOINTER ParameterValuePtr,
    scratchbird::odbc::SQLLEN BufferLength,
    scratchbird::odbc::SQLLEN* StrLen_or_IndPtr);

/**
 * @brief Get number of parameters
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLNumParams(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLSMALLINT* ParameterCountPtr);

/**
 * @brief Describe parameter
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLDescribeParam(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT ParameterNumber,
    scratchbird::odbc::SQLSMALLINT* DataTypePtr,
    scratchbird::odbc::SQLULEN* ParameterSizePtr,
    scratchbird::odbc::SQLSMALLINT* DecimalDigitsPtr,
    scratchbird::odbc::SQLSMALLINT* NullablePtr);

// =============================================================================
// Column Binding and Retrieval Functions
// =============================================================================

/**
 * @brief Bind column for retrieval
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLBindCol(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT ColumnNumber,
    scratchbird::odbc::SQLSMALLINT TargetType,
    scratchbird::odbc::SQLPOINTER TargetValuePtr,
    scratchbird::odbc::SQLLEN BufferLength,
    scratchbird::odbc::SQLLEN* StrLen_or_IndPtr);

/**
 * @brief Get number of result columns
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLNumResultCols(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLSMALLINT* ColumnCountPtr);

/**
 * @brief Describe column
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLDescribeCol(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT ColumnNumber,
    scratchbird::odbc::SQLCHAR* ColumnName,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* NameLengthPtr,
    scratchbird::odbc::SQLSMALLINT* DataTypePtr,
    scratchbird::odbc::SQLULEN* ColumnSizePtr,
    scratchbird::odbc::SQLSMALLINT* DecimalDigitsPtr,
    scratchbird::odbc::SQLSMALLINT* NullablePtr);

/**
 * @brief Get column attribute
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLColAttribute(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT ColumnNumber,
    scratchbird::odbc::SQLUSMALLINT FieldIdentifier,
    scratchbird::odbc::SQLPOINTER CharacterAttributePtr,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* StringLengthPtr,
    scratchbird::odbc::SQLLEN* NumericAttributePtr);

/**
 * @brief Fetch next row
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLFetch(
    scratchbird::odbc::SQLHSTMT StatementHandle);

/**
 * @brief Fetch with scroll
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLFetchScroll(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLSMALLINT FetchOrientation,
    scratchbird::odbc::SQLLEN FetchOffset);

/**
 * @brief Get data for unbound column
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetData(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT ColumnNumber,
    scratchbird::odbc::SQLSMALLINT TargetType,
    scratchbird::odbc::SQLPOINTER TargetValuePtr,
    scratchbird::odbc::SQLLEN BufferLength,
    scratchbird::odbc::SQLLEN* StrLen_or_IndPtr);

/**
 * @brief Get row count
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLRowCount(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLLEN* RowCountPtr);

/**
 * @brief Check for more results
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLMoreResults(
    scratchbird::odbc::SQLHSTMT StatementHandle);

// =============================================================================
// Positioned Operations Functions
// =============================================================================

/**
 * @brief Set cursor position
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSetPos(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLSETPOSIROW RowNumber,
    scratchbird::odbc::SQLUSMALLINT Operation,
    scratchbird::odbc::SQLUSMALLINT LockType);

/**
 * @brief Bulk operations
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLBulkOperations(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLSMALLINT Operation);

// =============================================================================
// Catalog Functions
// =============================================================================

/**
 * @brief Get tables list
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLTables(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLCHAR* TableType,
    scratchbird::odbc::SQLSMALLINT NameLength4);

/**
 * @brief Get columns list
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLColumns(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLCHAR* ColumnName,
    scratchbird::odbc::SQLSMALLINT NameLength4);

/**
 * @brief Get primary keys
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLPrimaryKeys(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3);

/**
 * @brief Get foreign keys
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLForeignKeys(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* PKCatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* PKSchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* PKTableName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLCHAR* FKCatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength4,
    scratchbird::odbc::SQLCHAR* FKSchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength5,
    scratchbird::odbc::SQLCHAR* FKTableName,
    scratchbird::odbc::SQLSMALLINT NameLength6);

/**
 * @brief Get statistics
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLStatistics(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLUSMALLINT Unique,
    scratchbird::odbc::SQLUSMALLINT Reserved);

/**
 * @brief Get special columns
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSpecialColumns(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLUSMALLINT IdentifierType,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLUSMALLINT Scope,
    scratchbird::odbc::SQLUSMALLINT Nullable);

/**
 * @brief Get procedures
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLProcedures(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* ProcName,
    scratchbird::odbc::SQLSMALLINT NameLength3);

/**
 * @brief Get procedure columns
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLProcedureColumns(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* ProcName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLCHAR* ColumnName,
    scratchbird::odbc::SQLSMALLINT NameLength4);

/**
 * @brief Get table privileges
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLTablePrivileges(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3);

/**
 * @brief Get column privileges
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLColumnPrivileges(
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* CatalogName,
    scratchbird::odbc::SQLSMALLINT NameLength1,
    scratchbird::odbc::SQLCHAR* SchemaName,
    scratchbird::odbc::SQLSMALLINT NameLength2,
    scratchbird::odbc::SQLCHAR* TableName,
    scratchbird::odbc::SQLSMALLINT NameLength3,
    scratchbird::odbc::SQLCHAR* ColumnName,
    scratchbird::odbc::SQLSMALLINT NameLength4);

// =============================================================================
// Transaction Functions
// =============================================================================

/**
 * @brief End transaction (commit or rollback)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLEndTran(
    scratchbird::odbc::SQLSMALLINT HandleType,
    scratchbird::odbc::SQLHANDLE Handle,
    scratchbird::odbc::SQLSMALLINT CompletionType);

// =============================================================================
// Diagnostic Functions
// =============================================================================

/**
 * @brief Get diagnostic record
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetDiagRec(
    scratchbird::odbc::SQLSMALLINT HandleType,
    scratchbird::odbc::SQLHANDLE Handle,
    scratchbird::odbc::SQLSMALLINT RecNumber,
    scratchbird::odbc::SQLCHAR* SQLState,
    scratchbird::odbc::SQLINTEGER* NativeErrorPtr,
    scratchbird::odbc::SQLCHAR* MessageText,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* TextLengthPtr);

/**
 * @brief Get diagnostic field
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetDiagField(
    scratchbird::odbc::SQLSMALLINT HandleType,
    scratchbird::odbc::SQLHANDLE Handle,
    scratchbird::odbc::SQLSMALLINT RecNumber,
    scratchbird::odbc::SQLSMALLINT DiagIdentifier,
    scratchbird::odbc::SQLPOINTER DiagInfoPtr,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* StringLengthPtr);

// =============================================================================
// Descriptor Functions
// =============================================================================

/**
 * @brief Set descriptor field
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSetDescField(
    scratchbird::odbc::SQLHDESC DescriptorHandle,
    scratchbird::odbc::SQLSMALLINT RecNumber,
    scratchbird::odbc::SQLSMALLINT FieldIdentifier,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER BufferLength);

/**
 * @brief Get descriptor field
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetDescField(
    scratchbird::odbc::SQLHDESC DescriptorHandle,
    scratchbird::odbc::SQLSMALLINT RecNumber,
    scratchbird::odbc::SQLSMALLINT FieldIdentifier,
    scratchbird::odbc::SQLPOINTER ValuePtr,
    scratchbird::odbc::SQLINTEGER BufferLength,
    scratchbird::odbc::SQLINTEGER* StringLengthPtr);

/**
 * @brief Set descriptor record
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLSetDescRec(
    scratchbird::odbc::SQLHDESC DescriptorHandle,
    scratchbird::odbc::SQLSMALLINT RecNumber,
    scratchbird::odbc::SQLSMALLINT Type,
    scratchbird::odbc::SQLSMALLINT SubType,
    scratchbird::odbc::SQLLEN Length,
    scratchbird::odbc::SQLSMALLINT Precision,
    scratchbird::odbc::SQLSMALLINT Scale,
    scratchbird::odbc::SQLPOINTER DataPtr,
    scratchbird::odbc::SQLLEN* StringLengthPtr,
    scratchbird::odbc::SQLLEN* IndicatorPtr);

/**
 * @brief Get descriptor record
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLGetDescRec(
    scratchbird::odbc::SQLHDESC DescriptorHandle,
    scratchbird::odbc::SQLSMALLINT RecNumber,
    scratchbird::odbc::SQLCHAR* Name,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* StringLengthPtr,
    scratchbird::odbc::SQLSMALLINT* TypePtr,
    scratchbird::odbc::SQLSMALLINT* SubTypePtr,
    scratchbird::odbc::SQLLEN* LengthPtr,
    scratchbird::odbc::SQLSMALLINT* PrecisionPtr,
    scratchbird::odbc::SQLSMALLINT* ScalePtr,
    scratchbird::odbc::SQLSMALLINT* NullablePtr);

/**
 * @brief Copy descriptor
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLCopyDesc(
    scratchbird::odbc::SQLHDESC SourceDescHandle,
    scratchbird::odbc::SQLHDESC TargetDescHandle);

// =============================================================================
// ODBC 2.x Compatibility Functions (deprecated but commonly used)
// =============================================================================

/**
 * @brief Get error (ODBC 2.x)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLError(
    scratchbird::odbc::SQLHENV EnvironmentHandle,
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLHSTMT StatementHandle,
    scratchbird::odbc::SQLCHAR* SQLState,
    scratchbird::odbc::SQLINTEGER* NativeErrorPtr,
    scratchbird::odbc::SQLCHAR* MessageText,
    scratchbird::odbc::SQLSMALLINT BufferLength,
    scratchbird::odbc::SQLSMALLINT* TextLengthPtr);

/**
 * @brief Allocate environment (ODBC 2.x)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLAllocEnv(
    scratchbird::odbc::SQLHENV* EnvironmentHandlePtr);

/**
 * @brief Allocate connection (ODBC 2.x)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLAllocConnect(
    scratchbird::odbc::SQLHENV EnvironmentHandle,
    scratchbird::odbc::SQLHDBC* ConnectionHandlePtr);

/**
 * @brief Allocate statement (ODBC 2.x)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLAllocStmt(
    scratchbird::odbc::SQLHDBC ConnectionHandle,
    scratchbird::odbc::SQLHSTMT* StatementHandlePtr);

/**
 * @brief Free environment (ODBC 2.x)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLFreeEnv(
    scratchbird::odbc::SQLHENV EnvironmentHandle);

/**
 * @brief Free connection (ODBC 2.x)
 */
ODBC_EXPORT scratchbird::odbc::SQLRETURN ODBC_API SQLFreeConnect(
    scratchbird::odbc::SQLHDBC ConnectionHandle);

}  // extern "C"

#endif  // SCRATCHBIRD_ODBC_DRIVER_H
