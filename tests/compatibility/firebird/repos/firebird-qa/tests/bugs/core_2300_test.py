#coding:utf-8

"""
ID:          issue-2724
ISSUE:       2724
TITLE:       Unexpected error "arithmetic exception, numeric overflow, or string truncation" while evaluating SUBSTRING the second time
DESCRIPTION:
JIRA:        CORE-2300
FBTEST:      bugs.core_2300
"""

import pytest
from firebird.qa import *

init_script = """set term ^ ;
create procedure p
  returns ( res varchar(10) )
as begin
  res = null;
  suspend;
  res = '0123456789';
  suspend;
end ^
set term ; ^
commit;"""

db = db_factory(init=init_script)

test_script = """select substring(res from 1 for 5) from p order by 1; -- success
select substring(res from 1 for 5) from p order by 1; -- error
"""

act = isql_act('db', test_script)

expected_stdout = """
SUBSTRING
=========
<null>
01234


SUBSTRING
=========
<null>
01234

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

