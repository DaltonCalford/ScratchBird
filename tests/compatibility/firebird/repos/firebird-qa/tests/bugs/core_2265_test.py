#coding:utf-8

"""
ID:          issue-2691
ISSUE:       2691
TITLE:       Grouping by function doesn't work properly
DESCRIPTION:
JIRA:        CORE-2265
FBTEST:      bugs.core_2265
"""

import pytest
from firebird.qa import *

init_script = """create table t (col1 date, col2 int);
commit;

insert into t values ('2011-01-01', 1);
commit;
"""

db = db_factory(init=init_script)

test_script = """select extract(year from col1), sum(col2)
from t
group by extract(year from col1);

select extract(year from col1), sum(col2)
from t
group by 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
EXTRACT                   SUM
======= =====================
   2011                     1


EXTRACT                   SUM
======= =====================
   2011                     1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

