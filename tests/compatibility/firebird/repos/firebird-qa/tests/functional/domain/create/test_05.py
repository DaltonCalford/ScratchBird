#coding:utf-8

"""
ID:          domain.create-05
FBTEST:      functional.domain.create.05
TITLE:       CREATE DOMAIN - DOUBLE PRECISION
DESCRIPTION: Simple domain creation based DOUBLE PRECISION datatype
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test DOUBLE PRECISION;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            DOUBLE PRECISION Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
