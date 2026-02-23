#coding:utf-8

"""
ID:          issue-4149
ISSUE:       4149
TITLE:       Wrong data returned if a sub-query or a computed field refers to the base table in the ORDER BY clause
DESCRIPTION:
JIRA:        CORE-3806
FBTEST:      bugs.core_3806
"""

import pytest
from firebird.qa import *

init_script = """create table t (col1 int, col2 int, col3 int);
insert into t values (100, 200, 300);
insert into t values (101, 201, 301);
insert into t values (102, 202, 302);
commit;
"""

db = db_factory(init=init_script)

test_script = """alter table t drop col1;
select col2, col3 from t as t1;
select col2, col3 from t as t1 where exists (select * from t as t2 order by t1.col2 );
"""

act = isql_act('db', test_script)

expected_stdout = """
        COL2         COL3
============ ============
         200          300
         201          301
         202          302

        COL2         COL3
============ ============
         200          300
         201          301
         202          302
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

