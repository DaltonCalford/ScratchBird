#coding:utf-8

"""
ID:          new-database-02
TITLE:       New DB - RDB$CHARACTER_SETS
DESCRIPTION: Check the correct content of RDB$CHARACTER_SETS for new database
FBTEST:      functional.basic.db.02
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

db = db_factory()

test_script = """
    set list on;
    set blob all;
    set count on;
    select * from rdb$character_sets order by rdb$character_set_id;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$SECURITY_CLASS[ ]+SQL\\$.*', '')])

expected_stdout = """
    RDB$CHARACTER_SET_NAME          NONE
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        NONE
    RDB$CHARACTER_SET_ID            0
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$182
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          OCTETS
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        OCTETS
    RDB$CHARACTER_SET_ID            1
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$183
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ASCII
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ASCII
    RDB$CHARACTER_SET_ID            2
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$184
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          UNICODE_FSS
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        UNICODE_FSS
    RDB$CHARACTER_SET_ID            3
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         3
    RDB$SECURITY_CLASS              SQL$185
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          UTF8
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        UTF8
    RDB$CHARACTER_SET_ID            4
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         4
    RDB$SECURITY_CLASS              SQL$186
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          SJIS_0208
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        SJIS_0208
    RDB$CHARACTER_SET_ID            5
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$187
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          EUCJ_0208
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        EUCJ_0208
    RDB$CHARACTER_SET_ID            6
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$188
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS737
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS737
    RDB$CHARACTER_SET_ID            9
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$208
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS437
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS437
    RDB$CHARACTER_SET_ID            10
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$189
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS850
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS850
    RDB$CHARACTER_SET_ID            11
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$190
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS865
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS865
    RDB$CHARACTER_SET_ID            12
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$191
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS860
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS860
    RDB$CHARACTER_SET_ID            13
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$204
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS863
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS863
    RDB$CHARACTER_SET_ID            14
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$206
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS775
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS775
    RDB$CHARACTER_SET_ID            15
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$209
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS858
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS858
    RDB$CHARACTER_SET_ID            16
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$210
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS862
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS862
    RDB$CHARACTER_SET_ID            17
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$211
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS864
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS864
    RDB$CHARACTER_SET_ID            18
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$212
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          NEXT
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        NEXT
    RDB$CHARACTER_SET_ID            19
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$220
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_1
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_1
    RDB$CHARACTER_SET_ID            21
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$192
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_2
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_2
    RDB$CHARACTER_SET_ID            22
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$193
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_3
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_3
    RDB$CHARACTER_SET_ID            23
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$194
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_4
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_4
    RDB$CHARACTER_SET_ID            34
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$195
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_5
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_5
    RDB$CHARACTER_SET_ID            35
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$196
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_6
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_6
    RDB$CHARACTER_SET_ID            36
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$197
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_7
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_7
    RDB$CHARACTER_SET_ID            37
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$198
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_8
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_8
    RDB$CHARACTER_SET_ID            38
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$199
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_9
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_9
    RDB$CHARACTER_SET_ID            39
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$200
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          ISO8859_13
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        ISO8859_13
    RDB$CHARACTER_SET_ID            40
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$201
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          KSC_5601
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        KSC_5601
    RDB$CHARACTER_SET_ID            44
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$224
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS852
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS852
    RDB$CHARACTER_SET_ID            45
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$202
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS857
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS857
    RDB$CHARACTER_SET_ID            46
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$203
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS861
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS861
    RDB$CHARACTER_SET_ID            47
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$205
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS866
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS866
    RDB$CHARACTER_SET_ID            48
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$213
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          DOS869
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        DOS869
    RDB$CHARACTER_SET_ID            49
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$214
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          CYRL
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        CYRL
    RDB$CHARACTER_SET_ID            50
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$207
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1250
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1250
    RDB$CHARACTER_SET_ID            51
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$215
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1251
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1251
    RDB$CHARACTER_SET_ID            52
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$216
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1252
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1252
    RDB$CHARACTER_SET_ID            53
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$217
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1253
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1253
    RDB$CHARACTER_SET_ID            54
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$218
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1254
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1254
    RDB$CHARACTER_SET_ID            55
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$219
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          BIG_5
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        BIG_5
    RDB$CHARACTER_SET_ID            56
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$225
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          GB_2312
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        GB_2312
    RDB$CHARACTER_SET_ID            57
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$226
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1255
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1255
    RDB$CHARACTER_SET_ID            58
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$221
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1256
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1256
    RDB$CHARACTER_SET_ID            59
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$222
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1257
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1257
    RDB$CHARACTER_SET_ID            60
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$223
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          KOI8R
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        KOI8R
    RDB$CHARACTER_SET_ID            63
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$227
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          KOI8U
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        KOI8U
    RDB$CHARACTER_SET_ID            64
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$228
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          WIN1258
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        WIN1258
    RDB$CHARACTER_SET_ID            65
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$229
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          TIS620
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        TIS620
    RDB$CHARACTER_SET_ID            66
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         1
    RDB$SECURITY_CLASS              SQL$230
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          GBK
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        GBK
    RDB$CHARACTER_SET_ID            67
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$231
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          CP943C
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        CP943C
    RDB$CHARACTER_SET_ID            68
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         2
    RDB$SECURITY_CLASS              SQL$232
    RDB$OWNER_NAME                  SYSDBA

    RDB$CHARACTER_SET_NAME          GB18030
    RDB$FORM_OF_USE                 <null>
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$DEFAULT_COLLATE_NAME        GB18030
    RDB$CHARACTER_SET_ID            69
    RDB$SYSTEM_FLAG                 1
    RDB$DESCRIPTION                 <null>
    RDB$FUNCTION_NAME               <null>
    RDB$BYTES_PER_CHARACTER         4
    RDB$SECURITY_CLASS              SQL$233
    RDB$OWNER_NAME                  SYSDBA


    Records affected: 52
"""

@pytest.mark.version('>=3.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
