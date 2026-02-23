#coding:utf-8

"""
ID:          issue-2607
ISSUE:       2607
TITLE:       Unexpected (wrong) results with COALESCE and GROUP BY
DESCRIPTION:
JIRA:        CORE-2176
FBTEST:      bugs.core_2176
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter view v_rel (rid, rnm, rty) as
    select r.rdb$relation_id, r.rdb$relation_name, r.rdb$relation_type
    from rdb$relations r
    where r.rdb$relation_id <= 30 -- ::: NB ::: added this cond because number of rows in 2.5 and 3.0 differ!
    ;
    commit;

    set count on ;
    set list on;

    select rid as c from v_rel
    group by 1 ;
    -- correct result

    select * from (
    select rid as c from v_rel
    ) group by 1 ;
    -- also correct

    select coalesce(rid, 0) as c from v_rel
    group by 1 ;
    -- ERROR: no zero ID is reported, the last ID is reported twice

    select * from (
    select coalesce(rid, 0) as c from v_rel
    ) group by 1 ;
    -- ERROR: single NULL is returned

    select * from (
      select coalesce(rid, 0) as a, coalesce(rty, 0) as b from v_rel
    ) group by 1, 2 ;
    -- ERROR: infinite result set with all zero values

    select * from (
      select coalesce(rid, 0) as a, coalesce(rnm, '') as b from v_rel
    ) group by 1, 2 ;
    -- ERROR: conversion error
"""

act = isql_act('db', test_script)

expected_stdout = """
    C                               0
    C                               1
    C                               2
    C                               3
    C                               4
    C                               5
    C                               6
    C                               7
    C                               8
    C                               9
    C                               10
    C                               11
    C                               12
    C                               13
    C                               14
    C                               15
    C                               16
    C                               17
    C                               18
    C                               19
    C                               20
    C                               21
    C                               22
    C                               23
    C                               24
    C                               25
    C                               26
    C                               27
    C                               28
    C                               29
    C                               30
    Records affected: 31
    C                               0
    C                               1
    C                               2
    C                               3
    C                               4
    C                               5
    C                               6
    C                               7
    C                               8
    C                               9
    C                               10
    C                               11
    C                               12
    C                               13
    C                               14
    C                               15
    C                               16
    C                               17
    C                               18
    C                               19
    C                               20
    C                               21
    C                               22
    C                               23
    C                               24
    C                               25
    C                               26
    C                               27
    C                               28
    C                               29
    C                               30
    Records affected: 31
    C                               0
    C                               1
    C                               2
    C                               3
    C                               4
    C                               5
    C                               6
    C                               7
    C                               8
    C                               9
    C                               10
    C                               11
    C                               12
    C                               13
    C                               14
    C                               15
    C                               16
    C                               17
    C                               18
    C                               19
    C                               20
    C                               21
    C                               22
    C                               23
    C                               24
    C                               25
    C                               26
    C                               27
    C                               28
    C                               29
    C                               30
    Records affected: 31
    C                               0
    C                               1
    C                               2
    C                               3
    C                               4
    C                               5
    C                               6
    C                               7
    C                               8
    C                               9
    C                               10
    C                               11
    C                               12
    C                               13
    C                               14
    C                               15
    C                               16
    C                               17
    C                               18
    C                               19
    C                               20
    C                               21
    C                               22
    C                               23
    C                               24
    C                               25
    C                               26
    C                               27
    C                               28
    C                               29
    C                               30
    Records affected: 31
    A                               0
    B                               0
    A                               1
    B                               0
    A                               2
    B                               0
    A                               3
    B                               0
    A                               4
    B                               0
    A                               5
    B                               0
    A                               6
    B                               0
    A                               7
    B                               0
    A                               8
    B                               0
    A                               9
    B                               0
    A                               10
    B                               0
    A                               11
    B                               0
    A                               12
    B                               0
    A                               13
    B                               0
    A                               14
    B                               0
    A                               15
    B                               0
    A                               16
    B                               0
    A                               17
    B                               0
    A                               18
    B                               0
    A                               19
    B                               0
    A                               20
    B                               0
    A                               21
    B                               0
    A                               22
    B                               0
    A                               23
    B                               0
    A                               24
    B                               0
    A                               25
    B                               0
    A                               26
    B                               0
    A                               27
    B                               0
    A                               28
    B                               0
    A                               29
    B                               0
    A                               30
    B                               0
    Records affected: 31
    A                               0
    B                               RDB$PAGES
    A                               1
    B                               RDB$DATABASE
    A                               2
    B                               RDB$FIELDS
    A                               3
    B                               RDB$INDEX_SEGMENTS
    A                               4
    B                               RDB$INDICES
    A                               5
    B                               RDB$RELATION_FIELDS
    A                               6
    B                               RDB$RELATIONS
    A                               7
    B                               RDB$VIEW_RELATIONS
    A                               8
    B                               RDB$FORMATS
    A                               9
    B                               RDB$SECURITY_CLASSES
    A                               10
    B                               RDB$FILES
    A                               11
    B                               RDB$TYPES
    A                               12
    B                               RDB$TRIGGERS
    A                               13
    B                               RDB$DEPENDENCIES
    A                               14
    B                               RDB$FUNCTIONS
    A                               15
    B                               RDB$FUNCTION_ARGUMENTS
    A                               16
    B                               RDB$FILTERS
    A                               17
    B                               RDB$TRIGGER_MESSAGES
    A                               18
    B                               RDB$USER_PRIVILEGES
    A                               19
    B                               RDB$TRANSACTIONS
    A                               20
    B                               RDB$GENERATORS
    A                               21
    B                               RDB$FIELD_DIMENSIONS
    A                               22
    B                               RDB$RELATION_CONSTRAINTS
    A                               23
    B                               RDB$REF_CONSTRAINTS
    A                               24
    B                               RDB$CHECK_CONSTRAINTS
    A                               25
    B                               RDB$LOG_FILES
    A                               26
    B                               RDB$PROCEDURES
    A                               27
    B                               RDB$PROCEDURE_PARAMETERS
    A                               28
    B                               RDB$CHARACTER_SETS
    A                               29
    B                               RDB$COLLATIONS
    A                               30
    B                               RDB$EXCEPTIONS
    Records affected: 31
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

