#coding:utf-8

"""
ID:          issue-1419
ISSUE:       1419
TITLE:       Restoring RDB$BASE_FIELD for expression
DESCRIPTION: RDB$BASE_FIELD for expression have to be NULL
JIRA:        CORE-1009
FBTEST:      bugs.core_1009
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core1009.fbk')

test_script = """
  set list on;
  select rdb$field_name, rdb$base_field from rdb$relation_fields where rdb$relation_name = 'TEST_VIEW';
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FIELD_NAME                  ID
    RDB$BASE_FIELD                  ID
    RDB$FIELD_NAME                  EXPR
    RDB$BASE_FIELD                  <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

