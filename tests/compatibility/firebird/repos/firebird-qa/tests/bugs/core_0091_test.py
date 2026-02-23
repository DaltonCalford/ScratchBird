#coding:utf-8

"""
ID:          issue-416
ISSUE:       416
TITLE:       Recreate and self-referencing indexes
DESCRIPTION:
JIRA:        CORE-91
FBTEST:      bugs.core_0091
"""

import pytest
from firebird.qa import *

init_script = """recreate table t2 (
    i integer not null primary key,
    p integer references t2(i) on delete set null
 );
commit;
"""

db = db_factory(init=init_script)

test_script = """select * from t2;
insert into t2 values (1, null);
delete from t2 where i=1;
commit;

recreate table t2 (
    i integer not null primary key,
    p integer references t2(i) on delete set null
 );
commit;
select * from t2;
insert into t2 values (1, null);
delete from t2 where i=1;
commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()


