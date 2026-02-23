#coding:utf-8

"""
ID:          fkey.primary.select-01
FBTEST:      functional.fkey.primary.select_pk_01
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  Master transaction is perform select with lock and fetch record.
  Detail transaction inserts record in detail_table.
  Expected: no errors.
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
commit;"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        cust_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
        con.begin(cust_tpb)
        with con.cursor() as c:
            c.execute("SELECT INT_F FROM MASTER_TABLE WHERE ID=1 WITH LOCK").fetchall()
            con.commit()
            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cd:
                    cd.execute("INSERT INTO DETAIL_TABLE (ID, FKEY) VALUES (1,1)")
                con_detail.commit()
    # Passed.
