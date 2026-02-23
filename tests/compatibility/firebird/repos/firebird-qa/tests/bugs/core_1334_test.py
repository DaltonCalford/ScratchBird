#coding:utf-8

"""
ID:          issue-1753
ISSUE:       1753
TITLE:       Joins with NULL RDB$DB_KEY crash the server
DESCRIPTION:
JIRA:        CORE-1334
FBTEST:      bugs.core_1334
"""

import pytest
from firebird.qa import *

init_script = """create table t1 (id integer primary key);
create table t2 (id integer references t1);
COMMIT;
insert into t1 values (1);
insert into t1 values (2);
insert into t2 values (2);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """select *
  from t1
  left join t2
    on (t2.id = t1.id)
  left join t2 t3
    on (t3.rdb$db_key = t2.rdb$db_key);
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID           ID           ID
============ ============ ============
           1       <null>       <null>
           2            2            2

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

