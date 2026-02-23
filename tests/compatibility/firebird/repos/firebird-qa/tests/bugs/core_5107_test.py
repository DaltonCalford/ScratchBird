#coding:utf-8

"""
ID:          issue-5391
ISSUE:       5391
TITLE:       set autoddl off and sequence of: ( create view V as select * from T; alter
  view V as select 1 x from rdb$database; drop view V; ) leads to server crash
DESCRIPTION:
JIRA:        CORE-5107
FBTEST:      bugs.core_5107
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    commit;
    recreate table test(id int, x int);
    create view v_test as select * from test;
    alter view v_test as select 1 id from rdb$database;
    drop view v_test;
    commit;
    set list on;
    select 'Done' as msg from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             Done
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

