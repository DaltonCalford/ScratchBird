#coding:utf-8

"""
ID:          intfunc.cast-13
TITLE:       CAST CHAR -> TIMESTAM
DESCRIPTION:
  Be careful about time format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.13
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST('10.2.1489 14:34:59.1234' AS TIMESTAMP) FROM rdb$Database;")

expected_stdout = """
CAST
=========================

1489-02-10 14:34:59.1234
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
