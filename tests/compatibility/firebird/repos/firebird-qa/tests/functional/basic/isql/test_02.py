#coding:utf-8

"""
ID:          isql-02
TITLE:       ISQL - SHOW SYSTEM TABLES
DESCRIPTION: Check for correct output of "SHOW SYSTEM;" command on empty database.
NOTES:
    [01.02.2026] pzotov
    Added 'mon$local_temporary_tables' and 'mon$local_temporary_table_columns' - since 6.0.0.1403.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', 'show system;')

##############################################################
# version: 3.0
##############################################################

fb3x_checked_stdout = """
Tables:
       MON$ATTACHMENTS                        MON$CALL_STACK
       MON$CONTEXT_VARIABLES                  MON$DATABASE
       MON$IO_STATS                           MON$MEMORY_USAGE
       MON$RECORD_STATS                       MON$STATEMENTS
       MON$TABLE_STATS                        MON$TRANSACTIONS
       RDB$AUTH_MAPPING                       RDB$BACKUP_HISTORY
       RDB$CHARACTER_SETS                     RDB$CHECK_CONSTRAINTS
       RDB$COLLATIONS                         RDB$DATABASE
       RDB$DB_CREATORS                        RDB$DEPENDENCIES
       RDB$EXCEPTIONS                         RDB$FIELDS
       RDB$FIELD_DIMENSIONS                   RDB$FILES
       RDB$FILTERS                            RDB$FORMATS
       RDB$FUNCTIONS                          RDB$FUNCTION_ARGUMENTS
       RDB$GENERATORS                         RDB$INDEX_SEGMENTS
       RDB$INDICES                            RDB$LOG_FILES
       RDB$PACKAGES                           RDB$PAGES
       RDB$PROCEDURES                         RDB$PROCEDURE_PARAMETERS
       RDB$REF_CONSTRAINTS                    RDB$RELATIONS
       RDB$RELATION_CONSTRAINTS               RDB$RELATION_FIELDS
       RDB$ROLES                              RDB$SECURITY_CLASSES
       RDB$TRANSACTIONS                       RDB$TRIGGERS
       RDB$TRIGGER_MESSAGES                   RDB$TYPES
       RDB$USER_PRIVILEGES                    RDB$VIEW_RELATIONS
       SEC$DB_CREATORS                        SEC$GLOBAL_AUTH_MAPPING
       SEC$USERS                              SEC$USER_ATTRIBUTES

Collations:
       ASCII                                  BIG_5
       BS_BA                                  CP943C
       CP943C_UNICODE                         CS_CZ
       CYRL                                   DA_DA
       DB_CSY                                 DB_DAN865
       DB_DEU437                              DB_DEU850
       DB_ESP437                              DB_ESP850
       DB_FIN437                              DB_FRA437
       DB_FRA850                              DB_FRC850
       DB_FRC863                              DB_ITA437
       DB_ITA850                              DB_NLD437
       DB_NLD850                              DB_NOR865
       DB_PLK                                 DB_PTB850
       DB_PTG860                              DB_RUS
       DB_SLO                                 DB_SVE437
       DB_SVE850                              DB_TRK
       DB_UK437                               DB_UK850
       DB_US437                               DB_US850
       DE_DE                                  DOS437
       DOS737                                 DOS775
       DOS850                                 DOS852
       DOS857                                 DOS858
       DOS860                                 DOS861
       DOS862                                 DOS863
       DOS864                                 DOS865
       DOS866                                 DOS869
       DU_NL                                  EN_UK
       EN_US                                  ES_ES
       ES_ES_CI_AI                            EUCJ_0208
       FI_FI                                  FR_CA
       FR_CA_CI_AI                            FR_FR
       FR_FR_CI_AI                            GB18030
       GB18030_UNICODE                        GBK
       GBK_UNICODE                            GB_2312
       ISO8859_1                              ISO8859_13
       ISO8859_2                              ISO8859_3
       ISO8859_4                              ISO8859_5
       ISO8859_6                              ISO8859_7
       ISO8859_8                              ISO8859_9
       ISO_HUN                                ISO_PLK
       IS_IS                                  IT_IT
       KOI8R                                  KOI8R_RU
       KOI8U                                  KOI8U_UA
       KSC_5601                               KSC_DICTIONARY
       LT_LT                                  NEXT
       NONE                                   NO_NO
       NXT_DEU                                NXT_ESP
       NXT_FRA                                NXT_ITA
       NXT_US                                 OCTETS
       PDOX_ASCII                             PDOX_CSY
       PDOX_CYRL                              PDOX_HUN
       PDOX_INTL                              PDOX_ISL
       PDOX_NORDAN4                           PDOX_PLK
       PDOX_SLO                               PDOX_SWEDFIN
       PT_BR                                  PT_PT
       PXW_CSY                                PXW_CYRL
       PXW_GREEK                              PXW_HUN
       PXW_HUNDC                              PXW_INTL
       PXW_INTL850                            PXW_NORDAN4
       PXW_PLK                                PXW_SLOV
       PXW_SPAN                               PXW_SWEDFIN
       PXW_TURK                               SJIS_0208
       SV_SV                                  TIS620
       TIS620_UNICODE                         UCS_BASIC
       UNICODE                                UNICODE_CI
       UNICODE_CI_AI                          UNICODE_FSS
       UTF8                                   WIN1250
       WIN1251                                WIN1251_UA
       WIN1252                                WIN1253
       WIN1254                                WIN1255
       WIN1256                                WIN1257
       WIN1257_EE                             WIN1257_LT
       WIN1257_LV                             WIN1258
       WIN_CZ                                 WIN_CZ_CI_AI
       WIN_PTBR
