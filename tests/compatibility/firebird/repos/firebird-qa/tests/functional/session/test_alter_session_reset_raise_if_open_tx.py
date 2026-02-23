#coding:utf-8

"""
ID:          session.alter-session-reset-raise-if-open-tx
ISSUE:       6093
TITLE:       ALTER SESSION RESET: throw error (isc_ses_reset_err) if any open transaction exist in current connection
DESCRIPTION:
  Test issue about ALTER SESSION RESET:
  "throw error (isc_ses_reset_err) if any open transaction exist in current connection except
  of current transaction..."

  We start three transactions within the same connection. First of them runs 'ALTER SESSION RESET'.
  It must fail with error with phrase about active transactions that prevent from doing this action.

  NOTE: this test does NOT check admissibility of session reset when prepared 2PC transactions exist.
  It checks only ERROR RAISING when there are several Tx opened within the same connection.
FBTEST:      functional.session.alter_session_reset_raise_if_open_tx
JIRA:        CORE-5832

NOTES: checked on 4.0.1.2692, 5.0.0.489
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db')

expected_stdout = """
    Cannot reset user session
    -There are open transactions (3 active)
    gdscodes: (335545206, 335545207)
    sqlcode: -901
    sqlstate: 01002
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        with con.transaction_manager() as tx1, \
             con.transaction_manager() as tx2, \
             con.transaction_manager() as tx3:
            tx1.begin()
            tx2.begin()
            tx3.begin()

            cur1=tx1.cursor()
            try:
                cur1.execute('alter session reset')
            except DatabaseError as e:
                print(e.__str__())
                print('gdscodes:', e.gds_codes)
                print('sqlcode:',  e.sqlcode)
                print('sqlstate:', e.sqlstate)

    act.stdout = capsys.readouterr().out
    act.expected_stdout = expected_stdout
    assert act.clean_stdout == act.clean_expected_stdout
