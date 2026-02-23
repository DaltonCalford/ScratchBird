#coding:utf-8

"""
ID:          issue-5191
ISSUE:       5191
TITLE:       Speed up function creation and loading when there are many functions in the database
DESCRIPTION:
JIRA:        CORE-4898
FBTEST:      bugs.core_4898
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- See: http://sourceforge.net/p/firebird/code/62075
    set list on;
    select ri.rdb$relation_name,rs.rdb$field_name,rs.rdb$field_position
    from rdb$indices ri join rdb$index_segments rs
    using (rdb$index_name)
    where ri.rdb$relation_name='RDB$FUNCTIONS' and rdb$field_name='RDB$FUNCTION_ID';
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$RELATION_NAME               RDB$FUNCTIONS
    RDB$FIELD_NAME                  RDB$FUNCTION_ID
    RDB$FIELD_POSITION              0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

