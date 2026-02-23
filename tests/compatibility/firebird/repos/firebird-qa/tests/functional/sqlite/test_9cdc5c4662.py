#coding:utf-8

"""
ID:          9cdc5c4662
ISSUE:       https://www.sqlite.org/src/tktview/9cdc5c4662
TITLE:       Incorrect result from second execution of correlated scalar sub-query that uses a partial sort
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

    create table t1(x varchar(10));
    insert into t1 values('alfki');
    insert into t1 values('anatr');

    create table t2(y varchar(10), z timestamp);
    create index t2y on t2 (y);

    insert into t2 values('anatr', '1997-08-08 00:00:00');
    insert into t2 values('alfki', '1997-08-25 00:00:00');

    set count on;
    select (
      select y from t2 where x = y order by y, z
    )
    from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Y alfki
    Y anatr
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
