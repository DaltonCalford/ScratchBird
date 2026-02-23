#coding:utf-8

"""
ID:          issue-2503
ISSUE:       2503
TITLE:       GROUP by and RDB$DB_KEY problems
DESCRIPTION:
JIRA:        CORE-2067
FBTEST:      bugs.core_2067
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

test_script = """-- First problem: it should be invalid to group by n and select rdb$db_key
select n, rdb$db_key from t1 group by n;

-- Second problem: error: column unknown DB_KEY is wrong raised
select n, rdb$db_key from t1 group by 1, 2;

-- Third problem: wrong values for rdb$db_key are returned
select n, t1.rdb$db_key from t1 group by 1, 2;
"""

act = isql_act('db', test_script)

expected_stdout = """
           N DB_KEY
============ ================
           1 8000000001000000
           2 8000000002000000
           3 8000000003000000


           N DB_KEY
============ ================
           1 8000000001000000
           2 8000000002000000
           3 8000000003000000
"""

expected_stderr = """Statement failed, SQLSTATE = 42000
Dynamic SQL Error
-SQL error code = -104
-Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

