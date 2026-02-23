#coding:utf-8

"""
ID:          1b8d726456
ISSUE:       https://www.sqlite.org/src/tktview/1b8d726456
TITLE:       MAX yields unexpected result for UTF-16
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    create table t0(c0 varchar(1));
    insert into t0(c0) values ('윆');
    insert into t0(c0) values (1);

    set count on;
    select max(case 1 when 1 then t0.c0 end) from t0; -- 윆
    select max(t0.c0) from t0; -- 1
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MAX \uc706
    Records affected: 1
    MAX \uc706
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True, charset = 'utf8')
    assert act.clean_stdout == act.clean_expected_stdout
