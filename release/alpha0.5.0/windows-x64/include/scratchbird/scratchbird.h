#ifndef SCRATCHBIRD_H
#define SCRATCHBIRD_H

/*
 * ScratchBird v0.5.0 - Main API Header (Windows)
 * 
 * This is the primary header file for ScratchBird client applications on Windows.
 * Include this file to access the ScratchBird database API.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Version Information */
#define SCRATCHBIRD_VERSION_MAJOR    0
#define SCRATCHBIRD_VERSION_MINOR    5
#define SCRATCHBIRD_VERSION_PATCH    0
#define SCRATCHBIRD_VERSION_BUILD    1

#define SCRATCHBIRD_VERSION_STRING   "ScratchBird v0.5.0"

/* Windows DLL Import/Export */
#ifdef _WIN32
    #ifdef SCRATCHBIRD_EXPORTS
        #define SCRATCHBIRD_API __declspec(dllexport)
    #else
        #define SCRATCHBIRD_API __declspec(dllimport)
    #endif
#else
    #define SCRATCHBIRD_API
#endif

/* Basic Types */
typedef void* SB_DATABASE;
typedef void* SB_TRANSACTION;
typedef void* SB_STATEMENT;
typedef void* SB_BLOB;

/* Return Codes */
#define SB_SUCCESS           0
#define SB_ERROR            -1
#define SB_NOT_IMPLEMENTED  -2

/* Basic API Functions */
SCRATCHBIRD_API const char* sb_get_version(void);
SCRATCHBIRD_API int sb_initialize(void);
SCRATCHBIRD_API void sb_shutdown(void);

/* Connection Management */
SCRATCHBIRD_API SB_DATABASE sb_connect(const char* database, const char* user, const char* password);
SCRATCHBIRD_API void sb_disconnect(SB_DATABASE db);

/* Transaction Management */
SCRATCHBIRD_API SB_TRANSACTION sb_transaction_start(SB_DATABASE db);
SCRATCHBIRD_API int sb_transaction_commit(SB_TRANSACTION trans);
SCRATCHBIRD_API int sb_transaction_rollback(SB_TRANSACTION trans);

/* Statement Execution */
SCRATCHBIRD_API SB_STATEMENT sb_statement_prepare(SB_DATABASE db, const char* sql);
SCRATCHBIRD_API int sb_statement_execute(SB_STATEMENT stmt);
SCRATCHBIRD_API void sb_statement_free(SB_STATEMENT stmt);

/* Hierarchical Schema Support (New in v0.5.0) */
SCRATCHBIRD_API int sb_schema_create(SB_DATABASE db, const char* schema_path);
SCRATCHBIRD_API int sb_schema_drop(SB_DATABASE db, const char* schema_path);
SCRATCHBIRD_API int sb_schema_exists(SB_DATABASE db, const char* schema_path);

/* Schema-Aware Database Links (New in v0.5.0) */
typedef enum {
    SB_SCHEMA_MODE_NONE = 0,
    SB_SCHEMA_MODE_FIXED = 1,
    SB_SCHEMA_MODE_CONTEXT_AWARE = 2,
    SB_SCHEMA_MODE_HIERARCHICAL = 3,
    SB_SCHEMA_MODE_MIRROR = 4
} SB_SCHEMA_MODE;

SCRATCHBIRD_API int sb_database_link_create(SB_DATABASE db, const char* link_name, 
                                           const char* target_db, SB_SCHEMA_MODE mode);

/* Two-Phase Commit Support (New in v0.5.0) */
SCRATCHBIRD_API int sb_transaction_prepare(SB_TRANSACTION trans);
SCRATCHBIRD_API int sb_transaction_commit_2pc(SB_TRANSACTION trans);
SCRATCHBIRD_API int sb_transaction_rollback_2pc(SB_TRANSACTION trans);

/* Windows-specific functions */
#ifdef _WIN32
SCRATCHBIRD_API int sb_set_codepage(int codepage);
SCRATCHBIRD_API int sb_get_windows_error(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SCRATCHBIRD_H */