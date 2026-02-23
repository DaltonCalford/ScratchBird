#coding:utf-8

"""
ID:          issue-2690
ISSUE:       2690
TITLE:       ALTER DOMAIN with dependencies may leave a transaction handle in inconsistent state causing segmentation faults
DESCRIPTION:
JIRA:        CORE-2264
FBTEST:      bugs.core_2264
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain d int;
    set term ^;
    create or alter procedure p1 as
      declare v d;
    begin
      v = v + v;
    end
    ^
    set term ;^
    commit;
    alter domain d type varchar(11);
    alter domain d type varchar(11); -- segmentation fault here
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
