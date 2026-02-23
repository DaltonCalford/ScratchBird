#coding:utf-8

"""
ID:          domain.create-07
FBTEST:      functional.domain.create.07
TITLE:       CREATE DOMAIN - DATE
DESCRIPTION: Simple domain creation based DATE datatype
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test DATE;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            DATE Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
