#coding:utf-8

"""
ID:          issue-c8849946
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/c884994653523d7d6d614075af45c7ecf338008e
TITLE:       Scrollable cursors. Inconsistent cursor repositioning
DESCRIPTION:
    Test case provided by dimitr, see letters with subj = "firebird-driver & scrollable cursors".
    Date: 01.12.2021 08:20.
NOTES:
    [19.07.2024] pzotov
    1. No ticket has been created for described problem.
       Problem was fixed 30.11.2021 at 16:55, commit = push = c8849946
    2. Confirmed bug on 5.0.0.324. Fixed in 5.0.0.325-c884994
    Checked on 6.0.0.396, 5.0.1.1440.
"""

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

N_ROWS = 1010
#############

def print_row(row, cur = None):
    if row:
        print(f"{row[0]}")
        if cur and (cur.is_bof() or cur.is_eof()):
            print('### STRANGE BOF/EOR WHILE SOME DATA CAN BE SEEN ###')
    else:
        msg = '*** NO_DATA***'
        if cur:
            msg += '  BOF=%r    EOF=%r' % ( cur.is_bof(), cur.is_eof() )
        print(msg)

#----------------------------

@pytest.mark.scroll_cur
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        cur = con.cursor()
        cur.open(f'select row_number()over() as id from rdb$types, rdb$types rows {N_ROWS}')

        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)

        print_row(cur.fetch_last(), cur)
        print_row(cur.fetch_next(), cur)

        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_prior(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)
        print_row(cur.fetch_next(), cur)


    act.expected_stdout = """
        1
        2
        3
        2
        1
        *** NO_DATA***  BOF=True    EOF=False
        1
        2
        3
        2
        1
        *** NO_DATA***  BOF=True    EOF=False
        1010
        *** NO_DATA***  BOF=False    EOF=True
        1010
        1009
        1008
        1009
        1010
        *** NO_DATA***  BOF=False    EOF=True
        1010
        1009
        1008
        1009
        1010
        *** NO_DATA***  BOF=False    EOF=True
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
