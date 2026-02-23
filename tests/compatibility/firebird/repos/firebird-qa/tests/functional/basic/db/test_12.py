#coding:utf-8

"""
ID:          new-database-12
TITLE:       New DB - RDB$FUNCTIONS content
DESCRIPTION: Check for correct content of RDB$FUNCTIONS in a new database.
FBTEST:      functional.basic.db.12
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
    select *
    from rdb$functions rf
    order by rdb$engine_name, rf.rdb$package_name, rf.rdb$function_name, rdb$module_name, rdb$entrypoint;
"""

act = isql_act('db', test_script)

# version: 3.0

expected_stdout_1 = """
    Records affected: 0
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    RDB$FUNCTION_NAME               DATABASE_VERSION
    RDB$FUNCTION_TYPE               <null>
    RDB$QUERY_NAME                  <null>
    RDB$DESCRIPTION                 <null>
    RDB$MODULE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$RETURN_ARGUMENT             0
    RDB$SYSTEM_FLAG                 1
    RDB$ENGINE_NAME                 SYSTEM
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL
    RDB$PRIVATE_FLAG                0
    RDB$FUNCTION_SOURCE             <null>
    RDB$FUNCTION_ID                 1
    RDB$FUNCTION_BLR                <null>
    RDB$VALID_BLR                   1
    RDB$DEBUG_INFO                  <null>
    RDB$SECURITY_CLASS              <null>
    RDB$OWNER_NAME                  SYSDBA
    RDB$LEGACY_FLAG                 <null>
    RDB$DETERMINISTIC_FLAG          <null>
    RDB$SQL_SECURITY                <null>

    Records affected: 1
"""

@pytest.mark.version('>=4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
