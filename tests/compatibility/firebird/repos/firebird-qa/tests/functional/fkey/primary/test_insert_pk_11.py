#coding:utf-8

"""
ID:          fkey.primary.insert-11
FBTEST:      functional.fkey.primary.insert_pk_11
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  Master table has primary key consisting of several fields.
  Master transaction modifies all primary key fields and committed.
  Detail transaction inserts record in detail_table.
  Expected: no errors.
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

init_script = """CREATE TABLE MASTER_TABLE (
    ID_1 INTEGER NOT NULL,
    ID_2 VARCHAR(20) NOT NULL,
    INT_F  INTEGER,
    PRIMARY KEY (ID_1, ID_2)
);

CREATE TABLE DETAIL_TABLE (
    ID    INTEGER PRIMARY KEY,
    FKEY_1  INTEGER,
    FKEY_2  VARCHAR(20)
);

ALTER TABLE DETAIL_TABLE ADD CONSTRAINT FK_DETAIL_TABLE FOREIGN KEY (FKEY_1, FKEY_2) REFERENCES MASTER_TABLE (ID_1, ID_2);
COMMIT;
INSERT INTO MASTER_TABLE (ID_1, ID_2, INT_F) VALUES (1, 'one', 10);
COMMIT;"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        cust_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
        con.begin(cust_tpb)
        with con.cursor() as c:
            c.execute("UPDATE MASTER_TABLE SET ID_1=2 WHERE ID_1=1")
            c.execute("UPDATE MASTER_TABLE SET ID_2='two' WHERE ID_2='one'")
            con.commit()
            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cd:
                    cd.execute("INSERT INTO DETAIL_TABLE (ID, FKEY_1, FKEY_2) VALUES (1, 2, 'two')")
                con_detail.commit()
    # Passed.
