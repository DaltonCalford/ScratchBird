#coding:utf-8

"""
ID:          issue-1411
ISSUE:       1411
TITLE:       Incorrect results when left join on subquery with constant column
DESCRIPTION:
JIRA:        CORE-1000
FBTEST:      bugs.core_1000
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE A (
    ID INTEGER
);

CREATE TABLE B (
    ID INTEGER
);

insert into A (id) values (1);
insert into A (id) values (2);
insert into A (id) values (3);

insert into B (id) values (1);
insert into B (id) values (2);

commit;
"""

db = db_factory(init=init_script)

test_script = """select a.id, b.id, bexists
from a
  left join (select id, 1 bexists from b) b on (a.id=b.id);

"""

act = isql_act('db', test_script)

expected_stdout = """
          ID           ID      BEXISTS
============ ============ ============
           1            1            1
           2            2            1
           3       <null>       <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
