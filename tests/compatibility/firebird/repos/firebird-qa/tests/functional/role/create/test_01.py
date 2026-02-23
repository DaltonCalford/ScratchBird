#coding:utf-8

"""
ID:          role.create-01
TITLE:       CREATE ROLE
DESCRIPTION:
FBTEST:      functional.role.create.01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create role test;
    commit;
    set list on;
    select * from rdb$roles order by rdb$role_name;
"""

act = isql_act('db', test_script, substitutions=[('SQL\\$.*', 'SQLnnnn')])

# version: 3.0

expected_stdout_1 = """
    RDB$ROLE_NAME                   RDB$ADMIN
    RDB$OWNER_NAME                  SYSDBA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$SECURITY_CLASS              SQLnnnn

    RDB$ROLE_NAME                   TEST
    RDB$OWNER_NAME                  SYSDBA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 0
    RDB$SECURITY_CLASS              SQLnnnn
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    RDB$ROLE_NAME                   RDB$ADMIN
    RDB$OWNER_NAME                  SYSDBA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$SECURITY_CLASS              SQLnnnn
    RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF

    RDB$ROLE_NAME                   TEST
    RDB$OWNER_NAME                  SYSDBA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 0
    RDB$SECURITY_CLASS              SQLnnnn
    RDB$SYSTEM_PRIVILEGES           0000000000000000
"""

@pytest.mark.version('>=4.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
