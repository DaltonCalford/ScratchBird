#coding:utf-8

"""
ID:          new-database-03
TITLE:       New DB - RDB$COLLATIONS
DESCRIPTION: Check the correct content of RDB$COLLATIONS on new DB.
FBTEST:      functional.basic.db.03
NOTES:
[17.01.2023] pzotov
    DISABLED after discussion with dimitr, letters 17-sep-2022 11:23.
    Reasons:
        * There is no much sense to keep such tests because they fails extremely often during new major FB developing.
        * There is no chanse to get successful outcome for the whole test suite is some of system table became invalid,
          i.e. lot of other tests will be failed in such case.
    Single test for check DDL (type of columns, their order and total number) will be implemented for all RDB-tables.
"""

import pytest
from firebird.qa import *

substitutions = \
    [
        ('RDB\\$SPECIFIC_ATTRIBUTES.*', '')
        ,('COLL-VERSION=\\d+.\\d+(;ICU-VERSION=\\d+.\\d+)?.*', 'COLL-VERSION=<attr>')
        ,('RDB\\$SECURITY_CLASS[ ]+SQL\\$.*', '')
    ]

db = db_factory()

test_script = """
    set list on;
    set blob all;
    set count on;
    -- NB: rdb$collation_name is UNIQUE.
    select * from rdb$collations order by rdb$collation_name;
"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """

    RDB$COLLATION_NAME              ASCII
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            2
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$236
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              BIG_5
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            56
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$368
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              BS_BA
    RDB$COLLATION_ID                6
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$337
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              CP943C
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            68
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$379
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              CP943C_UNICODE
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            68
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1e9
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$380
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              CS_CZ
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            22
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$293
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              CYRL
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            50
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$321
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DA_DA
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$274
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_CSY
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$306
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_DAN865
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            12
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$271
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_DEU437
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$249
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_DEU850
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$260
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_ESP437
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$250
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_ESP850
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$261
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_FIN437
    RDB$COLLATION_ID                6
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$251
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_FRA437
    RDB$COLLATION_ID                7
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$252
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_FRA850
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$262
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_FRC850
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$259
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_FRC863
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            14
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$320
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_ITA437
    RDB$COLLATION_ID                8
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$253
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_ITA850
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$263
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_NLD437
    RDB$COLLATION_ID                9
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$254
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_NLD850
    RDB$COLLATION_ID                6
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$264
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_NOR865
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            12
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$272
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_PLK
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$307
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_PTB850
    RDB$COLLATION_ID                7
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$265
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_PTG860
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            13
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$316
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_RUS
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            50
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$322
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_SLO
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$308
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_SVE437
    RDB$COLLATION_ID                10
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$255
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_SVE850
    RDB$COLLATION_ID                8
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$266
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_TRK
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            46
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$314
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_UK437
    RDB$COLLATION_ID                11
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$256
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_UK850
    RDB$COLLATION_ID                9
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$267
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_US437
    RDB$COLLATION_ID                12
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$257
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DB_US850
    RDB$COLLATION_ID                10
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$268
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DE_DE
    RDB$COLLATION_ID                6
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$279
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS437
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$245
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS737
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            9
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$324
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS775
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            15
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$325
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS850
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            11
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$258
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS852
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$305
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS857
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            46
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$313
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS858
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            16
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$326
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS860
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            13
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$315
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS861
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            47
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$317
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS862
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            17
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$327
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS863
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            14
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$319
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS864
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            18
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$328
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS865
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            12
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$269
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS866
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            48
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$329
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DOS869
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            49
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$330
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              DU_NL
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$275
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              EN_UK
    RDB$COLLATION_ID                12
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$285
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              EN_US
    RDB$COLLATION_ID                14
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$286
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ES_ES
    RDB$COLLATION_ID                10
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1e0
    DISABLE-COMPRESSIONS=1;SPECIALS-FIRST=1
    RDB$SECURITY_CLASS              SQL$283
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ES_ES_CI_AI
    RDB$COLLATION_ID                17
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1e1
    DISABLE-COMPRESSIONS=1;SPECIALS-FIRST=1
    RDB$SECURITY_CLASS              SQL$289
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              EUCJ_0208
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            6
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$244
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              FI_FI
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$276
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              FR_CA
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$278
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              FR_CA_CI_AI
    RDB$COLLATION_ID                19
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         FR_CA
    RDB$SPECIFIC_ATTRIBUTES         1d:1e3
    SPECIALS-FIRST=1
    RDB$SECURITY_CLASS              SQL$291
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              FR_FR
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$277
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              FR_FR_CI_AI
    RDB$COLLATION_ID                18
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         FR_FR
    RDB$SPECIFIC_ATTRIBUTES         1d:1e2
    SPECIALS-FIRST=1
    RDB$SECURITY_CLASS              SQL$290
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              GB18030
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            69
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$381
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              GB18030_UNICODE
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            69
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1ea
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$382
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              GBK
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            67
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$377
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              GBK_UNICODE
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            67
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1e8
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$378
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              GB_2312
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            57
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$369
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_1
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$273
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_13
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            40
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$303
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_2
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            22
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$292
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_3
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            23
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$296
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_4
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            34
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$297
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_5
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            35
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$298
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_6
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            36
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$299
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_7
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            37
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$300
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_8
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            38
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$301
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO8859_9
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            39
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$302
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO_HUN
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            22
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$294
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              ISO_PLK
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            22
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$295
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              IS_IS
    RDB$COLLATION_ID                7
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$280
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              IT_IT
    RDB$COLLATION_ID                8
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$281
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              KOI8R
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            63
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$370
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              KOI8R_RU
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            63
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$371
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              KOI8U
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            64
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$372
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              KOI8U_UA
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            64
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$373
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              KSC_5601
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            44
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$366
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              KSC_DICTIONARY
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            44
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$367
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              LT_LT
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            40
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$304
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NEXT
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            19
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$354
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NONE
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            0
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$234
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NO_NO
    RDB$COLLATION_ID                9
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$282
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NXT_DEU
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            19
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$356
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NXT_ESP
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            19
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$359
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NXT_FRA
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            19
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$357
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NXT_ITA
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            19
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$358
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              NXT_US
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            19
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$355
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              OCTETS
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            1
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$235
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_ASCII
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$246
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_CSY
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$309
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_CYRL
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            50
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$323
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_HUN
    RDB$COLLATION_ID                7
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$311
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_INTL
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$247
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_ISL
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            47
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$318
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_NORDAN4
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            12
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$270
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_PLK
    RDB$COLLATION_ID                6
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$310
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_SLO
    RDB$COLLATION_ID                8
    RDB$CHARACTER_SET_ID            45
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$312
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PDOX_SWEDFIN
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            10
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$248
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PT_BR
    RDB$COLLATION_ID                16
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$288
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PT_PT
    RDB$COLLATION_ID                15
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$287
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_CSY
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$332
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_CYRL
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            52
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$341
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_GREEK
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            54
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$351
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_HUN
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$336
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_HUNDC
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$333
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_INTL
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$344
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_INTL850
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$345
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_NORDAN4
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$346
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_PLK
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$334
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_SLOV
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$335
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_SPAN
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$347
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_SWEDFIN
    RDB$COLLATION_ID                5
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$348
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              PXW_TURK
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            55
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$353
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              SJIS_0208
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            5
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$243
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              SV_SV
    RDB$COLLATION_ID                11
    RDB$CHARACTER_SET_ID            21
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$284
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              TIS620
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            66
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$375
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              TIS620_UNICODE
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            66
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1e7
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$376
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              UCS_BASIC
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            4
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$239
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              UNICODE
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            4
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         1d:1e4
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$240
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              UNICODE_CI
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            4
    RDB$COLLATION_ATTRIBUTES        3
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         UNICODE
    RDB$SPECIFIC_ATTRIBUTES         1d:1e5
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$241
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              UNICODE_CI_AI
    RDB$COLLATION_ID                4
    RDB$CHARACTER_SET_ID            4
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         UNICODE
    RDB$SPECIFIC_ATTRIBUTES         1d:1e6
    COLL-VERSION=153.88
    RDB$SECURITY_CLASS              SQL$242
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              UNICODE_FSS
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            3
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$237
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              UTF8
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            4
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$238
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1250
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$331
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1251
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            52
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$340
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1251_UA
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            52
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$342
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1252
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$343
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1253
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            54
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$350
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1254
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            55
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$352
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1255
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            58
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$360
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1256
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            59
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$361
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1257
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            60
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$362
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1257_EE
    RDB$COLLATION_ID                1
    RDB$CHARACTER_SET_ID            60
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$363
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1257_LT
    RDB$COLLATION_ID                2
    RDB$CHARACTER_SET_ID            60
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$364
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1257_LV
    RDB$COLLATION_ID                3
    RDB$CHARACTER_SET_ID            60
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$365
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN1258
    RDB$COLLATION_ID                0
    RDB$CHARACTER_SET_ID            65
    RDB$COLLATION_ATTRIBUTES        1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$374
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN_CZ
    RDB$COLLATION_ID                7
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        3
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$338
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN_CZ_CI_AI
    RDB$COLLATION_ID                8
    RDB$CHARACTER_SET_ID            51
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$339
    RDB$OWNER_NAME                  SYSDBA

    RDB$COLLATION_NAME              WIN_PTBR
    RDB$COLLATION_ID                6
    RDB$CHARACTER_SET_ID            53
    RDB$COLLATION_ATTRIBUTES        7
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BASE_COLLATION_NAME         <null>
    RDB$SPECIFIC_ATTRIBUTES         <null>
    RDB$SECURITY_CLASS              SQL$349
    RDB$OWNER_NAME                  SYSDBA


    Records affected: 149
"""

@pytest.mark.version('>=3.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
