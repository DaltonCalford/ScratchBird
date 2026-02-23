#coding:utf-8

"""
ID:          issue-2843
ISSUE:       2843
TITLE:       ALTER VIEW doesn't clear dependencies on old views
DESCRIPTION:
JIRA:        CORE-2427
FBTEST:      bugs.core_2427
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """create view v1 (n) as select 'ABC' from rdb$database;
create view v3 (n) as select substring(lower(n) from 1) from v1;
create view newv (n) as select 'XYZ' from rdb$database;
alter view v3 (n) as select substring(lower(n) from 1) from newv;
drop view v1;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
