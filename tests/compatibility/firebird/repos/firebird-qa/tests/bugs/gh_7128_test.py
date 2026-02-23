#coding:utf-8
"""
ID:          issue-7128
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7128
TITLE:       Incorrect error message with isc_sql_interprete()
DESCRIPTION: 
NOTES:
    [28.03.2024] pzotov
        Bug caused crash of FB up to 5.0.0.890 (10-jan-2023).
        Since 5.0.0.905 (11-jan-2023) following error raises:
            Invalid resultset interface
            -901
            335545049

    [03.09.2024] pzotov
    1. Warning is issued:
          $PYTHON_HOME/Lib/site-packages/firebird/driver/interfaces.py:710: FirebirdWarning: Invalid resultset interface
          self._check()
       It was decided to suppress warning by using 'warnings' package.

    2. Result for snapshots with date = 09-feb-2022:
        3.0.9.33560:
            Exception ignored in: <function Cursor.__del__ at 0x000001DD87EA49A0>
            Traceback (most recent call last):
              File "$PYTHON_HOME/Lib/site-packages/firebird/driver/core.py", line 3047, in __del__
              File "$PYTHON_HOME/Lib/site-packages/firebird/driver/core.py", line 3788, in close
              File "$PYTHON_HOME/Lib/site-packages/firebird/driver/core.py", line 3655, in _clear
              File "$PYTHON_HOME/Lib/site-packages/firebird/driver/interfaces.py", line 709, in close
            OSError: exception: access violation writing 0x0000000000000024
        4.0.1.2175: passed.
        5.0.0.393: crashed,
            > raise self.__report(DatabaseError, self.status.get_errors())
            E firebird.driver.types.DatabaseError: Error writing data to the connection.
            E -send_packet/send
    3. Version 3.0.13.33793 raises:
            > raise self.__report(DatabaseError, self.status.get_errors())
            E firebird.driver.types.DatabaseError: Invalid resultset interface
       (and this exceprion is not catched for some reason).

    Checked on 6.0.0.447, 5.0.2.1487, 4.0.6.3142
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraLockResolution, TraAccessMode, DatabaseError, FirebirdWarning
import time
import warnings

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    tpb_isol_set = (Isolation.SERIALIZABLE, Isolation.SNAPSHOT, Isolation.READ_COMMITTED_READ_CONSISTENCY, Isolation.READ_COMMITTED_RECORD_VERSION, Isolation.READ_COMMITTED_NO_RECORD_VERSION)

    with act.db.connect() as con:
        for x_isol in tpb_isol_set:
            custom_tpb = tpb(isolation = x_isol, lock_timeout = 0)
            tx = con.transaction_manager(custom_tpb)
            cur = tx.cursor()
            tx.begin()
            with warnings.catch_warnings():
                warnings.filterwarnings('ignore')
                try:
                    print(x_isol.name)
                    cur.execute('select 0 from rdb$types rows 2')
                    cur.fetchone()
                    tx._cursors = []
                    tx.commit()
                    cur.fetchone()
                except DatabaseError as e:
                    print(e.__str__())
                    print(e.sqlcode)
                    for g in e.gds_codes:
                        print(g)
                finally:
                    cur.close()

            act.expected_stdout = f"""
                {x_isol.name}
                Invalid resultset interface
                -901
                335545049
            """
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
