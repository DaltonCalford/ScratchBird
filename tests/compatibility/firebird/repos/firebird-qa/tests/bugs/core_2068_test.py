#coding:utf-8

"""
ID:          issue-2504
ISSUE:       2504
TITLE:       Comparision with IN and subquery with RDB$DB_KEY returns wrong result
DESCRIPTION:
JIRA:        CORE-2068
FBTEST:      bugs.core_2068
"""

import pytest
from firebird.qa import *

init_script = """create table t1 (n integer);

insert into t1 values (1);
insert into t1 values (2);
insert into t1 values (3);
commit;
"""

db = db_factory(init=init_script)

test_script = """select a.*
 from t1 a
 where a.rdb$db_key = (
 select b.rdb$db_key
 from t1 b
 where n = 1
);

select a.*
 from t1 a
 where a.rdb$db_key in (
 select b.rdb$db_key
 from t1 b
 where n = 1
);
"""

act = isql_act('db', test_script)

expected_stdout = """
           N
============
           1


           N
============
           1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

