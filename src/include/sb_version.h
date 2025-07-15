/*
 * ScratchBird Database Version Information
 * Based on Firebird 6.0.0.929 with SQL Dialect 4 enhancements
 */

#ifndef SB_VERSION_H
#define SB_VERSION_H

#define SCRATCHBIRD_MAJOR_VER    0
#define SCRATCHBIRD_MINOR_VER    6
#define SCRATCHBIRD_REV_NO       0
#define SCRATCHBIRD_BUILD_NO     1

#define SCRATCHBIRD_VERSION      "0.6.0"
#define SCRATCHBIRD_BUILD        "ScratchBird-0.6.0.1"

// SQL Dialect 4 support
#define SCRATCHBIRD_SQL_DIALECT_MAX  4
#define SCRATCHBIRD_HIERARCHICAL_SCHEMAS  1
#define SCRATCHBIRD_DATABASE_LINKS        1
#define SCRATCHBIRD_SYNONYMS              1

// Tool names
#define SCRATCHBIRD_ISQL         "sb_isql"
#define SCRATCHBIRD_GBAK         "sb_gbak" 
#define SCRATCHBIRD_GFIX         "sb_gfix"
#define SCRATCHBIRD_GSEC         "sb_gsec"
#define SCRATCHBIRD_GSTAT        "sb_gstat"
#define SCRATCHBIRD_NBACKUP      "sb_nbackup"
#define SCRATCHBIRD_SVCMGR       "sb_svcmgr"
#define SCRATCHBIRD_TRACEMGR     "sb_tracemgr"

// Company/Product information
#define SCRATCHBIRD_COMPANY      "ScratchBird Project"
#define SCRATCHBIRD_PRODUCT      "ScratchBird Database"
#define SCRATCHBIRD_COPYRIGHT    "Copyright (C) 2024 ScratchBird Project"

#endif /* SB_VERSION_H */
