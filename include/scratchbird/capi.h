#ifndef SCRATCHBIRD_CAPI_H
#define SCRATCHBIRD_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum SB_StatusCode {
    SB_STATUS_OK = 0,
    SB_STATUS_NOT_IMPLEMENTED = 1,
    SB_STATUS_ERROR = 2
} SB_StatusCode;

typedef struct SB_Status {
    SB_StatusCode code;
    const char* message; /* may be NULL; owned by library, do not free */
} SB_Status;

/* Opaque handles */
typedef struct SB_Database SB_Database;
typedef struct SB_Session SB_Session;
typedef struct SB_Transaction SB_Transaction;
typedef struct SB_Statement SB_Statement;

typedef struct SB_CreateDbOptions {
    uint32_t page_size;          /* bytes; 0=default */
    const char* default_charset; /* UTF8 if NULL */
    uint32_t page_cache;         /* pages; 0=default */
    uint32_t sweep_interval;     /* seconds; 0=default */
    uint8_t reserve_space;       /* 0/1 */
} SB_CreateDbOptions;

SB_Status sb_create_database(const char* path, const SB_CreateDbOptions* opts,
                             SB_Database** out_db);
SB_Status sb_open_database(const char* path, SB_Database** out_db);
void sb_close_database(SB_Database* db);

SB_Status sb_create_session(SB_Database* db, SB_Session** out_session);

SB_Status sb_begin_transaction(SB_Session* s, SB_Transaction** out_tx);
SB_Status sb_commit(SB_Transaction* tx);
SB_Status sb_rollback(SB_Transaction* tx);

SB_Status sb_prepare(SB_Session* s, const char* sql, SB_Statement** out_stmt);
SB_Status sb_execute(SB_Statement* st, const char* const* params, int32_t num_params);

#ifdef __cplusplus
}
#endif

#endif /* SCRATCHBIRD_CAPI_H */
