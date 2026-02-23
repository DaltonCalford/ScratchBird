#coding:utf-8

"""
ID:          new-database-19
TITLE:       New DB - RDB$PROCEDURE_PARAMETERS content
DESCRIPTION: Check the correct content of RDB$PROCEDURE_PARAMETERS in new database.
FBTEST:      functional.basic.db.19
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
    from rdb$procedure_parameters
    order by rdb$procedure_name,rdb$parameter_name,rdb$parameter_number;
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

    RDB$PARAMETER_NAME              RDB$DST_OFFSET
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            3
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$TIME_ZONE_OFFSET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$EFFECTIVE_OFFSET
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            4
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$TIME_ZONE_OFFSET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$END_TIMESTAMP
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            1
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$TIMESTAMP_TZ
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$FROM_TIMESTAMP
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            1
    RDB$PARAMETER_TYPE              0
    RDB$FIELD_SOURCE                RDB$TIMESTAMP_TZ
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$START_TIMESTAMP
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            0
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$TIMESTAMP_TZ
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$TIME_ZONE_NAME
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            0
    RDB$PARAMETER_TYPE              0
    RDB$FIELD_SOURCE                RDB$TIME_ZONE_NAME
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$TO_TIMESTAMP
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            2
    RDB$PARAMETER_TYPE              0
    RDB$FIELD_SOURCE                RDB$TIMESTAMP_TZ
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    RDB$PARAMETER_NAME              RDB$ZONE_OFFSET
    RDB$PROCEDURE_NAME              TRANSITIONS
    RDB$PARAMETER_NUMBER            2
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$TIME_ZONE_OFFSET
    RDB$DESCRIPTION                 <null>
    RDB$SYSTEM_FLAG                 1
    RDB$DEFAULT_VALUE               <null>
    RDB$DEFAULT_SOURCE              <null>
    RDB$COLLATION_ID                <null>
    RDB$NULL_FLAG                   1
    RDB$PARAMETER_MECHANISM         0
    RDB$FIELD_NAME                  <null>
    RDB$RELATION_NAME               <null>
    RDB$PACKAGE_NAME                RDB$TIME_ZONE_UTIL

    Records affected: 8
"""

@pytest.mark.version('>=4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
