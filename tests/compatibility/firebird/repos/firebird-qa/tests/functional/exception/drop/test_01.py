#coding:utf-8

"""
ID:          exception.drop-01
FBTEST:      functional.exception.drop.01
TITLE:       DROP EXCEPTION
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """CREATE EXCEPTION test 'message to show';
commit;"""

db = db_factory(init=init_script)

test_script = """DROP EXCEPTION test;
SHOW EXCEPTION test;"""

act = isql_act('db', test_script)

expected_stderr = """There is no exception TEST in this database"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
