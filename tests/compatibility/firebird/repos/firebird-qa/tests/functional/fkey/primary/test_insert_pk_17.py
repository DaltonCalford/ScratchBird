#coding:utf-8

"""
ID:          fkey.primary.insert-17
FBTEST:      functional.fkey.primary.insert_pk_17
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  1 Master transaction modifies non key field.
  2 Detail transaction inserts record.
  3 Master transaction modifies primary key.
  Expected: referential integrity error
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, tpb, Isolation

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
            c.execute("update master_table set int_f = 10 WHERE ID=1")
            #Create second connection for change detail table
            with act.db.connect() as con_detail:
                con_detail.begin(cust_tpb)
                with con_detail.cursor() as cd:
                    cd.execute("INSERT INTO DETAIL_TABLE (ID, FKEY) VALUES (1,1)")
                with pytest.raises(DatabaseError,
                                   match='.*Foreign key references are present for the record.*'):
                    c.execute("update master_table set ID=10 WHERE ID=1")
    # Passed.
