#coding:utf-8

"""
ID:          issue-5173
ISSUE:       5173
TITLE:       Compound ALTER TABLE statement with ADD and DROP the same constraint failed
  if this constraint involves index creation (PK/UNQ/FK)
DESCRIPTION:
JIRA:        CORE-4878
FBTEST:      bugs.gh_5173
"""

import pytest
from firebird.qa import db_factory, isql_act, Action

db = db_factory()

test_script = """
    set autoddl off;
    recreate table t1(x int not null);
    recreate table t2(x int not null);
    recreate table t3(x int not null);

    -- set echo on;

    alter table t1 add constraint t1_unq unique(x), drop constraint t1_unq;

    alter table t2 add constraint t2_pk primary key(x), drop constraint t2_pk;

    alter table t3 add constraint t3_pk primary key(x), add constraint t3_fk foreign key(x) references t3(x), drop constraint t3_fk;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.execute()
