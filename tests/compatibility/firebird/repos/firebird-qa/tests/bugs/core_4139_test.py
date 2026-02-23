#coding:utf-8

"""
ID:          issue-4466
ISSUE:       4466
TITLE:       Error "invalid stream" can be raised in some cases while matching a computed index
DESCRIPTION:
JIRA:        CORE-4139
FBTEST:      bugs.core_4139
"""

import pytest
from firebird.qa import *

init_script = """create table A (ID int);
create table B (ID int);
create index IDX on A computed by (ID);
"""

db = db_factory(init=init_script)

test_script = """SET HEADING OFF;
select min( (select 1 from A where cast(ID as int) = B.ID) ) from B;
"""

act = isql_act('db', test_script)

expected_stdout = """<null>"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

