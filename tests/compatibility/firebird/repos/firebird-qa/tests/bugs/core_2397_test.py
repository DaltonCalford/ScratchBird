#coding:utf-8

"""
ID:          issue-2816
ISSUE:       2816
TITLE:       If you drop two different indexes within the same transaction, you get database corruption
DESCRIPTION:
JIRA:        CORE-2397
FBTEST:      bugs.core_2397
"""

import pytest
from firebird.qa import *

init_script = """create table test(id int, title varchar(50));
commit;
create index test1 on test computed by (id +1);
create index test2 on test computed by (id +2);
commit;
"""

db = db_factory(init=init_script)

test_script = """SET AUTODDL OFF;
drop index test1;
drop index test2;
commit;
insert into test values(1,'test');
commit;
SELECT id from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID
============
           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

