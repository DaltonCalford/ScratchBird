#coding:utf-8

"""
ID:          issue-1274
ISSUE:       1274
TITLE:       Singleton isn't respected in COMPUTED BY expressions
DESCRIPTION:
JIRA:        CORE-881
FBTEST:      bugs.core_0881
"""

import pytest
from firebird.qa import *

init_script = """create table t1 (n integer);
create table t2 (n integer, c computed by ((select n from t1)));

insert into t1 values (1);
insert into t1 values (2);
insert into t2 values (1);
commit;
"""

db = db_factory(init=init_script)

test_script = """select * from t2;
"""

act = isql_act('db', test_script)

expected_stdout = """N            C
============ ============
"""

expected_stderr = """Statement failed, SQLSTATE = 21000
multiple rows in singleton select
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

