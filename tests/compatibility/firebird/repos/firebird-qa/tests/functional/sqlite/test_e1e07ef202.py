#coding:utf-8

"""
ID:          e1e07ef202
ISSUE:       https://www.sqlite.org/src/tktview/e1e07ef202
TITLE:       COLLATE in BETWEEN expression is ignored
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    set bail on;
    create collation name_coll for utf8 from unicode case insensitive;
    create domain dm_test varchar(3) character set utf8 collate name_coll;

    create table t0 (c3 varchar(3));
    insert into t0(c3) values ('-11');
    set count on;
    -- expected: no row; actual: row is fetched
    select * from t0 where (t0.c3 collate name_coll) between -1 and '5';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
