#coding:utf-8

"""
ID:          issue-1950
ISSUE:       1950
TITLE:       JOIN on first record of ordered derived table returns wrong record
DESCRIPTION:
JIRA:        CORE-1533
FBTEST:      bugs.core_1533
"""

import pytest
from firebird.qa import *

init_script = """create table X (
    ID integer,
    DAT DATE
);
commit;
insert into X (ID, DAT) values (1, '2006-05-16');
insert into X (ID, DAT) values (2, '2004-11-16');
insert into X (ID, DAT) values (3, '2007-01-01');
insert into X (ID, DAT) values (4, '2005-07-11');
commit;
create index IDX_X on X (DAT);
commit;
"""

db = db_factory(init=init_script)

test_script = """select X2.ID, x1.ID,x2.dat from X as X2 left join (select first 1 X.ID from X order by X.DAT) X1 on X1.ID=X2.ID;
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID           ID         DAT
============ ============ ===========
           1       <null> 2006-05-16
           2            2 2004-11-16
           3       <null> 2007-01-01
           4       <null> 2005-07-11

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

