#coding:utf-8

"""
ID:          issue-8701
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8701
TITLE:       Unable to select from a view which was created using 'CREATE VIEW (<column_name>)' if connection charset = utf8 and query returns [var]char
DESCRIPTION:
NOTES:
    [07.10.2025] pzotov
    Fixed by 4e3a0026

    Confirmed bug on 6.0.0.1293-2caedd2.
    Checked on 6.0.0.1295-b14254f
"""

import pytest
from firebird.qa import *

init_script = """
    set bail on;
    -- case-a: declaring view without specifying [<full_column_list>]:
    create view v_dual_nul_a as select null as implicit_col_name_for_null from rdb$database;
    create view v_dual_num_a as select 123 as implicit_col_name_for_numeric from rdb$database;
    create view v_dual_dts_a as select cast('01.02.2025 12:13:14' as timestamp) as implicit_col_name_for_timestamp from rdb$database;
    create view v_dual_boo_a as select true as implicit_col_name_for_boolean from rdb$database;
    create view v_dual_blb_a as select cast('bin' as blob sub_type text) as implicit_col_name_for_blob from rdb$database;
    create view v_dual_txt_a as select 'TXT' as implicit_col_name_for_char from rdb$database;

    -- case-b: declaring view __with__ specifying [<full_column_list>]:
    create view v_dual_nul_b(explicit_col_name_for_null) as select null from rdb$database;
    create view v_dual_num_b(explicit_col_name_for_numeric) as select 123 from rdb$database;
    create view v_dual_dts_b(explicit_col_name_for_timestamp) as select cast('01.02.2025 12:13:14' as timestamp) from rdb$database;
    create view v_dual_boo_b(explicit_col_name_for_boolean) as select true from rdb$database;
    create view v_dual_blb_b(explicit_col_name_for_blob) as select cast('bin' as blob sub_type text) from rdb$database;

    create view v_dual_txt_b(explicit_col_name_for_char) as select 'TXT' from rdb$database;
"""

db = db_factory(charset = 'utf8', init = init_script)

substitutions = [ ('^((?!(SQLSTATE|string|expected|actual|IMPLICIT|EXPLICIT|sqltype:|name:)).)*$', ''), ]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    test_sql = """
        set list on;
        set sqlda_display on;
        set bail off;

        select * from v_dual_nul_a;
        select * from v_dual_num_a;
        select * from v_dual_dts_a;
        select * from v_dual_boo_a;
        select * from v_dual_blb_a;
        select * from v_dual_txt_a;
        -------------------------------------
        select * from v_dual_nul_b;
        select * from v_dual_num_b;
        select * from v_dual_dts_b;
        select * from v_dual_boo_b;
        select * from v_dual_blb_b;

        -- Before fix this query raised
        -- SQLSTATE = 22001 / string right truncation / expected length 0, actual 3:
        select * from v_dual_txt_b; 
    """

    expected_stdout_5x = """
    """

    expected_stdout_6x = """
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 1 charset: 0 SYSTEM.NONE
        :  name: IMPLICIT_COL_NAME_FOR_NULL  alias: IMPLICIT_COL_NAME_FOR_NULL
        IMPLICIT_COL_NAME_FOR_NULL      <null>
        01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        :  name: IMPLICIT_COL_NAME_FOR_NUMERIC  alias: IMPLICIT_COL_NAME_FOR_NUMERIC
        IMPLICIT_COL_NAME_FOR_NUMERIC   123
        01: sqltype: 510 TIMESTAMP Nullable scale: 0 subtype: 0 len: 8
        :  name: IMPLICIT_COL_NAME_FOR_TIMESTAMP  alias: IMPLICIT_COL_NAME_FOR_TIMESTAMP
        IMPLICIT_COL_NAME_FOR_TIMESTAMP 2025-02-01 12:13:14.0000
        01: sqltype: 32764 BOOLEAN Nullable scale: 0 subtype: 0 len: 1
        :  name: IMPLICIT_COL_NAME_FOR_BOOLEAN  alias: IMPLICIT_COL_NAME_FOR_BOOLEAN
        IMPLICIT_COL_NAME_FOR_BOOLEAN   <true>
        01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
        :  name: IMPLICIT_COL_NAME_FOR_BLOB  alias: IMPLICIT_COL_NAME_FOR_BLOB
        IMPLICIT_COL_NAME_FOR_BLOB      0:1
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 12 charset: 4 SYSTEM.UTF8
        :  name: IMPLICIT_COL_NAME_FOR_CHAR  alias: IMPLICIT_COL_NAME_FOR_CHAR
        IMPLICIT_COL_NAME_FOR_CHAR      TXT
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 1 charset: 0 SYSTEM.NONE
        :  name: EXPLICIT_COL_NAME_FOR_NULL  alias: EXPLICIT_COL_NAME_FOR_NULL
        EXPLICIT_COL_NAME_FOR_NULL      <null>
        01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        :  name: EXPLICIT_COL_NAME_FOR_NUMERIC  alias: EXPLICIT_COL_NAME_FOR_NUMERIC
        EXPLICIT_COL_NAME_FOR_NUMERIC   123
        01: sqltype: 510 TIMESTAMP Nullable scale: 0 subtype: 0 len: 8
        :  name: EXPLICIT_COL_NAME_FOR_TIMESTAMP  alias: EXPLICIT_COL_NAME_FOR_TIMESTAMP
        EXPLICIT_COL_NAME_FOR_TIMESTAMP 2025-02-01 12:13:14.0000
        01: sqltype: 32764 BOOLEAN Nullable scale: 0 subtype: 0 len: 1
        :  name: EXPLICIT_COL_NAME_FOR_BOOLEAN  alias: EXPLICIT_COL_NAME_FOR_BOOLEAN
        EXPLICIT_COL_NAME_FOR_BOOLEAN   <true>
        01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
        :  name: EXPLICIT_COL_NAME_FOR_BLOB  alias: EXPLICIT_COL_NAME_FOR_BLOB
        EXPLICIT_COL_NAME_FOR_BLOB      0:3
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 12 charset: 4 SYSTEM.UTF8
        :  name: EXPLICIT_COL_NAME_FOR_CHAR  alias: EXPLICIT_COL_NAME_FOR_CHAR
        EXPLICIT_COL_NAME_FOR_CHAR      TXT
    """
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
