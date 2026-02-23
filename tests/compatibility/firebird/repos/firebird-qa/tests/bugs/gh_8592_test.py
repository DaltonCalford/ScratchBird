#coding:utf-8

"""
ID:          issue-8592
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8592
TITLE:       Presence of 'ROWS <n_limit>' causes garbage in error message when string conversion problem raises
DESCRIPTION:
    See https://www.sqlite.org/src/tktview/de7db14784
NOTES:
    [10.06.2025] pzotov
    Checked on 6.0.0.799-c82c9cf; 5.0.3.1660-0-d0d870a; 4.0.6.3207-4a300e7
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    create table test_a(f01 varchar(5) primary key, f02 varchar(5));
    create table test_b(f01 varchar(5), f02 int);
    insert into test_a values('one', 'i');
    insert into test_b values('one', 1);
    select a.f01 from test_a a where exists (select 1 from test_b b where a.f01 = b.f02 rows 1);
"""

act = isql_act('db', test_script, substitutions = [('[\t ]+', ' ')])

expected_stdout = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "one"
"""

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

