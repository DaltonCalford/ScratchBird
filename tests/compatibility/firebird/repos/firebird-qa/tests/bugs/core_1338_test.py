#coding:utf-8

"""
ID:          issue-1757
ISSUE:       1757
TITLE:       Problem with view, computed field and functions
DESCRIPTION:
  Original ticket name: 335544721 when selecting view with round
JIRA:        CORE-1338
FBTEST:      bugs.core_1338
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """create table a (a numeric(15,15));

insert into a values(2);

create view b(a) as select round(a,2) from a;

select * from b;
"""

act = isql_act('db', test_script)

expected_stdout = """
                    A
=====================
    2.000000000000000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

