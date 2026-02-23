#coding:utf-8

"""
ID:          a5c8ed66ca
ISSUE:       https://www.sqlite.org/src/tktview/a5c8ed66ca
TITLE:       Incorrect count(*) when partial indices exist
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b int, c int, d int);

    insert into t1(a) values(1);
    insert into t1(a) values(2);
    insert into t1(a) select a+2 from t1;
    insert into t1(a) select a+4 from t1;
    insert into t1(a) select a+8 from t1;
    insert into t1(a) select a+16 from t1;
    insert into t1(a) select a+32 from t1;
    insert into t1(a) select a+64 from t1;
    commit;
    create index t1a on t1(a) where a between 10 and 20;
    set count on;
    select count(*) from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 128
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
