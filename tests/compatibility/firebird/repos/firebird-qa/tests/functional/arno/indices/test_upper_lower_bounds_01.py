#coding:utf-8

"""
ID:          index.upper-lower-bounds-01
TITLE:       Upper and lower bounds
DESCRIPTION: Equal comparison should be prefered.
  Lower and Upper bounds are bound by the same value.
FBTEST:      functional.arno.indices.upper_lower_bounds_01
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE Table_1000 (
  ID INTEGER NOT NULL
);

SET TERM ^^ ;
CREATE PROCEDURE PR_FillTable_1000
AS
DECLARE VARIABLE FillID INTEGER;
BEGIN
  FillID = 1;
  WHILE (FillID <= 1000) DO
  BEGIN
    INSERT INTO Table_1000 (ID) VALUES (:FillID);
    FillID = FillID + 1;
  END
END
^^
SET TERM ; ^^

COMMIT;

EXECUTE PROCEDURE PR_FillTable_1000;

COMMIT;

CREATE UNIQUE ASC INDEX PK_Table_1000 ON Table_1000 (ID);

COMMIT;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    sequential = {}
    indexed = {}
    with act.db.connect() as con:
        for tbl in con.info.get_table_access_stats():
            if tbl.table_id >= 128:
                sequential[tbl.table_id] = tbl.sequential
                indexed[tbl.table_id] = tbl.indexed
        with con.cursor() as c:
            c.execute("SELECT Count(*) FROM Table_1000 t1000 WHERE t1000.ID > 1 and t1000.ID >= 100 and t1000.ID = 500 and t1000.ID <= 900 and t1000.ID < 1000")
            cnt = c.fetchone()[0]
        for tbl in con.info.get_table_access_stats():
            if tbl.table_id >= 128:
                if tbl.sequential:
                    sequential[tbl.table_id] = tbl.sequential - sequential.get(tbl.table_id, 0)
                if tbl.indexed:
                    indexed[tbl.table_id] = tbl.indexed - indexed.get(tbl.table_id, 0)
    # Check
    assert cnt == 1
    assert sequential == {}
    assert indexed == {128: 1}
