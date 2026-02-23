#coding:utf-8

"""
ID:          issue-5830
ISSUE:       5830
TITLE:       Use exception instead bugcheck for EVL_expr
DESCRIPTION:
NOTES:
  Ability to use nested aggregate looks strange.
  Query like here can not be run in MS SQL or Postgress.
  Sent letters to dimitr, 19.06.2018.
JIRA:        CORE-5563
FBTEST:      bugs.core_5563
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
        sum(
            sum(
                (select sum(rf.rdb$field_position) from rdb$relation_fields rf)
               )
           )
    from
        rdb$database
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    SUM                             <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

