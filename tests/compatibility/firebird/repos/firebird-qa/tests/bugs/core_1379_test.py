#coding:utf-8

"""
ID:          issue-1797
ISSUE:       1797
TITLE:       Invalid parameter type when using it in CHAR_LENGTH function
DESCRIPTION:
JIRA:        CORE-1379
FBTEST:      bugs.core_1379
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns(r int) as
    begin
        execute statement ('select 1 from rdb$database where char_length(?) = 0') (1) into r;
        suspend;
    end
    ^
    execute block returns(r int) as
    begin
        execute statement ('select 1 from rdb$database where char_length(?) = 0') ('') into r;
        suspend;
    end
    ^
    execute block returns(r int) as
        declare c varchar(1) = '';
    begin
        execute statement ('select 1 from rdb$database where char_length(?) = 0') (c) into r;
        suspend;
    end
    ^
    set term ;^
    -- 05.05.2015:
    -- 1) changed min version to 2.5 (according to ticket header info; output in 2.5 and 3.0 now fully matches)
    -- 2) removed STDOUT (for the first ES);
    -- 3) changed expected STDERR: all three ES must now raise exception 'Data type unknown'.
"""

act = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+', '-At block line')])

expected_stderr = """
    Statement failed, SQLSTATE = HY004
    Dynamic SQL Error
    -SQL error code = -804
    -Data type unknown
    -At block line: 3, col: 9

    Statement failed, SQLSTATE = HY004
    Dynamic SQL Error
    -SQL error code = -804
    -Data type unknown
    -At block line: 3, col: 9

    Statement failed, SQLSTATE = HY004
    Dynamic SQL Error
    -SQL error code = -804
    -Data type unknown
    -At block line: 3, col: 9
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

