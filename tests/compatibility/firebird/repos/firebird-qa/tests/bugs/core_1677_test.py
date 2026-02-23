#coding:utf-8

"""
ID:          issue-2102
ISSUE:       2102
TITLE:       Floor & ceiling functions give wrong results with exact numeric arguments
DESCRIPTION:
  select floor(cast(1500 as numeric(18,5))) from rdb$database -> -4827 (wrong)
  select floor(cast(1500 as numeric(18,4))) from rdb$database -> 1500 (correct)
  select ceiling(cast(1500 as numeric(18,5))) from rdb$database -> -4826 (wrong)
  Actually, any precision higher than 6 gives a wrong result.
JIRA:        CORE-1677
FBTEST:      bugs.core_1677
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select floor(cast(1500 as numeric(18,5))) F1,floor(cast(1500 as numeric(18,4))) F2, ceiling(cast(1500 as numeric(18,5))) F3 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
                   F1                    F2                    F3
===================== ===================== =====================
                 1500                  1500                  1500

"""

@pytest.mark.version('>=2.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

