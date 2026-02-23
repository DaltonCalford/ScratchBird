#coding:utf-8

"""
ID:          issue-7045
ISSUE:       7045
TITLE:       International characters in table or alias names causes queries of MON$STATEMENTS to fail
DESCRIPTION:
  Blob column mon$explained_plan seems to be source of problem, but only for mon$statements table.
  Other blob columns (e.g. rdb$exceptions.rdb$description or sec$users.sec$description) not affected.

  Confirmed bug on 5.0.0.310, got:
  firebird.driver.types.DatabaseError: Cannot transliterate character between character sets

  Checked on 5.0.0.311 - all fine.
FBTEST:      bugs.gh_7045
NOTES:
    [24.07.2022] pzotov
    Reproduced problem on 5.0.0.310. Checked on 5.0.0.591 - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.intl
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    with act.db.connect(charset='iso8859_1') as con:
        cur=con.cursor()
        chk_list = list('ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ')
        # for every non-ascii character from chk_list its usage
        # as column alias must NOT issue any problem:
        for non_ascii_char in chk_list:
            column_alias = non_ascii_char * 63
            stm = f"""
                select mon$explained_plan -- BLOB SUB_TYPE 1 SEGMENT SIZE 80 CHARACTER SET UTF8
                from mon$statements as "{column_alias}" where mon$attachment_id = current_connection
                ;
            """
            cur.execute(stm)
            for r in cur:
                pass

    act.expected_stdout = ''
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
