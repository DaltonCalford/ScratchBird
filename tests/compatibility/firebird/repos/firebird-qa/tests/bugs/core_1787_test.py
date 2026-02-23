#coding:utf-8

"""
ID:          issue-2213
ISSUE:       2213
TITLE:       Consistency check when subquery is ordered by aggregate function from other context
DESCRIPTION:
JIRA:        CORE-1787
FBTEST:      bugs.core_1787
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TEST_TABLE1
(ID BIGINT,
 FK_ID INTEGER,
 REG_DATE TIMESTAMP NOT NULL);

COMMIT;

insert into test_table1 values (1,5,'01.01.2000');
insert into test_table1 values (2,5,'01.01.2001');
insert into test_table1 values (3,7,'01.01.2002');
insert into test_table1 values (4,8,'01.01.2003');
insert into test_table1 values (5,8,'01.01.2004');
insert into test_table1 values (6,8,'01.01.2005');
insert into test_table1 values (7,8,'01.01.2007');

COMMIT;
"""

db = db_factory(init=init_script)

test_script = """select t.fk_id,(select first 1 t1.reg_date from test_table1 t1 where t1.fk_id = t.fk_id
                order by min(t.fk_id))
from test_table1 t
group by t.fk_id;
"""

act = isql_act('db', test_script)

expected_stdout = """
       FK_ID                  REG_DATE
============ =========================
           5 2000-01-01 00:00:00.0000
           7 2002-01-01 00:00:00.0000
           8 2003-01-01 00:00:00.0000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