"""

##############################################################
# version: 4.0
##############################################################

fb4x_checked_stdout = """
    Tables:
    MON$ATTACHMENTS
    MON$CALL_STACK
    MON$CONTEXT_VARIABLES
    MON$DATABASE
    MON$IO_STATS
    MON$MEMORY_USAGE
    MON$RECORD_STATS
    MON$STATEMENTS
    MON$TABLE_STATS
    MON$TRANSACTIONS
    RDB$AUTH_MAPPING
    RDB$BACKUP_HISTORY
    RDB$CHARACTER_SETS
    RDB$CHECK_CONSTRAINTS
    RDB$COLLATIONS
    RDB$CONFIG
    RDB$DATABASE
    RDB$DB_CREATORS
    RDB$DEPENDENCIES
    RDB$EXCEPTIONS
    RDB$FIELDS
    RDB$FIELD_DIMENSIONS
    RDB$FILES
    RDB$FILTERS
    RDB$FORMATS
    RDB$FUNCTIONS
    RDB$FUNCTION_ARGUMENTS
    RDB$GENERATORS
    RDB$INDEX_SEGMENTS
    RDB$INDICES
    RDB$LOG_FILES
    RDB$PACKAGES
    RDB$PAGES
    RDB$PROCEDURES
    RDB$PROCEDURE_PARAMETERS
    RDB$PUBLICATIONS
    RDB$PUBLICATION_TABLES
    RDB$REF_CONSTRAINTS
    RDB$RELATIONS
    RDB$RELATION_CONSTRAINTS
    RDB$RELATION_FIELDS
    RDB$ROLES
    RDB$SECURITY_CLASSES
    RDB$TIME_ZONES
    RDB$TRANSACTIONS
    RDB$TRIGGERS
    RDB$TRIGGER_MESSAGES
    RDB$TYPES
    RDB$USER_PRIVILEGES
    RDB$VIEW_RELATIONS
    SEC$DB_CREATORS
    SEC$GLOBAL_AUTH_MAPPING
    SEC$USERS
    SEC$USER_ATTRIBUTES

    Collations:
    ASCII
    BIG_5
    BS_BA
    CP943C
    CP943C_UNICODE
    CS_CZ
    CYRL
    DA_DA
    DB_CSY
    DB_DAN865
    DB_DEU437
    DB_DEU850
    DB_ESP437
    DB_ESP850
    DB_FIN437
    DB_FRA437
    DB_FRA850
    DB_FRC850
    DB_FRC863
    DB_ITA437
    DB_ITA850
    DB_NLD437
    DB_NLD850
    DB_NOR865
    DB_PLK
    DB_PTB850
    DB_PTG860
    DB_RUS
    DB_SLO
    DB_SVE437
    DB_SVE850
    DB_TRK
    DB_UK437
    DB_UK850
    DB_US437
    DB_US850
    DE_DE
    DOS437
    DOS737
    DOS775
    DOS850
    DOS852
    DOS857
    DOS858
    DOS860
    DOS861
    DOS862
    DOS863
    DOS864
    DOS865
    DOS866
    DOS869
    DU_NL
    EN_UK
    EN_US
    ES_ES
    ES_ES_CI_AI
    EUCJ_0208
    FI_FI
    FR_CA
    FR_CA_CI_AI
    FR_FR
    FR_FR_CI_AI
    GB18030
    GB18030_UNICODE
    GBK
    GBK_UNICODE
    GB_2312
    ISO8859_1
    ISO8859_13
    ISO8859_2
    ISO8859_3
    ISO8859_4
    ISO8859_5
    ISO8859_6
    ISO8859_7
    ISO8859_8
    ISO8859_9
    ISO_HUN
    ISO_PLK
    IS_IS
    IT_IT
    KOI8R
    KOI8R_RU
    KOI8U
    KOI8U_UA
    KSC_5601
    KSC_DICTIONARY
    LT_LT
    NEXT
    NONE
    NO_NO
    NXT_DEU
    NXT_ESP
    NXT_FRA
    NXT_ITA
    NXT_US
    OCTETS
    PDOX_ASCII
    PDOX_CSY
    PDOX_CYRL
    PDOX_HUN
    PDOX_INTL
    PDOX_ISL
    PDOX_NORDAN4
    PDOX_PLK
    PDOX_SLO
    PDOX_SWEDFIN
    PT_BR
    PT_PT
    PXW_CSY
    PXW_CYRL
    PXW_GREEK
    PXW_HUN
    PXW_HUNDC
    PXW_INTL
    PXW_INTL850
    PXW_NORDAN4
    PXW_PLK
    PXW_SLOV
    PXW_SPAN
    PXW_SWEDFIN
    PXW_TURK
    SJIS_0208
    SV_SV
    TIS620
    TIS620_UNICODE
    UCS_BASIC
    UNICODE
    UNICODE_CI
    UNICODE_CI_AI
    UNICODE_FSS
    UTF8
    WIN1250
    WIN1251
    WIN1251_UA
    WIN1252
    WIN1253
    WIN1254
    WIN1255
    WIN1256
    WIN1257
    WIN1257_EE
    WIN1257_LT
    WIN1257_LV
    WIN1258
    WIN_CZ
    WIN_CZ_CI_AI
    WIN_PTBR

    Roles:
    RDB$ADMIN
