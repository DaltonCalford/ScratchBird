#coding:utf-8

"""
ID:          issue-8746
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8746
TITLE:       Allow isc_tpb_read_consistency to imply read committed
DESCRIPTION:
NOTES:
    [25.09.2025] pzotov
    Problem can *not* be seen in ISQL because it forces to specify 'READ COMMITTED' before 'READ CONSISTENCY'
    and passes TWO tags into TPB: isc_tpb_read_committed + isc_tpb_read_consistency.

    In contrary, if we start tx using Python firebird-driver then only one flag (isc_tpb_read_consistency)
    is written into TPCB.
    Thanks to dimitr for suggestion.

    Confirmed problem on 6.0.0.1277: query to rdb$get_context('SYSTEM', 'ISOLATION_LEVEL') 
    returns for transaction with TIL = READ_COMMITTED_READ_CONSISTENCY value 'SNAPSHOT'
    rather than 'READ COMMITTED'.

    Checked on 6.0.0.1282, 5.0.4.1717.
"""
import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, DatabaseError

db = db_factory()
act = python_act('db', substitutions = [('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.4')
def test_1(act: Action, capsys):

    test_sql = """
        select t.mon$isolation_mode, rdb$get_context('SYSTEM', 'ISOLATION_LEVEL')
        from mon$transactions t
        where t.mon$transaction_id = current_transaction;
    """

    try:
        custom_tpb = tpb(isolation = Isolation.READ_COMMITTED_READ_CONSISTENCY)
        with act.db.connect() as con:
            tx = con.transaction_manager(custom_tpb)
            cur = tx.cursor()
            cur.execute(test_sql)
            hdr=cur.description
            for r in cur:
                for i in range(0,len(hdr)):
                    print( hdr[i][0].ljust(32),':', r[i] )
            
    except DatabaseError as e:
        print(e.__str__())
        for g in e.gds_codes:
            print(g)

    act.expected_stdout = f"""
        MON$ISOLATION_MODE : 4
        RDB$GET_CONTEXT : READ COMMITTED
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
