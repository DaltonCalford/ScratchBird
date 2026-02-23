#coding:utf-8

"""
ID:          issue-423
ISSUE:       423
TITLE:       Strange/Inconsistent query results
DESCRIPTION:
JIRA:        CORE-99
FBTEST:      bugs.core_0099
"""

import pytest
from firebird.qa import *

init_script = """create table T1 (F1 char(4), F2 char(4));
create index T1_F1 on T1 (F1);

insert into T1 (F1, F2) values ('001', '001');
insert into T1 (F1, F2) values ('002', '002');
insert into T1 (F1, F2) values ('003', '003');
insert into T1 (F1, F2) values ('004', '004');

commit;
"""

db = db_factory(sql_dialect=1, init=init_script)

test_script = """select * from t1 where f1 = 3;
select * from t1 where f2 = 3;
"""

act = isql_act('db', test_script)

expected_stdout = """F1     F2
====== ======
003    003

F1     F2
====== ======
003    003

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

