#coding:utf-8

"""
ID:          new-database-20
TITLE:       New DB - RDB$PROCEDURES content
DESCRIPTION: Check the correct content of RDB$PROCEDURES in new database.
FBTEST:      functional.basic.db.20
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
     set count on;
     set list on;
     set blob all;
     select p.*
     from rdb$procedures p
     order by p.rdb$procedure_id;
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
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PROCEDURE_ID                1
    RDB$PROCEDURE_INPUTS            3
    RDB$PROCEDURE_OUTPUTS           5
    RDB$DESCRIPTION                 <null>
    RDB$PROCEDURE_SOURCE            <null>
    RDB$PROCEDURE_BLR               <null>
    RDB$SECURITY_CLASS              <null>
    RDB$OWNER_NAME                  SYSDBA
    RDB$RUNTIME                     <null>
    RDB$SYSTEM_FLAG                 1
    RDB$PROCEDURE_TYPE              1
    RDB$VALID_BLR                   1
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 SYSTEM
    RDB$ENTRYPOINT                  <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL
    RDB$PRIVATE_FLAG                0
    RDB$SQL_SECURITY                <null>

    Records affected: 1
"""

@pytest.mark.version('>=4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
