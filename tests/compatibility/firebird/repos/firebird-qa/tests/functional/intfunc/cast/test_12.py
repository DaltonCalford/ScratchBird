#coding:utf-8

"""
ID:          intfunc.cast-12
TITLE:       CAST CHAR -> TIME
DESCRIPTION:
  Be careful about time format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.12
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST('9:11:60' AS TIME) FROM rdb$Database;")

expected_stdout = """
CAST
=============
"""

expected_stderr = """Statement failed, SQLSTATE = 22018

conversion error from string "9:11:60"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
