#coding:utf-8

"""
ID:          issue-7786
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7786
TITLE:       Regression: ISQL extracts metadata with non-compilable 'COLLATE' clause if table has column with charset that differ from DB charset
DESCRIPTION:
NOTES:
    [09.10.2023] pzotov
    Skip usage of SHOW command. It is enough to check that extracted metadata can be compiled w/o errors.
    Checked on 6.0.0.75.
"""

import pytest
from firebird.qa import *

init_sql = """
    set bail on;
    recreate table test (
         id int
        ,c2 char(1) character set win1251 computed by ('Ы')
    );
"""

db = db_factory(charset = 'utf8', init = init_sql)

act = python_act('db')

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    act.isql(switches=['-x'])
    init_metadata = act.stdout
    assert act.clean_stderr == '' # no errors must occur while extracting extracted metadata
    act.reset()
    #------------------------------
    drop_sql = """
        drop table test;
        commit;
    """
    act.isql(switches = ['-q'], input = drop_sql, combine_output = True)
    assert act.clean_stdout == '' # no errors must occur when drop previously created table and domain
    act.reset()
    #------------------------------
    # Apply extracted metadata
    act.isql(switches = ['-q'], input = init_metadata, combine_output = True)
    assert act.clean_stdout == '' # no errors must occur while applying script with extracted metadata
    act.reset()
