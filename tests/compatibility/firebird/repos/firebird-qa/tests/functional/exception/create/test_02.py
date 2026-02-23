#coding:utf-8

"""
ID:          exception.create-02
FBTEST:      functional.exception.create.02
TITLE:       CREATE EXCEPTION - try create Exception with the same name
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE EXCEPTION test 'A1';
    commit;
"""

db = db_factory(sql_dialect=3, init=init_script)

act = isql_act('db', "CREATE EXCEPTION test 'message to show';")


@pytest.mark.version('>=3.0')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_EXC_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE EXCEPTION {TEST_EXC_NAME} failed
        -Exception {TEST_EXC_NAME} already exists
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
