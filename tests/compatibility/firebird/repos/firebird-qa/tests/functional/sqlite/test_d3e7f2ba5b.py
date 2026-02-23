#coding:utf-8

"""
ID:          d3e7f2ba5b
ISSUE:       https://www.sqlite.org/src/tktview/d3e7f2ba5b
TITLE:       Nested boolean formula with IN operator computes an incorrect result
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
    create table t0(c0 int);
    insert into t0(c0) values (-1);

    set count on;
    select * from t0 where (
     (
     		(false is not false) -- 0
    	or -- 0 
    		not (false is false or (t0.c0 in (-1))) -- 0

     ) -- should be 0 (but is 1)
     is false
    );

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 -1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
