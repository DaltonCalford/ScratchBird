#coding:utf-8

"""
ID:          issue-2871
ISSUE:       2871
TITLE:       UNICODE_CI internal gds software consistency check
DESCRIPTION:
JIRA:        CORE-2457
FBTEST:      bugs.core_2457
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE ATABLE (
    AFIELD VARCHAR(50) CHARACTER SET UTF8 COLLATE UNICODE_CI);
CREATE DESCENDING INDEX ATABLE_BWD ON ATABLE (AFIELD);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """SELECT FIRST 1 T.AFIELD FROM ATABLE T
  WHERE T.AFIELD < 'X'
  ORDER BY T.AFIELD DESC;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
