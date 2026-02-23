#coding:utf-8

"""
ID:          generator.drop-03
FBTEST:      functional.generator.drop.03
TITLE:       DROP GENERATOR - generator does not exists
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory()
act = isql_act('db', "DROP GENERATOR NO_SUCH_GEN;")

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_GEN_NAME = 'NO_SUCH_GEN' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"NO_SUCH_GEN"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP SEQUENCE {TEST_GEN_NAME} failed
        -generator {TEST_GEN_NAME} is not defined
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