"""

##############################################################
# version: 5.0
##############################################################

fb5x_checked_stdout = """
    Tables:
    MON$ATTACHMENTS
    MON$CALL_STACK
    MON$COMPILED_STATEMENTS
    MON$CONTEXT_VARIABLES
    MON$DATABASE
    MON$IO_STATS
    MON$MEMORY_USAGE
    MON$RECORD_STATS
    MON$STATEMENTS
    MON$TABLE_STATS
    MON$TRANSACTIONS
    RDB$AUTH_MAPPING
    RDB$BACKUP_HISTORY
    RDB$CHARACTER_SETS
    RDB$CHECK_CONSTRAINTS
    RDB$COLLATIONS
    RDB$CONFIG
    RDB$DATABASE
    RDB$DB_CREATORS
    RDB$DEPENDENCIES
    RDB$EXCEPTIONS
    RDB$FIELDS
    RDB$FIELD_DIMENSIONS
    RDB$FILES
    RDB$FILTERS
    RDB$FORMATS
    RDB$FUNCTIONS
    RDB$FUNCTION_ARGUMENTS
    RDB$GENERATORS
    RDB$INDEX_SEGMENTS
    RDB$INDICES
    RDB$KEYWORDS
    RDB$LOG_FILES
    RDB$PACKAGES
    RDB$PAGES
    RDB$PROCEDURES
    RDB$PROCEDURE_PARAMETERS
    RDB$PUBLICATIONS
    RDB$PUBLICATION_TABLES
    RDB$REF_CONSTRAINTS
    RDB$RELATIONS
    RDB$RELATION_CONSTRAINTS
    RDB$RELATION_FIELDS
    RDB$ROLES
    RDB$SECURITY_CLASSES
    RDB$TIME_ZONES
    RDB$TRANSACTIONS
    RDB$TRIGGERS
    RDB$TRIGGER_MESSAGES
    RDB$TYPES
    RDB$USER_PRIVILEGES
    RDB$VIEW_RELATIONS
    SEC$DB_CREATORS
    SEC$GLOBAL_AUTH_MAPPING
    SEC$USERS
    SEC$USER_ATTRIBUTES
    Functions:
    RDB$BLOB_UTIL.IS_WRITABLE
    RDB$BLOB_UTIL.NEW_BLOB
    RDB$BLOB_UTIL.OPEN_BLOB
    RDB$BLOB_UTIL.READ_DATA
    RDB$BLOB_UTIL.SEEK
    RDB$PROFILER.START_SESSION
    RDB$TIME_ZONE_UTIL.DATABASE_VERSION
    Procedures:
    RDB$BLOB_UTIL.CANCEL_BLOB
    RDB$BLOB_UTIL.CLOSE_HANDLE
    RDB$PROFILER.CANCEL_SESSION
    RDB$PROFILER.DISCARD
    RDB$PROFILER.FINISH_SESSION
    RDB$PROFILER.FLUSH
    RDB$PROFILER.PAUSE_SESSION
    RDB$PROFILER.RESUME_SESSION
    RDB$PROFILER.SET_FLUSH_INTERVAL
    RDB$TIME_ZONE_UTIL.TRANSITIONS
    Packages:
    RDB$BLOB_UTIL
    RDB$PROFILER
    RDB$TIME_ZONE_UTIL
    Collations:
    ASCII
    BIG_5
    BS_BA
    CP943C
    CP943C_UNICODE
    CS_CZ
    CYRL
    DA_DA
    DB_CSY
    DB_DAN865
    DB_DEU437
    DB_DEU850
    DB_ESP437
    DB_ESP850
    DB_FIN437
    DB_FRA437
    DB_FRA850
    DB_FRC850
    DB_FRC863
    DB_ITA437
    DB_ITA850
    DB_NLD437
    DB_NLD850
    DB_NOR865
    DB_PLK
    DB_PTB850
    DB_PTG860
    DB_RUS
    DB_SLO
    DB_SVE437
    DB_SVE850
    DB_TRK
    DB_UK437
    DB_UK850
    DB_US437
    DB_US850
    DE_DE
    DOS437
    DOS737
    DOS775
    DOS850
    DOS852
    DOS857
    DOS858
    DOS860
    DOS861
    DOS862
    DOS863
    DOS864
    DOS865
    DOS866
    DOS869
    DU_NL
    EN_UK
    EN_US
    ES_ES
    ES_ES_CI_AI
    EUCJ_0208
    FI_FI
    FR_CA
    FR_CA_CI_AI
    FR_FR
    FR_FR_CI_AI
    GB18030
    GB18030_UNICODE
    GBK
    GBK_UNICODE
    GB_2312
    ISO8859_1
    ISO8859_13
    ISO8859_2
    ISO8859_3
    ISO8859_4
    ISO8859_5
    ISO8859_6
    ISO8859_7
    ISO8859_8
    ISO8859_9
    ISO_HUN
    ISO_PLK
    IS_IS
    IT_IT
    KOI8R
    KOI8R_RU
    KOI8U
    KOI8U_UA
    KSC_5601
    KSC_DICTIONARY
    LT_LT
    NEXT
    NONE
    NO_NO
    NXT_DEU
    NXT_ESP
    NXT_FRA
    NXT_ITA
    NXT_US
    OCTETS
    PDOX_ASCII
    PDOX_CSY
    PDOX_CYRL
    PDOX_HUN
    PDOX_INTL
    PDOX_ISL
    PDOX_NORDAN4
    PDOX_PLK
    PDOX_SLO
    PDOX_SWEDFIN
    PT_BR
    PT_PT
    PXW_CSY
    PXW_CYRL
    PXW_GREEK
    PXW_HUN
    PXW_HUNDC
    PXW_INTL
    PXW_INTL850
    PXW_NORDAN4
    PXW_PLK
    PXW_SLOV
    PXW_SPAN
    PXW_SWEDFIN
    PXW_TURK
    SJIS_0208
    SV_SV
    TIS620
    TIS620_UNICODE
    UCS_BASIC
    UNICODE
    UNICODE_CI
    UNICODE_CI_AI
    UNICODE_FSS
    UTF8
    WIN1250
    WIN1251
    WIN1251_UA
    WIN1252
    WIN1253
    WIN1254
    WIN1255
    WIN1256
    WIN1257
    WIN1257_EE
    WIN1257_LT
    WIN1257_LV
    WIN1258
    WIN_CZ
    WIN_CZ_CI_AI
    WIN_PTBR
    Roles:
    RDB$ADMIN
    Publications:
    RDB$DEFAULT
