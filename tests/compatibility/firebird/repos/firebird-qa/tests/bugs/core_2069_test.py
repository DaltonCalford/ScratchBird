#coding:utf-8

"""
ID:          issue-2505
ISSUE:       2505
TITLE:       Incorrect VIEW expansion when RDB$DB_KEY is used in view body
DESCRIPTION:
JIRA:        CORE-2069
FBTEST:      bugs.core_2069
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

test_script = """-- ok
select a.*
 from t1 a
 where a.rdb$db_key = (
 select b.rdb$db_key
 from t1 b
 where n = 1
);

-- ok
create view v1 as
 select a.*
 from t1 a
 where a.rdb$db_key = (
 select b.rdb$db_key
 from t1 b
 where n = 1
);

-- ok
select * from v1;

-- wrong: returns nothing
select * from v1 union all select * from v1;
"""

act = isql_act('db', test_script)

expected_stdout = """
           N
============
           1


           N
============
           1


           N
============
           1
           1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

