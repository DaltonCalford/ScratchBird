#coding:utf-8

"""
ID:          issue-6954
ISSUE:       6954
TITLE:       Add fb_info_protocol_version information request to Attachment::getInfo().
DESCRIPTION:
    We attempt to obtain DbInfoCode.PROTOCOL_VERSION and print only the fact that we could do that
    (instead of its concrete value which, of course can change).
NOTES:
    Improvement was committed:
    * in FB 4.x: 15.09.2021 18:25, cb2d8dfb (4.0.1.2602)
    * in FB 5.x: 09.09.2021 17:27, 18d59a5e (5.0.0.196)
    Before these snapshots attempt to obtain protocol version caused error:
    ======
    raise InterfaceError("An error response was received")
    firebird.driver.types.InterfaceError: An error response was received
    ======

    Checked on 6.0.0.396, 5.0.1.1440, 4.0.53127.
"""
import pytest
from firebird.qa import *
from firebird.driver import DbInfoCode

db = db_factory()
act = python_act('db') #, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=4.0.1')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        print( con.info.get_info(DbInfoCode.PROTOCOL_VERSION) > 0 )

    act.expected_stdout = 'True'
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
