#coding:utf-8

"""
ID:          domain.create-39
FBTEST:      functional.domain.create.39
TITLE:       CREATE DOMAIN - COLLATE
DESCRIPTION: Domain creation based on VARCHAR datatype with COLLATE specification
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test VARCHAR(32) CHARACTER SET DOS437 COLLATE DB_ITA437;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            VARCHAR(32) CHARACTER SET DOS437 Nullable
COLLATE DB_ITA437"""

@pytest.mark.skip("Covered by 'bugs/core_6336_test.py' and 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
