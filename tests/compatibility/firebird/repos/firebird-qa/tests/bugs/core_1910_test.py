#coding:utf-8

"""
ID:          issue-2341
ISSUE:       2341
TITLE:       Not valid fields in MERGE's insert clause are allowed
DESCRIPTION:
JIRA:        CORE-1910
FBTEST:      bugs.core_1910
"""

import pytest
from firebird.qa import *

init_script = """create table t1 (n integer, x integer);
create table t2 (a integer, b integer);
"""

db = db_factory(init=init_script)

test_script = """merge into t1 t1
  using (select * from t2) t2
    on t1.n = t2.a
    when matched then
      update set x = t2.b
    when not matched then
      insert (a, x) values (1, 2); -- "a" is not a field of t1
"""

act = isql_act('db', test_script, substitutions=[('column.*', '')])

expected_stderr = """Statement failed, SQLSTATE = 42S22
Dynamic SQL Error
-SQL error code = -206
-Column unknown
-A
-At line 7, column 15
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

