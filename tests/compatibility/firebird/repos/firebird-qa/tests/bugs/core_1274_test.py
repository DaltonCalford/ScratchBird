#coding:utf-8

"""
ID:          issue-1695
ISSUE:       1695
TITLE:       Wrong results when PLAN MERGE is chosen and datatypes of the equality predicate arguments are different
DESCRIPTION:
JIRA:        CORE-1274
FBTEST:      bugs.core_1274
"""

import pytest
from firebird.qa import *

init_script = """create table t1 (col1 int);
create table t2 (col2 varchar(10));
commit;

insert into t1 values (100);
insert into t1 values (20);
insert into t1 values (3);
commit;

insert into t2 values ('100');
insert into t2 values ('20');
insert into t2 values ('3');
commit;"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """select * from t1 join t2 on col1 = col2 ORDER by 1 DESC;
"""

act = isql_act('db', test_script)

expected_stdout = """
        COL1 COL2
============ ==========
         100 100
          20 20
           3 3

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

