#coding:utf-8

"""
ID:          domain.create-02
FBTEST:      functional.domain.create.02
TITLE:       CREATE DOMAIN - INTEGER
DESCRIPTION: Simple domain creation based INTEGER datatype
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test INTEGER;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            INTEGER Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=1.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
