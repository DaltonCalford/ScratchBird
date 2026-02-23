#coding:utf-8

"""
ID:          new-database-25
TITLE:       New DB - RDB$ROLES content
DESCRIPTION: Check the correct content of RDB$ROLES in new database.
FBTEST:      functional.basic.db.25
NOTES:
[17.01.2023] pzotov
    DISABLED after discussion with dimitr, letters 17-sep-2022 11:23.
    Reasons:
        * There is no much sense to keep such tests because they fails extremely often during new major FB developing.
        * There is no chanse to get successful outcome for the whole test suite is some of system table became invalid,
          i.e. lot of other tests will be failed in such case.
    Single test for check DDL (type of columns, their order and total number) will be implemented for all RDB-tables.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    -- NB: rdb$role_name is UNIQUE column.
    select * from rdb$roles order by rdb$role_name;
"""

act = isql_act('db', test_script,
               substitutions=[('RDB\\$SECURITY_CLASS\\s+SQL.*', 'RDB\\$SECURITY_CLASS SQL'),
                              ('[\t ]+', ' ')])

# version: 3.0

expected_stdout_1 = """
    RDB$ROLE_NAME                   RDB$ADMIN
    RDB$OWNER_NAME                  SYSDBA
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$SECURITY_CLASS              SQL$162

    Records affected: 1
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
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
    RDB$SECURITY_CLASS              SQL$383
    RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF

    Records affected: 1
"""

@pytest.mark.version('>=4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
