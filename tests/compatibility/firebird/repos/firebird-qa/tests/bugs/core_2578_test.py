#coding:utf-8

"""
ID:          issue-2988
ISSUE:       2988
TITLE:       select rdb$db_key from a view with a more than 1 table joined, results in conversion error
DESCRIPTION:
JIRA:        CORE-2578
FBTEST:      bugs.core_2578
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TABLE_A (
    F_A INTEGER,
    F_B INTEGER
);

CREATE TABLE TABLE_B(
    F_A INTEGER,
    F_C INTEGER
);

CREATE VIEW VIEW_A(
    K1,
    K2,
    F_A,
    F_B,
    F_C)
AS
select A.rdb$db_key, B.rdb$db_key, A.F_A, A.F_B, B.F_C from table_A A
left join table_B B on A.F_A = B.F_A;

commit;

insert into TABLE_A (F_A,F_B) values (1,1) ;
insert into TABLE_B (F_A,F_C) values (1,1) ;
commit;

"""

db = db_factory(init=init_script)

test_script = """select rdb$db_key from VIEW_A order by 1 ;
"""

act = isql_act('db', test_script)

expected_stdout = """
DB_KEY
================================
81000000010000008000000001000000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

