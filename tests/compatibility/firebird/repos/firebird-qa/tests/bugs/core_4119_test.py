#coding:utf-8

"""
ID:          issue-4447
ISSUE:       4447
TITLE:       Metadata source becomes wrong after twice transliteration to the metadata charset
DESCRIPTION:
JIRA:        CORE-4119
FBTEST:      bugs.core_4119
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(charset='WIN1251')

act = python_act('db', substitutions=[('=.*', '')])
script_file = temp_file('test_script.sql')

expected_stdout_5x = """
    Procedure text:
    begin
    -- Моя процедура
    end
"""

expected_stdout_6x = """
    Procedure: PUBLIC.MYPROC
    Procedure text:
    begin
    -- Моя процедура
    end
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, script_file: Path):
    sp_ddl = """
        set term ^;
        create procedure myproc as
        begin
            -- Моя процедура
        end^
        set term ;^
        show procedure myproc;
    """

    script_file.write_text(sp_ddl, encoding='cp1251')

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], input_file = script_file, charset='win1251')
    assert act.clean_stdout == act.clean_expected_stdout

