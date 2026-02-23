#coding:utf-8

"""
ID:          issue-2948
ISSUE:       2948
TITLE:       PSQL doesnt allow to use singleton query result as input parameter of stored procedure when procedure accessed using 'execute procedure'
DESCRIPTION:
JIRA:        CORE-2538
FBTEST:      bugs.core_2538
"""

import pytest
from firebird.qa import *

init_script = """set term ^ ;
create procedure P (I integer)
returns (O integer)
AS
BEGIN
  SUSPEND;
END ^
"""

db = db_factory(init=init_script)

test_script = """set term ^ ;

execute block
as
declare variable i integer;
begin
  select 1 from P((select 1 from rdb$database)) into :i;
end ^

execute block
as
declare variable i integer;
begin
  execute procedure P((select 1 from rdb$database)) returning_values :i;
end ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
