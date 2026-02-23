#coding:utf-8

"""
ID:          b148fa6105
ISSUE:       https://www.sqlite.org/src/tktview/b148fa6105
TITLE:       CAST takes implicit COLLATE of its operand
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation name_coll for utf8 from unicode case insensitive;
    create domain dm_test varchar(3) character set utf8 collate name_coll;

    create table t0(c0 dm_test);
    insert into t0(c0) values ('a');
    set count on;
    select * from t0 where t0.c0 = upper('a');
    select * from t0 where cast(t0.c0 as varchar(1)) = upper('a');
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 a
    Records affected: 1
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
