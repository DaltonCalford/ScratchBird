#coding:utf-8

"""
ID:          360c6073e1
ISSUE:       https://www.sqlite.org/src/tktview/360c6073e1
TITLE:       Aggregate MAX() function with COLLATE clause
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;

    create collation nocase for utf8 from unicode case insensitive;
    create table t1(x char(3));
    insert into t1 values('abc');
    insert into t1 values('ABC');
    insert into t1 values('BCD');

    select max(x) as v1 from t1;
    select max(x collate nocase) as v2 from t1;
    select max(x) v3a, max(x collate nocase) as v3b from t1;
    select max(x collate nocase) v4a, max(x) as v4b from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 abc
    V2 BCD
    V3A abc
    V3B BCD
    V4A BCD
    V4B abc
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
