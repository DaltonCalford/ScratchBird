#coding:utf-8

"""
ID:          generator.drop-01
FBTEST:      functional.generator.drop.01
TITLE:       DROP GENERATOR
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """CREATE GENERATOR test;
commit;"""

db = db_factory(init=init_script)

test_script = """DROP GENERATOR test;
SHOW GENERATOR TEST;"""

act = isql_act('db', test_script)

@pytest.mark.version('>=1.0')
def test_1(act: Action):
    act.expected_stderr = "There is no generator TEST in this database"
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
