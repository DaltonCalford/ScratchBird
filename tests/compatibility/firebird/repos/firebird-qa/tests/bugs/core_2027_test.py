#coding:utf-8

"""
ID:          issue-2464
ISSUE:       2464
TITLE:       Incorrect buffer size for ORDER BY expression with system fields
DESCRIPTION:
JIRA:        CORE-2027
FBTEST:      bugs.core_2027
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table t(rel_name char(31) character set unicode_fss);
    commit;
    insert into t
    select r.rdb$relation_name from rdb$relations r
    order by r.rdb$relation_id
    rows 40; -- minimal numer of rows in rdb$relations that is common for 2.5 and 3.0
    commit;

    set list on;
    select t.rel_name
    from t
    order by '0' || t.rel_name;
"""

act = isql_act('db', test_script)

expected_stdout = """
    REL_NAME                        MON$ATTACHMENTS
    REL_NAME                        MON$CALL_STACK
    REL_NAME                        MON$DATABASE
    REL_NAME                        MON$IO_STATS
    REL_NAME                        MON$RECORD_STATS
    REL_NAME                        MON$STATEMENTS
    REL_NAME                        MON$TRANSACTIONS
    REL_NAME                        RDB$BACKUP_HISTORY
    REL_NAME                        RDB$CHARACTER_SETS
    REL_NAME                        RDB$CHECK_CONSTRAINTS
    REL_NAME                        RDB$COLLATIONS
    REL_NAME                        RDB$DATABASE
    REL_NAME                        RDB$DEPENDENCIES
    REL_NAME                        RDB$EXCEPTIONS
    REL_NAME                        RDB$FIELDS
    REL_NAME                        RDB$FIELD_DIMENSIONS
    REL_NAME                        RDB$FILES
    REL_NAME                        RDB$FILTERS
    REL_NAME                        RDB$FORMATS
    REL_NAME                        RDB$FUNCTIONS
    REL_NAME                        RDB$FUNCTION_ARGUMENTS
    REL_NAME                        RDB$GENERATORS
    REL_NAME                        RDB$INDEX_SEGMENTS
    REL_NAME                        RDB$INDICES
    REL_NAME                        RDB$LOG_FILES
    REL_NAME                        RDB$PAGES
    REL_NAME                        RDB$PROCEDURES
    REL_NAME                        RDB$PROCEDURE_PARAMETERS
    REL_NAME                        RDB$REF_CONSTRAINTS
    REL_NAME                        RDB$RELATIONS
    REL_NAME                        RDB$RELATION_CONSTRAINTS
    REL_NAME                        RDB$RELATION_FIELDS
    REL_NAME                        RDB$ROLES
    REL_NAME                        RDB$SECURITY_CLASSES
    REL_NAME                        RDB$TRANSACTIONS
    REL_NAME                        RDB$TRIGGERS
    REL_NAME                        RDB$TRIGGER_MESSAGES
    REL_NAME                        RDB$TYPES
    REL_NAME                        RDB$USER_PRIVILEGES
    REL_NAME                        RDB$VIEW_RELATIONS
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

