#coding:utf-8

"""
ID:          issue-6394
ISSUE:       6394
TITLE:       Wrong result in "similar to" with non latin characters
DESCRIPTION:
JIRA:        CORE-6145
FBTEST:      bugs.core_6145
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(charset='UTF8')

act = python_act('db')

test_script = """
set bail on;
set list on;
set count on;
    set heading off;
    -- NB: When this script is Python variable then we have to use DUPLICATE percent signs!
    -- Otherwise get: "not enough arguments for format string"
    select 1 result_1 from rdb$database where 'я' similar to '%%Я%%';
    select 2 result_2 from rdb$database where 'Я' similar to '%%я%%';
    select 3 result_3 from rdb$database where 'я' similar to '[Яя]';
select 4 result_4 from rdb$database where 'Я' similar to 'я';
select 5 result_5 from rdb$database where 'Я' similar to 'Я';
select 6 result_6 from rdb$database where 'Я' similar to '[яЯ]';
"""

expected_stdout = """
    Records affected: 0
    Records affected: 0
    RESULT_3                        3
    Records affected: 1
    Records affected: 0
    RESULT_5                        5
    Records affected: 1
    RESULT_6                        6
    Records affected: 1
"""

script_file = temp_file('test-script.sql')

@pytest.mark.version('>=2.5')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='cp1251')
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input_file=script_file, charset='WIN1251')
    assert act.clean_stdout == act.clean_expected_stdout
