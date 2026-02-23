#coding:utf-8

"""
ID:          fkey.unique.insert-06
FBTEST:      functional.fkey.unique.insert_06
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  Master table has one primary key field and one unique field. Master transaction doesn't modifiy unique field and commit
  Detail transaction inserts record in detail_table.
  Expected: no errors
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

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
            c.execute("UPDATE MASTER_TABLE SET UF=null WHERE ID=1")
            con.commit()
            con.begin(cust_tpb)
            c.execute("UPDATE MASTER_TABLE SET INT_F=10")
            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cd:
                    cd.execute("INSERT INTO DETAIL_TABLE (ID, FKEY) VALUES (1,null)")
                con_detail.commit()
    # Passed.
