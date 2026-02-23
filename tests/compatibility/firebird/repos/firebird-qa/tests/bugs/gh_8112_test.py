#coding:utf-8

"""
ID:          issue-8112
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8112
TITLE:       Error isc_read_only_trans (335544361) should report SQLSTATE 25006.
DESCRIPTION:
NOTES:
    Confirmed problem on 6.0.0.345,  5.0.1.1395.
    Checked on 6.0.0.351, 5.0.1.1399.
"""
import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraLockResolution, TraAccessMode, DatabaseError

init_sql = """
    recreate table test(id int default current_transaction);
"""
db = db_factory(init = init_sql)
act = python_act('db', substitutions = [('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, capsys):

    try:
        custom_tpb = tpb(isolation = Isolation.SERIALIZABLE, access_mode = TraAccessMode.READ)
        with act.db.connect() as con:
            tx = con.transaction_manager(custom_tpb)
            cur = tx.cursor()
            cur.execute('insert into test default values')
            con.commit()
            
    except DatabaseError as e:
        print(e.sqlstate) # must be '25006' after fix.
        print(e.__str__())
        for g in e.gds_codes:
            print(g)

    act.expected_stdout = f"""
        25006
        attempted update during read-only transaction
        335544361
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
