#coding:utf-8

"""
ID:          issue-4366
ISSUE:       4366
TITLE:       Bugcheck or database corruption when attempting to store long incompressible data into a table
DESCRIPTION:
JIRA:        CORE-4036
FBTEST:      bugs.core_4036
"""

import pytest
from firebird.qa import *

init_script = """create table tw(s01 varchar(32600), s02 varchar(32600));
commit;"""

db = db_factory(init=init_script)

test_script = """insert into tw select rpad('',32600, gen_uuid()),rpad('',32600, gen_uuid()) from rdb$database;
commit;
set heading off;
SELECT count(*) from tw;
"""

act = isql_act('db', test_script)

expected_stdout = """1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

