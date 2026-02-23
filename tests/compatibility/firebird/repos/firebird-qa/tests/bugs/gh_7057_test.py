#coding:utf-8

"""
ID:          issue-7057
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7057
TITLE:       Client-side positioned updates work wrongly with scrollable cursors
DESCRIPTION:
    Fetching from a scrollable cursor may overwrite user-specified buffer and corrupt memory.
    Engine did overwrite the user-specified buffer with four more bytes than expected that could corrupt the caller memory.
    Discussed between dimitr, pcisar and pzotov, see letters of 29-30 NOV 2021,
    subj: "firebird-driver & scrollable cursors // misc. tests, requested by dimitr"
NOTES:
    [29.07.2024] pzotov
    1. ### ACHTUNG ###
       Old snapshots (before 5.0.0.890-aa847a7) must be checked with usage "--disable-db-cache" command switch for pytest!
       Otherwise one may FALSE failure (bugcheck) with:
       "internal Firebird consistency check (decompression overran buffer (179), file: sqz.cpp line: 293)"
    2. Test caused crash of server on snapshots before 6.0.0.401-a7d10a4.
       Problem related to MaxStatementCacheSize which default value > 0
       (explained by dimitr, letter 19-JUL-2024 12:52).
       
       It seems that bug was fixed in:
           FB 5.x: https://github.com/FirebirdSQL/firebird/commit/08dc25f8c45342a73c786bc60571c8a5f2c8c6e3 (27.07.2024 14:55)
           FB 6.x: https://github.com/FirebirdSQL/firebird/commit/a7d10a40147d326e56540498b50e40b2da0e5850 (29.07.2024 03:53)
           ("Fix #8185 - SIGSEGV with WHERE CURRENT OF statement with statement cache turned on.")

    3. Attempt to run this test on FB 4.0.5.3127 (10-JUL-2024) raises:
           "E firebird.driver.types.DatabaseError: feature is not supported"
       (scollable cursors are not supported in network protocol in FB-4.x)
    4. See also: functional/tabloid/test_f8cb4a6e.py

    Checked on 6.0.0.401-a7d10a4, 5.0.1.1453-62ee5f1.
"""
import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol, DatabaseError
import re

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])

#------------------------------------------------------
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
#------------------------------------------------------


@pytest.mark.scroll_cur
@pytest.mark.version('>=5.0.1')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        con.execute_immediate('recreate table ts(id int)')
        con.commit()
        con.execute_immediate('insert into ts (id) select row_number() over() from rdb$types rows 10')
        con.commit()

        cur = con.cursor()
        cur.open('select id from ts for update')
        cur.set_cursor_name('X')

        for row in cur:
            print_row(row)

        cur.fetch_first()
        print('Updating first record')
        con.execute_immediate('update ts set id = -id where current of X')
        con.commit()

        cur = con.cursor()
        cur.open('select id from ts for update')
        cur.set_cursor_name('X')

        for row in cur:
            print_row(row)

        cur.fetch_last()
        print('Updating last record')
        con.execute_immediate('update ts set id = -id where current of X')
        con.commit()

        cur = con.cursor()
        cur.open('select id from ts for update')
        cur.set_cursor_name('X')

        for row in cur:
            print_row(row)

        cur.fetch_absolute(5)
        print('Updating 5th record')
        con.execute_immediate('update ts set id = -id where current of X')
        con.commit()

        cur = con.cursor()
        cur.open('select id from ts')

        for row in cur:
            print_row(row)

    act.stdout = capsys.readouterr().out
    act.expected_stdout = """
        1
        2
        3
        4
        5
        6
        7
        8
        9
        10
        Updating first record
        -1
        2
        3
        4
        5
        6
        7
        8
        9
        10
        Updating last record
        -1
        2
        3
        4
        5
        6
        7
        8
        9
        -10
        Updating 5th record
        -1
        2
        3
        4
        -5
        6
        7
        8
        9
        -10
    """
    assert act.clean_stdout == act.clean_expected_stdout
