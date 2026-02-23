#coding:utf-8

"""
ID:          issue-3347
ISSUE:       3347
TITLE:       Incorrect ROW_COUNT value after SINGULAR condition
DESCRIPTION:
JIRA:        CORE-2965
FBTEST:      bugs.core_2965
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """set term !!;
execute block
returns(rcount integer)
as
declare tmpint integer;
begin
   select rdb$relation_id from rdb$database into tmpint;
   if (SINGULAR(select rdb$relation_id from rdb$database where rdb$relation_id is null)) then begin end
   rcount = row_count;
   suspend;
end!!
execute block
returns(rcount integer)
as
declare tmpint integer;
begin
   select rdb$relation_id from rdb$database into tmpint;
   if (SINGULAR(select * from rdb$relation_fields)) then begin end
   rcount = row_count;
   suspend;
end!!"""

act = isql_act('db', test_script)

expected_stdout = """
      RCOUNT
============
           1


      RCOUNT
============
           1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

