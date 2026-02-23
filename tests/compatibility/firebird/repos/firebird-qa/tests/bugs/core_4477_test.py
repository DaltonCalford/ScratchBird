#coding:utf-8

"""
ID:          issue-4797
ISSUE:       4797
TITLE:       Field RDB$MAP_TO_TYPE is not present in RDB$TYPES
DESCRIPTION:
JIRA:        CORE-4477
FBTEST:      bugs.core_4477
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select rdb$field_name,rdb$type,rdb$type_name,rdb$system_flag from rdb$types where upper(rdb$field_name) = upper('rdb$map_to_type') order by rdb$type;
"""

act = isql_act('db', test_script)

expected_stdout = """
   RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
   RDB$TYPE                        0
   RDB$TYPE_NAME                   USER
   RDB$SYSTEM_FLAG                 1

   RDB$FIELD_NAME                  RDB$MAP_TO_TYPE
   RDB$TYPE                        1
   RDB$TYPE_NAME                   ROLE
   RDB$SYSTEM_FLAG                 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

