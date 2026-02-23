#coding:utf-8

"""
ID:          fkey.primary.update-01
FBTEST:      functional.fkey.primary.upd_pk_01
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  Master table has one key field. Master transaction doesn't modify key field.
  Detail transaction updates record in detail_table record.
  Expected: no errors
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

init_script = """CREATE TABLE MASTER_TABLE (
    ID     INTEGER PRIMARY KEY,
    INT_F  INTEGER
);

CREATE TABLE DETAIL_TABLE (
    ID    INTEGER PRIMARY KEY,
    FKEY  INTEGER
);

ALTER TABLE DETAIL_TABLE ADD CONSTRAINT FK_DETAIL_TABLE FOREIGN KEY (FKEY) REFERENCES MASTER_TABLE (ID);
COMMIT;
INSERT INTO MASTER_TABLE (ID, INT_F) VALUES (1, 10);
INSERT INTO DETAIL_TABLE (ID, FKEY) VALUES (1, 1);
COMMIT;"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        cust_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
        con.begin(cust_tpb)
        with con.cursor() as c:
            c.execute("update master_table set int_f = 10")
            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cd:
                    cd.execute("UPDATE DETAIL_TABLE SET ID=2 WHERE ID=1")
                con_detail.commit()
    # Passed.
