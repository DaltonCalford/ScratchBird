#coding:utf-8

"""
ID:          issue-1732
ISSUE:       1732
TITLE:       RDB$DB_KEY not supported in derived tables and merge command
DESCRIPTION:
JIRA:        CORE-1313
FBTEST:      bugs.core_1313
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table t (c1 integer);
    commit;

    insert into t values (1);
    insert into t values (2);
    insert into t values (3);

    commit;

    select 'point-1' msg, t1.*
    from t t1
    right join (select t.rdb$db_key as dbkey from t) t2 on t2.dbkey = t1.rdb$db_key;

    merge into t t1
    using (select t.rdb$db_key as dbkey from t) t2
    on t2.dbkey = t1.rdb$db_key
    when not matched then insert values (null);

    select 'point-2' msg, t.* from t;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    MSG     C1
    point-1  1
    point-1  2
    point-1  3

    MSG     C1
    point-2  1
    point-2  2
    point-2  3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

