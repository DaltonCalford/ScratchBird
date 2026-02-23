#coding:utf-8

"""
ID:          issue-2468
ISSUE:       2468
TITLE:       Null in the first record in a condition on rdb$db_key
DESCRIPTION:
JIRA:        CORE-2031
FBTEST:      bugs.core_2031
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE A1 (
    FA1 INTEGER,
    FA2 INTEGER
);
commit;
insert into a1 (fa1, fa2) values (1, 1);
insert into a1 (fa1, fa2) values (1, 2);
insert into a1 (fa1, fa2) values (1, 3);
insert into a1 (fa1, fa2) values (1, 4);
insert into a1 (fa1, fa2) values (1, 5);
commit;
"""

db = db_factory(init=init_script)

test_script = """update a1 a set a.fa1 =
(select 2 from a1 aa
where a.rdb$db_key = aa.rdb$db_key);
commit;
select * from A1;"""

act = isql_act('db', test_script)

expected_stdout = """
         FA1          FA2
============ ============
           2            1
           2            2
           2            3
           2            4
           2            5

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

