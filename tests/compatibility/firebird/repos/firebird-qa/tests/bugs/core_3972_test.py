#coding:utf-8

"""
ID:          issue-4305
ISSUE:       4305
TITLE:       Allow the selection of SQL_INT64, SQL_DATE and SQL_TIME in dialect 1
DESCRIPTION:
JIRA:        CORE-3972
FBTEST:      bugs.core_3972
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    recreate table t1 (n1 numeric(12,3));
    commit;
    insert into t1 values (1.23);
    insert into t1 values (10.23);
    insert into t1 values (3.567);
    commit;
    set list on;
    select mon$sql_dialect from mon$database;
    select n1, n1 / 2 from t1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MON$SQL_DIALECT                 1
    N1                              1.230
    DIVIDE                          0.6150000000000000
    N1                              10.230
    DIVIDE                          5.115000000000000
    N1                              3.567
    DIVIDE                          1.783500000000000
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

