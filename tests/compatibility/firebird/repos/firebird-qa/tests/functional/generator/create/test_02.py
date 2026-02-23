#coding:utf-8

"""
ID:          generator.create-02
FBTEST:      functional.generator.create.02
TITLE:       CREATE GENERATOR - try create gen with same name
DESCRIPTION:
"""
import pytest
from firebird.qa import *

init_script = """
    create generator gen_test;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    create generator gen_test;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_GEN_NAME = 'GEN_TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"GEN_TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE SEQUENCE {TEST_GEN_NAME} failed
        -Sequence {TEST_GEN_NAME} already exists
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
