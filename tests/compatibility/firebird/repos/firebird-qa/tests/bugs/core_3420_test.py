#coding:utf-8

"""
ID:          issue-3783
ISSUE:       3783
TITLE:       BOOLEAN not present in system table RDB$TYPES
DESCRIPTION:
JIRA:        CORE-3420
FBTEST:      bugs.core_3420
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
        rdb$field_name,
        rdb$type,
        rdb$type_name,
        rdb$system_flag
    from rdb$types t
    where upper(t.rdb$type_name) = upper('boolean')
    order by t.rdb$field_name;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FIELD_NAME                  RDB$FIELD_TYPE
    RDB$TYPE                        23
    RDB$TYPE_NAME                   BOOLEAN
    RDB$SYSTEM_FLAG                 1
    RDB$FIELD_NAME                  RDB$FUNCTION_TYPE
    RDB$TYPE                        1
    RDB$TYPE_NAME                   BOOLEAN
    RDB$SYSTEM_FLAG                 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

