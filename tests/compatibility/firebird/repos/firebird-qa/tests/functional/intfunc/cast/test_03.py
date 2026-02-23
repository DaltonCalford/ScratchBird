#coding:utf-8

"""
ID:          intfunc.cast-03
TITLE:       CAST Numeric -> DATE
DESCRIPTION: Convert from number to date is not (yet) supported
FBTEST:      functional.intfunc.cast.03
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(CAST(1.25001 AS INT) AS DATE) FROM rdb$Database;")

expected_stdout = """CAST
===========
"""

expected_stderr = """Statement failed, SQLSTATE = 22018

conversion error from string "1"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
