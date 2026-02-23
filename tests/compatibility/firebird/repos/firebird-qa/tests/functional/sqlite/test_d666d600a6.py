#coding:utf-8

"""
ID:          d666d600a6
ISSUE:       https://www.sqlite.org/src/tktview/d666d600a6
TITLE:       COLLATE operator on lhs of BETWEEN expression is ignored.
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    create collation nocase for utf8 from unicode case insensitive;
    create table t1(x char(1));
    insert into t1 values('b');
    insert into t1 values(upper('b'));
    set count on;
    select * from t1 where x collate nocase between 'a' and upper('c');
    select * from t1 where x collate nocase >= 'a' and x collate nocase <= upper('c');
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
X b
X B
Records affected: 2
X b
X B
Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
