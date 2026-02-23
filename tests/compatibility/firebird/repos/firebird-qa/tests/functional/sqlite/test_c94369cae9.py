#coding:utf-8

"""
ID:          c94369cae9
ISSUE:       https://www.sqlite.org/src/tktview/c94369cae9
TITLE:       Wrong answer when use no-case collation due to the LIKE optimization
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create collation name_coll for utf8 from unicode case insensitive;
    create domain dm_test varchar(5) character set utf8 collate name_coll;
    create table t1(x dm_test unique);
    insert into t1 values('/abc');
    insert into t1 values('\def');
    insert into t1 values('\___');
    insert into t1 values('|%%%');
    insert into t1 values('|%%A');
    set count on;
    select x as v1 from t1 where x like '/%';
    select x as v2 from t1 where x like '\`_`_`_' escape '`';
    select x as v3 from t1 where x like '|`%`%`%' escape '`';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 /abc
    Records affected: 1
    
    V2 \___
    Records affected: 1
    
    V3 |%%%
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
