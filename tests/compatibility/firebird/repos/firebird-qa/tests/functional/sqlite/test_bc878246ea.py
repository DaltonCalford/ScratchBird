#coding:utf-8

"""
ID:          bc878246ea
ISSUE:       https://www.sqlite.org/src/tktview/bc878246ea
TITLE:       Incorrect result from LEFT JOIN query
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (id integer primary key not null);
    insert into test values (1);
    set count on;
    select t1.id as id_1, t2.id as id_2
    from test as t1
    left join test as t2 on t2.id between 10 and 20
    join test as t3 on (t3.id = t1.id or t2.id is not null and t3.id = t2.id);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID_1 1
    ID_2 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
