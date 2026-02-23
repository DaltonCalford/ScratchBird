#coding:utf-8

"""
ID:          generator.drop-02
FBTEST:      functional.generator.drop.02
TITLE:       DROP GENERATOR - in use
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    create generator gen_test;
    set term ^;
    create procedure sp_test as
    declare variable id int;
    begin
        id = gen_id(gen_test,1);
    end ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = isql_act('db', "drop generator gen_test;")

@pytest.mark.version('>=3')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_GEN_NAME = 'GEN_TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"GEN_TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -GENERATOR {TEST_GEN_NAME}
        -there are 1 dependencies
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
