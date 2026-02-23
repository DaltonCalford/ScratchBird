#coding:utf-8

"""
ID:          issue-1827
ISSUE:       1827
TITLE:       Support for autonomous transactions
DESCRIPTION:
JIRA:        CORE-1409
FBTEST:      bugs.core_1409
"""

import pytest
from firebird.qa import *

init_script = """create table log (
  msg varchar(60)
);
commit;
set term !;

create trigger t_conn on connect
as
begin
  if (current_user = 'SYSDBA') then
  begin
    in autonomous transaction
    do
    begin
      insert into log (msg) values ('SYSDBA connected');
    end
  end
end!

set term ;!
commit;
"""

db = db_factory(init=init_script)

test_script = """select msg from log;
"""

act = isql_act('db', test_script)

expected_stdout = """
MSG
============================================================
SYSDBA connected

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

