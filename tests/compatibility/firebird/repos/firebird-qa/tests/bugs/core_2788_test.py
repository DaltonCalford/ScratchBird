#coding:utf-8

"""
ID:          issue-3179
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3179
TITLE:       ISQL extracts the array dimensions after the character set name rather than after datatype and its length
DESCRIPTION:
JIRA:        CORE-2788
FBTEST:      bugs.core_2788
NOTES:
    [05.10.2023] pzotov
    Removed SHOW command for check result because its output often changes. Query to RDB$ tables is used instead.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('- line .*', ''), ('At line .*', '')])

sql_ddl = """
    -- DO NOT 'set bail on' here!
    create domain dm_test as char(1) character set iso8859_1[11:23];
    create domain dm_test as char(1)[11:23] character set iso8859_1;
    commit;
"""

sql_chk = """
    set list on;
    select f.rdb$field_name, f.rdb$field_length, f.rdb$dimensions, d.rdb$lower_bound, d.rdb$upper_bound
    from rdb$fields f
    join rdb$field_dimensions d on f.rdb$field_name = d.rdb$field_name
    where f.rdb$field_name = 'DM_TEST';
"""

expected_ddl_stdout = """
    RDB$FIELD_NAME                  DM_TEST
    RDB$FIELD_LENGTH                1
    RDB$DIMENSIONS                  1
    RDB$LOWER_BOUND                 11
    RDB$UPPER_BOUND                 23
"""

expected_ddl_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 57
    -[
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    # Apply initial DDL and check domain info:
    #
    act.expected_stderr = expected_ddl_stderr
    act.expected_stdout = expected_ddl_stdout
    act.isql(switches=[], input = '\n'.join((sql_ddl, sql_chk)) )
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
    act.reset()

    #---------------------------------------------------------
    # Extract metadata:
    #
    act.isql(switches=['-x'])
    xmeta = act.stdout
    #
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('drop domain dm_test')
        con.commit()
    #---------------------------------------------------------
    # Attempt to apply extracted metadata. No errors must occur:
    act.isql(switches=[], input=xmeta)
    #
    act.reset()

    #---------------------------------------------------------
    # Extract domain info again. Result must be the same as above:
    #
    act.expected_stdout = expected_ddl_stdout
    act.isql(switches=['-q'], input= sql_chk)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

