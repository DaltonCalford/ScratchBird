#coding:utf-8

"""
ID:          issue-2532
ISSUE:       2532
TITLE:       View over global temporary table
DESCRIPTION:
JIRA:        CORE-2098
FBTEST:      bugs.core_2098
"""

import pytest
from firebird.qa import *

init_script = """create global temporary table temptable (
 id integer);
commit;
"""

db = db_factory(init=init_script)

test_script = """recreate view tempview1
as
select
 a.id as id
from
 temptable a;
commit;
recreate view tempview2
as
select
 a.id + 1 as id
from
 temptable a;
commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
