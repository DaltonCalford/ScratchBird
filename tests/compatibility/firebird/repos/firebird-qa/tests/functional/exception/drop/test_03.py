#coding:utf-8

"""
ID:          exception.drop-03
FBTEST:      functional.exception.drop.03
TITLE:       DROP EXCEPTION - that doesn't exists
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
DROP EXCEPTION no_such_exc;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_EXC_NAME = 'NO_SUCH_EXC' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"NO_SUCH_EXC"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP EXCEPTION {TEST_EXC_NAME} failed
        -Exception not found
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
