#coding:utf-8

"""
ID:          b65cb2c8d9
ISSUE:       https://www.sqlite.org/src/tktview/b65cb2c8d9
TITLE:       Incorrect LIMIT on a UNION ALL query
DESCRIPTION:
    In a UNION ALL query with a LIMIT and OFFSET, if the OFFSET is greater than or equal to the number of rows in the
    first SELECT then the LIMIT is disabled. For example, the following SQL outputs 5 rows instead of just 1.
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x char);
    insert into t1 values('a');
    insert into t1 values('b');
    insert into t1 values('c');
    insert into t1 values('d');
    insert into t1 values('e');
    commit;

    set count on;
    select x, k as rdb_db_key
    from (select x, rdb$db_key k from t1)
    union all
    select * from (select x, rdb$db_key k from t1 order by x)
    offset 5 rows
    fetch first row only
    ;
"""

substitutions = [('[ \t]+', ' '), ('RDB_DB_KEY .*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X a
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
