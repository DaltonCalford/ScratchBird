#coding:utf-8

"""
ID:          issue-5662
ISSUE:       5662
TITLE:       Query cannot be executed if win1251 characters used in field aliases
DESCRIPTION:
JIRA:        CORE-5389
FBTEST:      bugs.core_5389
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('.*After line [0-9]+ in file .*', '')])

expected_stdout = """
    ФИО
    Д.рождения
    Город
    Расшифровка выступлений
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22018
    arithmetic exception, numeric overflow, or string truncation
    -Cannot transliterate character between character sets
"""

test_script = """
    set bail on;
    set list on;
    -- set names win1251;

    select
       '' as "ФИО"
      ,'' as "Д.рождения"
      ,'' as "Город"
      ,'' as "Расшифровка выступлений"
    from rdb$database;
"""

script_file = temp_file('test-script.sql')

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='cp1251')
    # check-1:  attempt to apply DDL with non-ascii characters __WITHOUT__ charset specifying (for ISQL)
    act.expected_stderr = expected_stderr
    act.isql(switches=['-q'], input_file=script_file, charset=None)
    assert act.clean_stderr == act.clean_expected_stderr
    # check-2:  attempt to apply DDL with non-ascii characters ___WITH___ specifying: ISQL ... -ch WIN1251
    act.reset()
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input_file=script_file, charset='WIN1251')
    assert act.clean_stdout == act.clean_expected_stdout
