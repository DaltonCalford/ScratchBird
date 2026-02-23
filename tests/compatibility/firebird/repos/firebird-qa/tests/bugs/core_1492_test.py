#coding:utf-8

"""
ID:          issue-1907
ISSUE:       1907
TITLE:       BLOB isn't compatible with [VAR]CHAR in COALESCE
DESCRIPTION:
JIRA:        CORE-1492
FBTEST:      bugs.core_1492
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t (id int primary key, b blob sub_type text );
    commit;

    insert into t(id, b) values (1, NULL);
    insert into t(id, b) values (2, 'QWER');
    commit;

    set list on;
    select coalesce(b, '') as b_blob
    from t
    order by id;
"""

act = isql_act('db', test_script, substitutions=[('B_BLOB.*', 'B_BLOB')])

expected_stdout = """
    B_BLOB                          0:1
    B_BLOB                          82:1e0
    QWER
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

