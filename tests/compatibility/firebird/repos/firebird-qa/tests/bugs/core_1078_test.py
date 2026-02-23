#coding:utf-8

"""
ID:          issue-649
ISSUE:       649
TITLE:       View with equally named source fields not faisible
DESCRIPTION:
JIRA:        CORE-1078
FBTEST:      bugs.core_1078
"""

import pytest
from firebird.qa import *

init_script = """create table t1(id integer, field1 integer);
create table t2(id integer, field1 integer);
"""

db = db_factory(init=init_script)

test_script = """create view view1 as select t1.field1 as t1f1, t2.field1 as t2f1 from t1 JOIN t2 on t2.id = t1.id;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