"""

fb6x_checked_stdout = """
        Schemas:
        SYSTEM; Default character set: SYSTEM.UTF8
        Tables:
        SYSTEM.MON$ATTACHMENTS
        SYSTEM.MON$CALL_STACK
        SYSTEM.MON$COMPILED_STATEMENTS
        SYSTEM.MON$CONTEXT_VARIABLES
        SYSTEM.MON$DATABASE
        SYSTEM.MON$IO_STATS
        SYSTEM.MON$LOCAL_TEMPORARY_TABLES
        SYSTEM.MON$LOCAL_TEMPORARY_TABLE_COLUMNS
        SYSTEM.MON$MEMORY_USAGE
        SYSTEM.MON$RECORD_STATS
        SYSTEM.MON$STATEMENTS
        SYSTEM.MON$TABLE_STATS
        SYSTEM.MON$TRANSACTIONS
        SYSTEM.RDB$AUTH_MAPPING
        SYSTEM.RDB$BACKUP_HISTORY
        SYSTEM.RDB$CHARACTER_SETS
        SYSTEM.RDB$CHECK_CONSTRAINTS
        SYSTEM.RDB$COLLATIONS
        SYSTEM.RDB$CONFIG
        SYSTEM.RDB$DATABASE
        SYSTEM.RDB$DB_CREATORS
        SYSTEM.RDB$DEPENDENCIES
        SYSTEM.RDB$EXCEPTIONS
        SYSTEM.RDB$FIELDS
        SYSTEM.RDB$FIELD_DIMENSIONS
        SYSTEM.RDB$FILES
        SYSTEM.RDB$FILTERS
        SYSTEM.RDB$FORMATS
        SYSTEM.RDB$FUNCTIONS
        SYSTEM.RDB$FUNCTION_ARGUMENTS
        SYSTEM.RDB$GENERATORS
        SYSTEM.RDB$INDEX_SEGMENTS
        SYSTEM.RDB$INDICES
        SYSTEM.RDB$KEYWORDS
        SYSTEM.RDB$LOG_FILES
        SYSTEM.RDB$PACKAGES
        SYSTEM.RDB$PAGES
        SYSTEM.RDB$PROCEDURES
        SYSTEM.RDB$PROCEDURE_PARAMETERS
        SYSTEM.RDB$PUBLICATIONS
        SYSTEM.RDB$PUBLICATION_TABLES
        SYSTEM.RDB$REF_CONSTRAINTS
        SYSTEM.RDB$RELATIONS
        SYSTEM.RDB$RELATION_CONSTRAINTS
        SYSTEM.RDB$RELATION_FIELDS
        SYSTEM.RDB$ROLES
        SYSTEM.RDB$SCHEMAS
        SYSTEM.RDB$SECURITY_CLASSES
        SYSTEM.RDB$TIME_ZONES
        SYSTEM.RDB$TRANSACTIONS
        SYSTEM.RDB$TRIGGERS
        SYSTEM.RDB$TRIGGER_MESSAGES
        SYSTEM.RDB$TYPES
        SYSTEM.RDB$USER_PRIVILEGES
        SYSTEM.RDB$VIEW_RELATIONS
        SYSTEM.SEC$DB_CREATORS
        SYSTEM.SEC$GLOBAL_AUTH_MAPPING
        SYSTEM.SEC$USERS
        SYSTEM.SEC$USER_ATTRIBUTES
        Functions:
        SYSTEM.RDB$BLOB_UTIL.IS_WRITABLE
        SYSTEM.RDB$BLOB_UTIL.NEW_BLOB
        SYSTEM.RDB$BLOB_UTIL.OPEN_BLOB
        SYSTEM.RDB$BLOB_UTIL.READ_DATA
        SYSTEM.RDB$BLOB_UTIL.SEEK
        SYSTEM.RDB$PROFILER.START_SESSION
        SYSTEM.RDB$TIME_ZONE_UTIL.DATABASE_VERSION
        Procedures:
        SYSTEM.RDB$BLOB_UTIL.CANCEL_BLOB
        SYSTEM.RDB$BLOB_UTIL.CLOSE_HANDLE
        SYSTEM.RDB$PROFILER.CANCEL_SESSION
        SYSTEM.RDB$PROFILER.DISCARD
        SYSTEM.RDB$PROFILER.FINISH_SESSION
        SYSTEM.RDB$PROFILER.FLUSH
        SYSTEM.RDB$PROFILER.PAUSE_SESSION
        SYSTEM.RDB$PROFILER.RESUME_SESSION
        SYSTEM.RDB$PROFILER.SET_FLUSH_INTERVAL
        SYSTEM.RDB$SQL.EXPLAIN
        SYSTEM.RDB$SQL.PARSE_UNQUALIFIED_NAMES
        SYSTEM.RDB$TIME_ZONE_UTIL.TRANSITIONS
        Packages:
        SYSTEM.RDB$BLOB_UTIL
        SYSTEM.RDB$PROFILER
        SYSTEM.RDB$SQL
        SYSTEM.RDB$TIME_ZONE_UTIL
        Collations:
        SYSTEM.ASCII
        SYSTEM.BIG_5
        SYSTEM.BS_BA
        SYSTEM.CP943C
        SYSTEM.CP943C_UNICODE
        SYSTEM.CS_CZ
        SYSTEM.CYRL
        SYSTEM.DA_DA
        SYSTEM.DB_CSY
        SYSTEM.DB_DAN865
        SYSTEM.DB_DEU437
        SYSTEM.DB_DEU850
        SYSTEM.DB_ESP437
        SYSTEM.DB_ESP850
        SYSTEM.DB_FIN437
        SYSTEM.DB_FRA437
        SYSTEM.DB_FRA850
        SYSTEM.DB_FRC850
        SYSTEM.DB_FRC863
        SYSTEM.DB_ITA437
        SYSTEM.DB_ITA850
        SYSTEM.DB_NLD437
        SYSTEM.DB_NLD850
        SYSTEM.DB_NOR865
        SYSTEM.DB_PLK
        SYSTEM.DB_PTB850
        SYSTEM.DB_PTG860
        SYSTEM.DB_RUS
        SYSTEM.DB_SLO
        SYSTEM.DB_SVE437
        SYSTEM.DB_SVE850
        SYSTEM.DB_TRK
        SYSTEM.DB_UK437
        SYSTEM.DB_UK850
        SYSTEM.DB_US437
        SYSTEM.DB_US850
        SYSTEM.DE_DE
        SYSTEM.DOS437
        SYSTEM.DOS737
        SYSTEM.DOS775
        SYSTEM.DOS850
        SYSTEM.DOS852
        SYSTEM.DOS857
        SYSTEM.DOS858
        SYSTEM.DOS860
        SYSTEM.DOS861
        SYSTEM.DOS862
        SYSTEM.DOS863
        SYSTEM.DOS864
        SYSTEM.DOS865
        SYSTEM.DOS866
        SYSTEM.DOS869
        SYSTEM.DU_NL
        SYSTEM.EN_UK
        SYSTEM.EN_US
        SYSTEM.ES_ES
        SYSTEM.ES_ES_CI_AI
        SYSTEM.EUCJ_0208
        SYSTEM.FI_FI
        SYSTEM.FR_CA
        SYSTEM.FR_CA_CI_AI
        SYSTEM.FR_FR
        SYSTEM.FR_FR_CI_AI
        SYSTEM.GB18030
        SYSTEM.GB18030_UNICODE
        SYSTEM.GBK
        SYSTEM.GBK_UNICODE
        SYSTEM.GB_2312
        SYSTEM.ISO8859_1
        SYSTEM.ISO8859_13
        SYSTEM.ISO8859_2
        SYSTEM.ISO8859_3
        SYSTEM.ISO8859_4
        SYSTEM.ISO8859_5
        SYSTEM.ISO8859_6
        SYSTEM.ISO8859_7
        SYSTEM.ISO8859_8
        SYSTEM.ISO8859_9
        SYSTEM.ISO_HUN
        SYSTEM.ISO_PLK
        SYSTEM.IS_IS
        SYSTEM.IT_IT
        SYSTEM.KOI8R
        SYSTEM.KOI8R_RU
        SYSTEM.KOI8U
        SYSTEM.KOI8U_UA
        SYSTEM.KSC_5601
        SYSTEM.KSC_DICTIONARY
        SYSTEM.LT_LT
        SYSTEM.NEXT
        SYSTEM.NONE
        SYSTEM.NO_NO
        SYSTEM.NXT_DEU
        SYSTEM.NXT_ESP
        SYSTEM.NXT_FRA
        SYSTEM.NXT_ITA
        SYSTEM.NXT_US
        SYSTEM.OCTETS
        SYSTEM.PDOX_ASCII
        SYSTEM.PDOX_CSY
        SYSTEM.PDOX_CYRL
        SYSTEM.PDOX_HUN
        SYSTEM.PDOX_INTL
        SYSTEM.PDOX_ISL
        SYSTEM.PDOX_NORDAN4
        SYSTEM.PDOX_PLK
        SYSTEM.PDOX_SLO
        SYSTEM.PDOX_SWEDFIN
        SYSTEM.PT_BR
        SYSTEM.PT_PT
        SYSTEM.PXW_CSY
        SYSTEM.PXW_CYRL
        SYSTEM.PXW_GREEK
        SYSTEM.PXW_HUN
        SYSTEM.PXW_HUNDC
        SYSTEM.PXW_INTL
        SYSTEM.PXW_INTL850
        SYSTEM.PXW_NORDAN4
        SYSTEM.PXW_PLK
        SYSTEM.PXW_SLOV
        SYSTEM.PXW_SPAN
        SYSTEM.PXW_SWEDFIN
        SYSTEM.PXW_TURK
        SYSTEM.SJIS_0208
        SYSTEM.SV_SV
        SYSTEM.TIS620
        SYSTEM.TIS620_UNICODE
        SYSTEM.UCS_BASIC
        SYSTEM.UNICODE
        SYSTEM.UNICODE_CI
        SYSTEM.UNICODE_CI_AI
        SYSTEM.UNICODE_FSS
        SYSTEM.UTF8
        SYSTEM.WIN1250
        SYSTEM.WIN1251
        SYSTEM.WIN1251_UA
        SYSTEM.WIN1252
        SYSTEM.WIN1253
        SYSTEM.WIN1254
        SYSTEM.WIN1255
        SYSTEM.WIN1256
        SYSTEM.WIN1257
        SYSTEM.WIN1257_EE
        SYSTEM.WIN1257_LT
        SYSTEM.WIN1257_LV
        SYSTEM.WIN1258
        SYSTEM.WIN_CZ
        SYSTEM.WIN_CZ_CI_AI
        SYSTEM.WIN_PTBR
        Roles:
        RDB$ADMIN
        Publications:
        RDB$DEFAULT
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<4') else fb4x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
