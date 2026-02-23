#coding:utf-8

"""
ID:          intfunc.count-02
TITLE:       COUNT
DESCRIPTION: Count of: 1) all rows; 2) not null values; 3) distinct values
FBTEST:      functional.intfunc.count.02
"""

import pytest
from firebird.qa import *

init_script = """
    create table test( id integer);
    insert into test values(0);
    insert into test values(0);
    insert into test values(null);
    insert into test values(null);
    insert into test values(null);
    insert into test values(1);
    insert into test values(1);
    insert into test values(1);
    insert into test values(1);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select count(*) as cnt_all, count(id) as cnt_nn, count(distinct id) as cnt_unq from test;
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CNT_ALL 9
    CNT_NN  6
    CNT_UNQ 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

