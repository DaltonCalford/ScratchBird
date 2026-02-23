#coding:utf-8

"""
ID:          issue-6494
ISSUE:       6494
TITLE:       Regression: crash when built-in function LEFT() or RIGHT() missed 2nd argument
  (number of characters to be taken)
DESCRIPTION:
JIRA:        CORE-6251
FBTEST:      bugs.core_6251
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test( s varchar(10) );
    commit;
    insert into test(s) values('1');
    select 1 from test f where right( f.s ) = '1';
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 39000
    function RIGHT could not be matched
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
