#coding:utf-8

"""
ID:          767a8cbc6d
ISSUE:       https://www.sqlite.org/src/tktview/767a8cbc6d
TITLE:       COLLATE NOCASE string comparison yields incorrect result when partial index presents
DESCRIPTION:
NOTES:
    [17.08.2025] pzotov
    ::: NB ::: Result in FB is opposite to SQLite!
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation coll_ci for utf8 from unicode case insensitive;
    create domain dm_ci as varchar(1) character set utf8 collate coll_ci;

    create table t0(c0 dm_ci, c1 varchar(1));
    create index i0 on t0(c0) where c0 >= c1;
    update or insert into t0(c0,c1) values('a', upper('b')) matching(c0, c1);

    set count on;
    select t0.*, t0.c1 <= t0.c0 as "'B' <= 'a' ? ==>" from t0;
    select t0.* from t0 where t0.c1 <= t0.c0; -- unexpected: row is not fetched
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 a
    C1 B
    'B' <= 'a' ? ==> <false>
    Records affected: 1
    Records affected: 0
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
