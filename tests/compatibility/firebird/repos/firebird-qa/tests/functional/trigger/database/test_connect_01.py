#coding:utf-8

"""
ID:          trigger.database.connect-01
TITLE:       Trigger on database connect. See also CORE-745
DESCRIPTION: 
  This tests normal operation of database CONNECT trigger.
FBTEST:      functional.trigger.database.connect_01
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
	end
	^

	create trigger ONCONNECT on connect position 0
	as
	begin
	  insert into LOG (MSG) values ('Connect');
	end
	^
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
	MSG                             Connect  
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
