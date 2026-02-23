#coding:utf-8

"""
ID:          intfunc.cast-05
TITLE:       CAST Numeric -> Numeric (Round up)
DESCRIPTION:
FBTEST:      functional.intfunc.cast.05
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(1.25001 AS NUMERIC(2,1)) FROM rdb$Database;")

expected_stdout = """
CAST
=======

1.3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
