#coding:utf-8

"""
ID:          index.create-10
TITLE:       CREATE INDEX - try create index with same name
DESCRIPTION:
FBTEST:      functional.index.create.10
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE t( a INTEGER);
CREATE INDEX test ON t(a);
commit;
"""

db = db_factory(sql_dialect=3, init=init_script)

test_script = """CREATE INDEX test ON t(a);"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 42S11
unsuccessful metadata update
-CREATE INDEX TEST failed
-Index TEST already exists
"""

@pytest.mark.skip("Covered by 'test_all_cases_basic.py'")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
