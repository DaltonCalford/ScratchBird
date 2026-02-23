#coding:utf-8

"""
ID:          table.create-07
TITLE:       CREATE TABLE - unknown datatype (domain)
DESCRIPTION:
FBTEST:      functional.table.create.07
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test(
     c1 unk_domain
    );
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_TABLE_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    TEST_DOMAIN_NAME = 'UNK_DOMAIN' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"UNK_DOMAIN"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE TABLE {TEST_TABLE_NAME} failed
        -SQL error code = -607
        -Invalid command
        -Specified domain or source column {TEST_DOMAIN_NAME} does not exist
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
