#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/f8cb4a6ec0a315ade057bbbdd819ea924cce93cc
TITLE:       Correct error message on non-positioned cursor
DESCRIPTION:
    Check error message when cursor is not yet positioned on valid record (i.e. is in BOF state).
NOTES:
    [11.09.2025] pzotov
    1. Fixed in 6.x: https://github.com/FirebirdSQL/firebird/commit/11d5d592430d855a150d7297e9e5a634ddae8517
       (within big push related to #8145, date: 07-may-2025; snapshot: 6.0.0.778-d735e65)
       Before fix err.gds_codes list was: (335544569, 335544436, 335544336, 335544451)
       ("deadlock / udate conflicts with concurrent update")
       Discussed with Dm. Sibiryakov, letters since 10.08.2024 20:52.
    2. See also:
       * Test for https://github.com/FirebirdSQL/firebird/issues/7057
       * https://github.com/FirebirdSQL/firebird-qa/pull/31 ("Add checks for errors condition").
    3. The problem (wrong error message with "deadlock / udate conflicts") still exists on 5.0.4.

    Checked on 6.0.0.1266.
"""
import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_sql = """
    recreate table ts(id int);
    commit;
    insert into ts (id) select row_number() over() from rdb$types rows 10;
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db', substitutions=[('[ \t]+', ' ')])

@pytest.mark.scroll_cur
@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        cur.open('select id from ts for update')
        cur.set_cursor_name('X')
        try:
            # NB: cursor not yet fetched now! This must raise
            # "Cursor X is not positioned in a valid record":
            con.execute_immediate('update ts set id = -id where current of X')
            # cur.execute('update ts set id = -id where current of X')
        except Exception as err:
            print(err)
            print(f'{err.gds_codes=}')
            print(f'{err.sqlcode=}')
            print(f'{err.sqlstate=}')

    act.stdout = capsys.readouterr().out
    act.expected_stdout = """
        Dynamic SQL Error
        -Cursor X is not positioned in a valid record
        err.gds_codes=(335544569, 335545092)
        err.sqlcode=-902
        err.sqlstate='HY109'
    """
    assert act.clean_stdout == act.clean_expected_stdout
