#coding:utf-8

"""
ID:          issue-4098
ISSUE:       4098
TITLE:       SIMILAR TO works wrongly
DESCRIPTION:
JIRA:        CORE-3754
FBTEST:      bugs.core_3754
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test_a(id integer, cnt integer);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set heading off;
    select iif( '1' similar to '(1|2){0,}', 1, 0)as result from rdb$database
    union all
    select iif( '1' similar to '(1|2){0,1}', 1, 0)from rdb$database
    union all
    select iif( '1' similar to '(1|2){1}', 1, 0)from rdb$database
    union all
    select iif( '123' similar to '(1|12[3]?){1}', 1, 0)from rdb$database
    union all
    select iif( '123' similar to '(1|12[3]?)+', 1, 0) from rdb$database
    union all
    select iif( 'qwertyqwertyqwe' similar to '(%qwe%){2,}', 1, 0) from rdb$database
    union all
    select iif( 'qwertyqwertyqwe' similar to '(%(qwe)%){2,}', 1, 0) from rdb$database
    union all
    select iif( 'qqqqqqqqqqqqqqq' similar to '(%q%){2,}', 1, 0) from rdb$database
    ;
    -- BTW: result in WI-T3.0.0.31681 matches to Postgress 9.3, checked 24.02.2015
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
    1
    1
    1
    1
    1
    1
    1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

