#coding:utf-8

"""
ID:          issue-2137
ISSUE:       2137
TITLE:       Buffer overflow in conversion
DESCRIPTION:
  Confirmed bug on WI-V2.0.0.12724:
    * "buffer overrun" when use dialect 1;
    * "string right truncation" when use dialect 3.
JIRA:        CORE-1712
FBTEST:      bugs.core_1712
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    recreate table testtable(f1 numeric(15, 2));
    commit;

    insert into testtable(f1) values(1e19);
    commit;

    set list on;
    select replace(cast(f1 as varchar(30)),'0','') f1_as_varchar30 from testtable;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    F1_AS_VARCHAR30                 1.e+19
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

