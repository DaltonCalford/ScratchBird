#coding:utf-8

"""
ID:          18458b1ad6
ISSUE:       https://www.sqlite.org/src/tktview/18458b1ad6
TITLE:       COLLATE issue in a view
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation coll_ci for utf8 from unicode case insensitive;
    create domain dm_ci as varchar(1) character set utf8 collate coll_ci;
    create table t0(c0 dm_ci);
    create view v0(c0, c1) as select distinct t0.c0, 'a' from t0;

    insert into t0(c0) values (upper('b'));

    set count on;
    select * from v0 where v0.c1 >= v0.c0; 
    select v0.*, v0.c1 >= v0.c0 as "'B' >= 'a' ? ==>" from v0; -- actual: 1, expected: 0
    -- todo: ask ASF or dimitr about result! comparison of data in nocase collation with data in ascii
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
    C0 B
    C1 a
    'B' >= 'a' ? ==> <false>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
