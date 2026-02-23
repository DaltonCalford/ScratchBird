#coding:utf-8

"""
ID:          issue-6064
ISSUE:       6064
TITLE:       Field name max length check wrongly if national characters specified
DESCRIPTION:
  Confirmed bug on 3.0.4.32972, got error:
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 31, actual 31

  Though this ticket was fixed only for FB 4.x, Adriano notes that error message
  was corrected in FB 3.0.6. Thus we check both major versions but use different
  length of columns: 32 and 64.
JIRA:        CORE-5802
FBTEST:      bugs.core_5802
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(charset='WIN1251')

act = python_act('db', substitutions=[('[-]?At line \\d+.*', ''), ('After line \\d+.*', '')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Name longer than database column size
"""

test_script = temp_file('test_script.sql')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, test_script: Path):
    if act.is_version('<4'):
        # Maximal number of characters in the column for FB 3.x is 31.
        # Here we use name of 32 characters and this must raise error
        # with text "Name longer than database column size":
        column_title = 'СъешьЖеЕщёЭтихМягкихФранкоБулок'
    else:
        # Maximal number of characters in the column for FB 4.x is 63.
        # Here we use name of 64 characters and this must raise error
        # with text "Name longer than database column size":
        column_title = 'СъешьЖеЕщёЭтихПрекрасныхФранкоБулокВместоДурацкихМорковныхКотлет'
    # Code to be executed further in separate ISQL process:
    test_script.write_text(f"""
    set list on;
    set sqlda_display on;
    -- Maximal number of characters in the column for FB 3.x is 31.
    -- Here we use name of 32 characters and this must raise error
    -- with text "Name longer than database column size":
    select 1 as "{column_title}" from rdb$database;
    """, encoding='cp1251')
    #
    act.expected_stderr = expected_stderr
    act.isql(switches=['-q'], input_file=test_script, charset='WIN1251')
    assert act.clean_stderr == act.clean_expected_stderr
