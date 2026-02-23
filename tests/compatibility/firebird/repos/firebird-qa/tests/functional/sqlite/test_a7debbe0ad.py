#coding:utf-8

"""
ID:          a7debbe0ad
ISSUE:       https://www.sqlite.org/src/tktview/a7debbe0ad
TITLE:       BETWEEN issue in a view
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

    create table t0(c0 varchar(1));
    create view v2(c0, c1) as select cast('b' as dm_ci), 'a' from t0 order by t0.c0;
    insert into t0(c0) values('');
    set count on;
    select sum(i) from (select case when v2.c1 between v2.c0 and v2.c1 then 1 else 0 end as i from v2); -- expected: 0, actual: 1
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    SUM 0
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
