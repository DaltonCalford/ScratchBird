#coding:utf-8

"""
ID:          trigger.database.connect-03
ISSUE:       1120
TITLE:       Multiple triggers on database connect
DESCRIPTION:
  This tests normal operation of database CONNECT triggers when there are more of them.
FBTEST:      functional.trigger.database.connect_03
JIRA:        CORE-745
"""

import pytest
from firebird.qa import *

init_script = """
	create table LOG (ID integer, MSG varchar(100));
	create generator LOGID;
	set term ^;
	create trigger LOG_BI for LOG active before insert position 0
	as
	begin
	  if (new.ID is null) then
		new.ID = gen_id(LOGID,1);
	end ^

	create trigger ONCONNECT_1 on connect position 0
	as
	begin
	  insert into LOG (MSG) values ('Connect T1');
	end ^

	create trigger ONCONNECT_2 on connect position 10
	as
	begin
	  insert into LOG (MSG) values ('Connect T2');
	end ^

	set term ;^

	commit;

"""

db = db_factory(init=init_script)

test_script = """
    set list on;
	select * from LOG;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	ID                              1
	MSG                             Connect T1
	ID                              2
	MSG                             Connect T2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
