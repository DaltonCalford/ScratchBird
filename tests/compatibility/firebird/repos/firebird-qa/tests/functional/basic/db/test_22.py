#coding:utf-8

"""
ID:          new-database-22
TITLE:       New DB - RDB$DATABASE content
DESCRIPTION: Check the correct content of RDB$DATABASE in new database.
FBTEST:      functional.basic.db.22
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

substitutions = [('RDB\\$CONSTRAINT_NAME[\\s]+RDB\\$INDEX.*', 'RDB\\$CONSTRAINT_NAME RDB\\$INDEX'),
                 ('RDB\\$INDEX_NAME[\\s]+RDB\\$INDEX.*', 'RDB\\$INDEX_NAME RDB\\$INDEX')]

db = db_factory()

test_script = """
    set list on;
    set count on;
    select rc.*
    from rdb$relation_constraints rc
    order by lpad( trim(replace(rdb$constraint_name, 'RDB$INDEX_', '')),31,'0');
"""

act = isql_act('db', test_script, substitutions=substitutions)

# version: 3.0

expected_stdout_1 = """
    RDB$CONSTRAINT_NAME             RDB$INDEX_0
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$RELATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_0

    RDB$CONSTRAINT_NAME             RDB$INDEX_2
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FIELDS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_2

    RDB$CONSTRAINT_NAME             RDB$INDEX_5
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$INDICES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_5

    RDB$CONSTRAINT_NAME             RDB$INDEX_7
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$SECURITY_CLASSES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_7

    RDB$CONSTRAINT_NAME             RDB$INDEX_8
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_8

    RDB$CONSTRAINT_NAME             RDB$INDEX_9
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FUNCTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_9

    RDB$CONSTRAINT_NAME             RDB$INDEX_11
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$GENERATORS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_11

    RDB$CONSTRAINT_NAME             RDB$INDEX_12
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_12

    RDB$CONSTRAINT_NAME             RDB$INDEX_13
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_13

    RDB$CONSTRAINT_NAME             RDB$INDEX_15
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_15

    RDB$CONSTRAINT_NAME             RDB$INDEX_17
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FILTERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_17

    RDB$CONSTRAINT_NAME             RDB$INDEX_18
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PROCEDURE_PARAMETERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_18

    RDB$CONSTRAINT_NAME             RDB$INDEX_19
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$CHARACTER_SETS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_19

    RDB$CONSTRAINT_NAME             RDB$INDEX_20
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$COLLATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_20

    RDB$CONSTRAINT_NAME             RDB$INDEX_21
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PROCEDURES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_21

    RDB$CONSTRAINT_NAME             RDB$INDEX_22
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PROCEDURES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_22

    RDB$CONSTRAINT_NAME             RDB$INDEX_23
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$EXCEPTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_23

    RDB$CONSTRAINT_NAME             RDB$INDEX_24
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$EXCEPTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_24

    RDB$CONSTRAINT_NAME             RDB$INDEX_25
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$CHARACTER_SETS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_25

    RDB$CONSTRAINT_NAME             RDB$INDEX_26
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$COLLATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_26

    RDB$CONSTRAINT_NAME             RDB$INDEX_32
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$TRANSACTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_32

    RDB$CONSTRAINT_NAME             RDB$INDEX_39
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$ROLES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_39

    RDB$CONSTRAINT_NAME             RDB$INDEX_44
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$BACKUP_HISTORY
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_44

    RDB$CONSTRAINT_NAME             RDB$INDEX_45
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FILTERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_45

    RDB$CONSTRAINT_NAME             RDB$INDEX_46
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$GENERATORS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_46

    RDB$CONSTRAINT_NAME             RDB$INDEX_47
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PACKAGES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_47

    RDB$CONSTRAINT_NAME             RDB$INDEX_53
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FUNCTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_53
    Records affected: 27
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    RDB$CONSTRAINT_NAME             RDB$INDEX_0
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$RELATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_0

    RDB$CONSTRAINT_NAME             RDB$INDEX_2
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FIELDS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_2

    RDB$CONSTRAINT_NAME             RDB$INDEX_5
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$INDICES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_5

    RDB$CONSTRAINT_NAME             RDB$INDEX_7
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$SECURITY_CLASSES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_7

    RDB$CONSTRAINT_NAME             RDB$INDEX_8
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_8

    RDB$CONSTRAINT_NAME             RDB$INDEX_9
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FUNCTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_9

    RDB$CONSTRAINT_NAME             RDB$INDEX_11
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$GENERATORS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_11

    RDB$CONSTRAINT_NAME             RDB$INDEX_12
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_12

    RDB$CONSTRAINT_NAME             RDB$INDEX_13
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_13

    RDB$CONSTRAINT_NAME             RDB$INDEX_15
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_15

    RDB$CONSTRAINT_NAME             RDB$INDEX_17
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FILTERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_17

    RDB$CONSTRAINT_NAME             RDB$INDEX_18
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PROCEDURE_PARAMETERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_18

    RDB$CONSTRAINT_NAME             RDB$INDEX_19
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$CHARACTER_SETS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_19

    RDB$CONSTRAINT_NAME             RDB$INDEX_20
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$COLLATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_20

    RDB$CONSTRAINT_NAME             RDB$INDEX_21
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PROCEDURES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_21

    RDB$CONSTRAINT_NAME             RDB$INDEX_22
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PROCEDURES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_22

    RDB$CONSTRAINT_NAME             RDB$INDEX_23
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$EXCEPTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_23

    RDB$CONSTRAINT_NAME             RDB$INDEX_24
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$EXCEPTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_24

    RDB$CONSTRAINT_NAME             RDB$INDEX_25
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$CHARACTER_SETS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_25

    RDB$CONSTRAINT_NAME             RDB$INDEX_26
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$COLLATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_26

    RDB$CONSTRAINT_NAME             RDB$INDEX_32
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$TRANSACTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_32

    RDB$CONSTRAINT_NAME             RDB$INDEX_39
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$ROLES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_39

    RDB$CONSTRAINT_NAME             RDB$INDEX_44
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$BACKUP_HISTORY
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_44

    RDB$CONSTRAINT_NAME             RDB$INDEX_45
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FILTERS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_45

    RDB$CONSTRAINT_NAME             RDB$INDEX_46
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$GENERATORS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_46

    RDB$CONSTRAINT_NAME             RDB$INDEX_47
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PACKAGES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_47

    RDB$CONSTRAINT_NAME             RDB$INDEX_53
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$FUNCTIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_53

    RDB$CONSTRAINT_NAME             RDB$INDEX_54
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$BACKUP_HISTORY
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_54

    RDB$CONSTRAINT_NAME             RDB$INDEX_55
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PUBLICATIONS
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_55

    RDB$CONSTRAINT_NAME             RDB$INDEX_56
    RDB$CONSTRAINT_TYPE             UNIQUE
    RDB$RELATION_NAME               RDB$PUBLICATION_TABLES
    RDB$DEFERRABLE                  NO
    RDB$INITIALLY_DEFERRED          NO
    RDB$INDEX_NAME                  RDB$INDEX_56


    Records affected: 30
"""

@pytest.mark.version('>=4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
