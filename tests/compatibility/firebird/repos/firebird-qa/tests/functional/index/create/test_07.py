#coding:utf-8

"""
ID:          index.create-07
TITLE:       CREATE INDEX - Multi column
DESCRIPTION:
FBTEST:      functional.index.create.07
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE t( a INTEGER, b INT, c INT, d INT);
commit;
"""

db = db_factory(init=init_script)

test_script = """CREATE INDEX test ON t(a,b,c,d);
SHOW INDEX test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST INDEX ON T(A, B, C, D)"""

@pytest.mark.skip("Covered by 'test_all_cases_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
