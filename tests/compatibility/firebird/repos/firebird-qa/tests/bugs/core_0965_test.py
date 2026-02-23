#coding:utf-8

"""
ID:          issue-1368
ISSUE:       1368
TITLE:       Many aggregate functions within a single select list may cause a server crash
DESCRIPTION:
JIRA:        CORE-965
FBTEST:      bugs.core_0965
"""

import pytest
from firebird.qa import *

init_script = """create table tagg (col varchar(30000)) ;

insert into tagg (col) values ('0123456789') ;
commit ;
"""

db = db_factory(init=init_script)

test_script = """select 1 from (
  select col as f1, min(col) as f2, max(col) as f3
   from tagg
  group by 1
) ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

