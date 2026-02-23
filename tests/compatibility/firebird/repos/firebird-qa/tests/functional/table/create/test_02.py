#coding:utf-8

"""
ID:          table.create-02
TITLE:       CREATE TABLE - column properties
DESCRIPTION:
FBTEST:      functional.table.create.02
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE fk(id INT NOT NULL PRIMARY KEY);
commit;
"""

db = db_factory(init=init_script)

test_script = """CREATE TABLE test(
 c1 SMALLINT NOT NULL,
 c2 INTEGER DEFAULT 0,
 c3 FLOAT NOT NULL UNIQUE,
 c4 DOUBLE PRECISION NOT NULL PRIMARY KEY,
 c5 INT REFERENCES fk(id),
 c6 INT CHECK (c6>c5),
 c7 COMPUTED (c1+c2),
 c8 CHAR(31) DEFAULT USER,
 c9 VARCHAR(40) DEFAULT 'data'
);
SHOW TABLE test;
"""

act = isql_act('db', test_script)

expected_stdout = """C1                              SMALLINT Not Null
C2                              INTEGER Nullable DEFAULT 0
C3                              FLOAT Not Null
C4                              DOUBLE PRECISION Not Null
C5                              INTEGER Nullable
C6                              INTEGER Nullable
C7                              Computed by: (c1+c2)
C8                              CHAR(31) Nullable DEFAULT USER
C9                              VARCHAR(40) Nullable DEFAULT 'data'
CONSTRAINT INTEG_8:
  Foreign key (C5)    References FK (ID)
CONSTRAINT INTEG_7:
  Primary key (C4)
CONSTRAINT INTEG_5:
  Unique key (C3)
CONSTRAINT INTEG_9:
CHECK (c6>c5)
"""

@pytest.mark.skip("Test will be reimplemented.")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
