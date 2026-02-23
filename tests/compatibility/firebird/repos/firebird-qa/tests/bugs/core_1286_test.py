#coding:utf-8

"""
ID:          issue-4153
ISSUE:       4153
TITLE:       isql: zero divide + coredump when use "-pag 0" command switch & set heading on inside .sql script
DESCRIPTION:
JIRA:        CORE-3810
FBTEST:      bugs.core_1286
"""

import pytest
from firebird.qa import *

init_script = """create table test(id int);
commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
           R
============
           1
"""

test_script = """
set heading on;
select 1 as r from rdb$fields rows 1;
-- Crash of ISQL (not server) is reproduced when make connect by ISQL of WI-V2.5.1.26351.
-- After ISQL crash firebird.log contains: INET/inet_error: read errno = 10054
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.isql(switches=['-pag', '0'], input=test_script)
    assert act.clean_stdout == act.clean_expected_stdout


