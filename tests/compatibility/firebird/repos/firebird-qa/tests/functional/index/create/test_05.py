#coding:utf-8

"""
ID:          index.create-05
TITLE:       CREATE DESC INDEX
DESCRIPTION:
FBTEST:      functional.index.create.05
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE t( a INTEGER);"""

db = db_factory(init=init_script)

test_script = """CREATE DESC INDEX test ON t(a);
SHOW INDEX test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST DESCENDING INDEX ON T(A)"""

@pytest.mark.skip("Covered by 'test_all_cases_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
