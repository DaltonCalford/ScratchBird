#coding:utf-8

"""
ID:          issue-1333
ISSUE:       1333
TITLE:       Comment in create database
DESCRIPTION: Accept comment in Create database
JIRA:        CORE-932
FBTEST:      bugs.core_0932
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    commit;
    create database /* foo */ '$(DATABASE_LOCATION)tmp_c0932_1.fdb';
    select iif( m.mon$database_name containing 'tmp_c0932_1', 'OK', 'FAIL' ) as result_1 from mon$database m;
    commit;
    drop database;
    create database /*/**//**/'$(DATABASE_LOCATION)tmp_c0932_2.fdb'/*/**//**//**//*/**/;
    select iif( m.mon$database_name containing 'tmp_c0932_2', 'OK', 'FAIL' ) as result_2 from mon$database m;
    commit;
    drop database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT_1                        OK
    RESULT_2                        OK
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

