#coding:utf-8

"""
ID:          issue-1852
ISSUE:       1852
TITLE:       Incorrect result with EXECUTE STATEMENT and VARCHAR columns
DESCRIPTION: Last two bytes of VARCHAR columns are lost.
JIRA:        CORE-1434
FBTEST:      bugs.core_1434
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns (res varchar(31))
     as
     begin
        for execute statement
            'select cast(rdb$relation_name as varchar(24)) '
            || ' from rdb$relations r where r.rdb$system_flag = 1 and r.rdb$relation_name NOT starting with ''MON$'''
            || '  order by rdb$relation_id rows 30'
            into :res
        do
            suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    RES                             RDB$PAGES
    RES                             RDB$DATABASE
    RES                             RDB$FIELDS
    RES                             RDB$INDEX_SEGMENTS
    RES                             RDB$INDICES
    RES                             RDB$RELATION_FIELDS
    RES                             RDB$RELATIONS
    RES                             RDB$VIEW_RELATIONS
    RES                             RDB$FORMATS
    RES                             RDB$SECURITY_CLASSES
    RES                             RDB$FILES
    RES                             RDB$TYPES
    RES                             RDB$TRIGGERS
    RES                             RDB$DEPENDENCIES
    RES                             RDB$FUNCTIONS
    RES                             RDB$FUNCTION_ARGUMENTS
    RES                             RDB$FILTERS
    RES                             RDB$TRIGGER_MESSAGES
    RES                             RDB$USER_PRIVILEGES
    RES                             RDB$TRANSACTIONS
    RES                             RDB$GENERATORS
    RES                             RDB$FIELD_DIMENSIONS
    RES                             RDB$RELATION_CONSTRAINTS
    RES                             RDB$REF_CONSTRAINTS
    RES                             RDB$CHECK_CONSTRAINTS
    RES                             RDB$LOG_FILES
    RES                             RDB$PROCEDURES
    RES                             RDB$PROCEDURE_PARAMETERS
    RES                             RDB$CHARACTER_SETS
    RES                             RDB$COLLATIONS
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

