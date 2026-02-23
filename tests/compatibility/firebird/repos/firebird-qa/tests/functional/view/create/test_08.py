#coding:utf-8

"""
ID:          view.create-08
TITLE:       CREATE VIEW - updateable WITH CHECK OPTION
DESCRIPTION:
FBTEST:      functional.view.create.08
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test(id int);
    create view v_test (id) as select id from test where id<10 with check option;
    insert into v_test values(10);
"""

act = isql_act('db', test_script, substitutions=[('-At trigger.*', '')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_VEW_NAME = "V_TEST" if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"V_TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table {TEST_VEW_NAME}
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
