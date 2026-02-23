#coding:utf-8

"""
ID:          issue-1439
ISSUE:       1439
TITLE:       Server crashes at runtime when an explicit MERGE plan is specified over a few JOIN ones
DESCRIPTION:
JIRA:        CORE-1025
FBTEST:      bugs.core_1025
"""

import pytest
from firebird.qa import *

init_script = """recreate table t (id int not null);
alter table t add constraint pk primary key (id);

insert into t values (1);
commit;
"""

db = db_factory(init=init_script)

test_script = """select *
from t t1, t t2, t t3, t t4
where t1.id = t2.id
  and t2.id = t3.id
  and t3.id = t4.id
PLAN MERGE (JOIN (T1 NATURAL, T2 INDEX (PK)), JOIN(T3 NATURAL, T4 INDEX (PK)));

"""

act = isql_act('db', test_script)

expected_stdout = """ID           ID           ID           ID
============ ============ ============ ============
           1            1            1            1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

