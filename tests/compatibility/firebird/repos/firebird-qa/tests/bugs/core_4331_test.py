#coding:utf-8

"""
ID:          issue-4654
ISSUE:       4654
TITLE:       LAG, LEAD and NTH_VALUE raise error when the second argument is NULL
DESCRIPTION:
JIRA:        CORE-4331
FBTEST:      bugs.core_4331
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
	recreate table test(id int primary key, x int, y int);
	commit;
	insert into test values(101, 1, 10);
	insert into test values(102, 2, 20);
	insert into test values(103, 3, 30);
	insert into test values(104, 4, 40);
	insert into test values(105, 5, 50);
	insert into test values(106, 6, 60);
	insert into test values(107, 7, 70);
	commit;

	set list on;
	select
		 id
		,lag(x,null)over(order by id) x_lag
		,lead(x,null)over(order by id) x_lead
		,nth_value(x,null)over(order by id) x_nth
	from test t
	order by id;
"""

act = isql_act('db', test_script)

expected_stdout = """
	ID                              101
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>

	ID                              102
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>

	ID                              103
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>

	ID                              104
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>

	ID                              105
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>

	ID                              106
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>

	ID                              107
	X_LAG                           <null>
	X_LEAD                          <null>
	X_NTH                           <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
