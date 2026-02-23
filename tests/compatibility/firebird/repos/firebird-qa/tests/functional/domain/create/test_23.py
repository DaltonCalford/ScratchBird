#coding:utf-8

"""
ID:          domain.create-23
FBTEST:      functional.domain.create.23
TITLE:       CREATE DOMAIN - NATIONAL CHAR
DESCRIPTION: Simple domain creation based NATIONAL CHAR datatype
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test NATIONAL CHAR(32767);
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            CHAR(32767) CHARACTER SET ISO8859_1 Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
