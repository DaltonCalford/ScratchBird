#coding:utf-8

"""
ID:          1ba0086e
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/1ba0086e136279d2ed6ddb043e67c709cf10d490
TITLE:       Add optional COLUMN to ALTER TABLE ... ADD and DROP
DESCRIPTION:
NOTES:
    [01.09.2025] pzotov
    Checked on 6.0.0.1261-8d5bb71.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set bail on;
    recreate table test(id int default 1);
    alter table test
        add column x int
       ,add column y int
       ,add column z computed by(x+x)
       ,drop column y
       ,drop column z
       ,drop column x;
    insert into test default values;
    select * from test;
"""
substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        ID 1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
