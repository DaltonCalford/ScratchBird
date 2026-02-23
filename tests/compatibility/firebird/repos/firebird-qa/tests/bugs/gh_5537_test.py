#coding:utf-8

"""
ID:          issue-5537
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5537
TITLE:       Non US-ASCII field names treated as unicode, although charset non-unicode, lowering max field length [CORE5258]
DESCRIPTION:
NOTES:
    [09.11.2024] pzotov
    FB-3.x must raise "Name longer than database column size", all others must work fine.
    Checked on 3.0.13.33794, 4.0.6.3165, 5.0.2.1553, 6.0.0.520
"""
from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [(r'After line \d+ .*', '')])
tmp_file = temp_file('tmp_5537.sql')

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_file: Path):
    
    NON_ASCII_TXT = 'Поле в 26 символов!'
    tmp_file.write_bytes(f"""set list on; select '' as "{NON_ASCII_TXT}" from rdb$database;""".encode('cp1251'))

    expected_3x = """
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Name longer than database column size
    """
    expected_4x = f"{NON_ASCII_TXT}"

    act.expected_stdout = expected_3x if act.is_version('<4') else expected_4x
    act.isql(switches = ['-q'], input_file = tmp_file, charset = 'win1251', io_enc = 'cp1251', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
