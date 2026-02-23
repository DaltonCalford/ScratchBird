#coding:utf-8

"""
ID:          fkey.unique.select-02
FBTEST:      functional.fkey.unique.select_uf_02
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  Master transaction is perform select with lock but not perform fetch.
  Detail transaction inserts record in detail_table and commit;
  Master transaction fetched record and trying update it;
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, tpb, Isolation

init_script = """CREATE TABLE MASTER_TABLE (
    ID     INTEGER PRIMARY KEY,
    UF     INTEGER UNIQUE,
    INT_F  INTEGER
);

CREATE TABLE DETAIL_TABLE (
    ID    INTEGER PRIMARY KEY,
    FKEY  INTEGER
);

ALTER TABLE DETAIL_TABLE ADD CONSTRAINT FK_DETAIL_TABLE FOREIGN KEY (FKEY) REFERENCES MASTER_TABLE (UF);
COMMIT;
INSERT INTO MASTER_TABLE (ID, UF, INT_F) VALUES (1, 1, 10);
commit;"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        cust_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
        con.begin(cust_tpb)
        with con.cursor() as c:
            c.execute("SELECT UF, INT_F FROM MASTER_TABLE WHERE ID=1 WITH LOCK")
            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cd:
                    cd.execute("INSERT INTO DETAIL_TABLE (ID, FKEY) VALUES (1,1)")
                con_detail.commit()
                c.fetchall()
                with pytest.raises(DatabaseError,
                                   match='.*Foreign key references are present for the record.*'):
                    c.execute("UPDATE MASTER_TABLE SET UF=2")
    # Passed.
