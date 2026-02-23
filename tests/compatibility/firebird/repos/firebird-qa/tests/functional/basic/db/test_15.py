#coding:utf-8

"""
ID:          new-database-15
TITLE:       New DB - RDB$INDEX_SEGMENTS content
DESCRIPTION: Check the correct content of RDB$INDEX_SEGMENTS in new database.
FBTEST:      functional.basic.db.15
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
    select rs.*
    from rdb$index_segments rs
    order by lpad( trim(replace(rdb$index_name, 'RDB$INDEX_', '')),31,'0'), rdb$field_name;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$INDEX_NAME[\\s]+RDB\\$INDEX.*',
                                                  'RDB\\$INDEX_NAME RDB\\$INDEX')])

# version: 3.0

expected_stdout_1 = """
    RDB$INDEX_NAME                  RDB$INDEX_0
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_1
    RDB$FIELD_NAME                  RDB$RELATION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_2
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_3
    RDB$FIELD_NAME                  RDB$FIELD_SOURCE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_4
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_5
    RDB$FIELD_NAME                  RDB$INDEX_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_6
    RDB$FIELD_NAME                  RDB$INDEX_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_7
    RDB$FIELD_NAME                  RDB$SECURITY_CLASS
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_8
    RDB$FIELD_NAME                  RDB$TRIGGER_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_9
    RDB$FIELD_NAME                  RDB$FUNCTION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_9
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_10
    RDB$FIELD_NAME                  RDB$FUNCTION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_10
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_11
    RDB$FIELD_NAME                  RDB$GENERATOR_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_12
    RDB$FIELD_NAME                  RDB$CONSTRAINT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_13
    RDB$FIELD_NAME                  RDB$CONSTRAINT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_14
    RDB$FIELD_NAME                  RDB$CONSTRAINT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_15
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_15
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_16
    RDB$FIELD_NAME                  RDB$FORMAT
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_16
    RDB$FIELD_NAME                  RDB$RELATION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_17
    RDB$FIELD_NAME                  RDB$INPUT_SUB_TYPE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_17
    RDB$FIELD_NAME                  RDB$OUTPUT_SUB_TYPE
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_18
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_18
    RDB$FIELD_NAME                  RDB$PARAMETER_NAME
    RDB$FIELD_POSITION              2
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_18
    RDB$FIELD_NAME                  RDB$PROCEDURE_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_19
    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_20
    RDB$FIELD_NAME                  RDB$COLLATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_21
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_21
    RDB$FIELD_NAME                  RDB$PROCEDURE_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_22
    RDB$FIELD_NAME                  RDB$PROCEDURE_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_23
    RDB$FIELD_NAME                  RDB$EXCEPTION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_24
    RDB$FIELD_NAME                  RDB$EXCEPTION_NUMBER
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_25
    RDB$FIELD_NAME                  RDB$CHARACTER_SET_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_26
    RDB$FIELD_NAME                  RDB$CHARACTER_SET_ID
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_26
    RDB$FIELD_NAME                  RDB$COLLATION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_27
    RDB$FIELD_NAME                  RDB$DEPENDENT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_28
    RDB$FIELD_NAME                  RDB$DEPENDED_ON_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_29
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_30
    RDB$FIELD_NAME                  RDB$USER
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_31
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_32
    RDB$FIELD_NAME                  RDB$TRANSACTION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_33
    RDB$FIELD_NAME                  RDB$VIEW_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_34
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_35
    RDB$FIELD_NAME                  RDB$TRIGGER_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_36
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_37
    RDB$FIELD_NAME                  RDB$TYPE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_38
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_39
    RDB$FIELD_NAME                  RDB$ROLE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_40
    RDB$FIELD_NAME                  RDB$TRIGGER_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_41
    RDB$FIELD_NAME                  RDB$FOREIGN_KEY
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_42
    RDB$FIELD_NAME                  RDB$CONSTRAINT_TYPE
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_42
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_43
    RDB$FIELD_NAME                  RDB$INDEX_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_44
    RDB$FIELD_NAME                  RDB$BACKUP_ID
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_44
    RDB$FIELD_NAME                  RDB$BACKUP_LEVEL
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_45
    RDB$FIELD_NAME                  RDB$FUNCTION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_46
    RDB$FIELD_NAME                  RDB$GENERATOR_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_47
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_48
    RDB$FIELD_NAME                  RDB$FIELD_SOURCE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_49
    RDB$FIELD_NAME                  RDB$FIELD_SOURCE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_50
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_50
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_51
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_51
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_52
    RDB$FIELD_NAME                  RDB$MAP_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_53
    RDB$FIELD_NAME                  RDB$FUNCTION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>


    Records affected: 67
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    RDB$INDEX_NAME                  RDB$INDEX_0
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_1
    RDB$FIELD_NAME                  RDB$RELATION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_2
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_3
    RDB$FIELD_NAME                  RDB$FIELD_SOURCE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_4
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_5
    RDB$FIELD_NAME                  RDB$INDEX_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_6
    RDB$FIELD_NAME                  RDB$INDEX_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_7
    RDB$FIELD_NAME                  RDB$SECURITY_CLASS
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_8
    RDB$FIELD_NAME                  RDB$TRIGGER_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_9
    RDB$FIELD_NAME                  RDB$FUNCTION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_9
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_10
    RDB$FIELD_NAME                  RDB$FUNCTION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_10
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_11
    RDB$FIELD_NAME                  RDB$GENERATOR_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_12
    RDB$FIELD_NAME                  RDB$CONSTRAINT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_13
    RDB$FIELD_NAME                  RDB$CONSTRAINT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_14
    RDB$FIELD_NAME                  RDB$CONSTRAINT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_15
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_15
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_16
    RDB$FIELD_NAME                  RDB$FORMAT
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_16
    RDB$FIELD_NAME                  RDB$RELATION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_17
    RDB$FIELD_NAME                  RDB$INPUT_SUB_TYPE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_17
    RDB$FIELD_NAME                  RDB$OUTPUT_SUB_TYPE
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_18
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_18
    RDB$FIELD_NAME                  RDB$PARAMETER_NAME
    RDB$FIELD_POSITION              2
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_18
    RDB$FIELD_NAME                  RDB$PROCEDURE_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_19
    RDB$FIELD_NAME                  RDB$CHARACTER_SET_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_20
    RDB$FIELD_NAME                  RDB$COLLATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_21
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_21
    RDB$FIELD_NAME                  RDB$PROCEDURE_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_22
    RDB$FIELD_NAME                  RDB$PROCEDURE_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_23
    RDB$FIELD_NAME                  RDB$EXCEPTION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_24
    RDB$FIELD_NAME                  RDB$EXCEPTION_NUMBER
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_25
    RDB$FIELD_NAME                  RDB$CHARACTER_SET_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_26
    RDB$FIELD_NAME                  RDB$CHARACTER_SET_ID
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_26
    RDB$FIELD_NAME                  RDB$COLLATION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_27
    RDB$FIELD_NAME                  RDB$DEPENDENT_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_27
    RDB$FIELD_NAME                  RDB$DEPENDENT_TYPE
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_28
    RDB$FIELD_NAME                  RDB$DEPENDED_ON_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_28
    RDB$FIELD_NAME                  RDB$DEPENDED_ON_TYPE
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_28
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              2
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_29
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_30
    RDB$FIELD_NAME                  RDB$USER
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_31
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_32
    RDB$FIELD_NAME                  RDB$TRANSACTION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_33
    RDB$FIELD_NAME                  RDB$VIEW_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_34
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_35
    RDB$FIELD_NAME                  RDB$TRIGGER_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_36
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_37
    RDB$FIELD_NAME                  RDB$TYPE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_38
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_39
    RDB$FIELD_NAME                  RDB$ROLE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_40
    RDB$FIELD_NAME                  RDB$TRIGGER_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_41
    RDB$FIELD_NAME                  RDB$FOREIGN_KEY
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_42
    RDB$FIELD_NAME                  RDB$CONSTRAINT_TYPE
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_42
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_43
    RDB$FIELD_NAME                  RDB$INDEX_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_44
    RDB$FIELD_NAME                  RDB$BACKUP_ID
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_44
    RDB$FIELD_NAME                  RDB$BACKUP_LEVEL
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_45
    RDB$FIELD_NAME                  RDB$FUNCTION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_46
    RDB$FIELD_NAME                  RDB$GENERATOR_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_47
    RDB$FIELD_NAME                  RDB$PACKAGE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_48
    RDB$FIELD_NAME                  RDB$FIELD_SOURCE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_49
    RDB$FIELD_NAME                  RDB$FIELD_SOURCE
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_50
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_50
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_51
    RDB$FIELD_NAME                  RDB$FIELD_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_51
    RDB$FIELD_NAME                  RDB$RELATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_52
    RDB$FIELD_NAME                  RDB$MAP_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_53
    RDB$FIELD_NAME                  RDB$FUNCTION_ID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_54
    RDB$FIELD_NAME                  RDB$GUID
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_55
    RDB$FIELD_NAME                  RDB$PUBLICATION_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_56
    RDB$FIELD_NAME                  RDB$PUBLICATION_NAME
    RDB$FIELD_POSITION              1
    RDB$STATISTICS                  <null>

    RDB$INDEX_NAME                  RDB$INDEX_56
    RDB$FIELD_NAME                  RDB$TABLE_NAME
    RDB$FIELD_POSITION              0
    RDB$STATISTICS                  <null>

    Records affected: 74
"""

@pytest.mark.version('>=4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

