#coding:utf-8

"""
ID:          issue-4630
ISSUE:       4630
TITLE:       Fields present only in WHERE clause of views WITH CHECK OPTION causes invalid CHECK CONSTRAINT violation
DESCRIPTION:
JIRA:        CORE-4307
FBTEST:      bugs.core_4307
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t1 (n1 integer, n2 integer);
    insert into t1 values (1, 2);
    insert into t1 values (1, 3);
    insert into t1 values (1, 4);
    insert into t1 values (2, 2);
    insert into t1 values (2, 3);
    insert into t1 values (2, 4);
    insert into t1 values (3, 2);
    insert into t1 values (3, 3);
    insert into t1 values (3, 4);
    commit;
    recreate view v1 as select n1 from t1 where n1 < n2 with check option;
    commit;
    update v1 set n1 = n1 - 1;
"""

db = db_factory(init=init_script)

test_script = """
    update v1 set n1 = n1 - 1;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
