#coding:utf-8

"""
ID:          issue-2366
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2366
TITLE:       ISQL: bad headers when text columns has >= 80 characters
DESCRIPTION:
JIRA:        CORE-6438
FBTEST:      bugs.core_6438
NOTES:
    [19.03.2025] pzotov
    Fix https://github.com/FirebirdSQL/firebird/commit/37a42a6093077c9156a5853cfe69bba1ea92a468 (08-nov-2020)
    Confirmed bug on 3.0.7.33388 (07-nov-2020), 4.0.0.2240 (04-nov-2020).
    Checked on 4.0.0.2249 (09-nov-2020)
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

PAD_TO_WIDTH = 65533

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    data = '1' * PAD_TO_WIDTH
    test_sql = f"""
        set list off;
        select '{data}' as col_1, 1 as col_2 from rdb$database;
    """
    act.isql(switches=[], input = test_sql, combine_output = True)
    hdr_len = txt_len = 0
    for line in act.stdout.splitlines():
        if line.startswith('='):
            # before fix: hdr_len=79 instead of expected {PAD_TO_WIDTH}
            hdr_len = len(line.split()[0])
        elif line.startswith('1'):
            txt_len = len(line.split()[0])
    act.reset()

    print('hdr_len:', hdr_len)
    print('txt_len:', txt_len)

    expected_stdout = f"""
        hdr_len: {PAD_TO_WIDTH}
        txt_len: {PAD_TO_WIDTH}
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
