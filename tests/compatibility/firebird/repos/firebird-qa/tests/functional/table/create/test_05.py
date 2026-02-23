#coding:utf-8

"""
ID:          table.create-05
TITLE:       CREATE TABLE - create table with same name
DESCRIPTION:
FBTEST:      functional.table.create.05
NOTES:
    [12.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test(
     c1 smallint
    );
    commit;

    create table test(
     c1 smallint,
     c2 integer
    );
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_TABLE_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42S01
        unsuccessful metadata update
        -CREATE TABLE {TEST_TABLE_NAME} failed
        -Table {TEST_TABLE_NAME} already exists
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
