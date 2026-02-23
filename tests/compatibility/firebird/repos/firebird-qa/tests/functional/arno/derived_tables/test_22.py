#coding:utf-8

"""
ID:          derived-table-22
TITLE:       Derived table outer reference (triggers)
DESCRIPTION: NEW/OLD context variables should be available inside the derived table.
FBTEST:      functional.arno.derived_tables.22
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TABLEA
(
  ID INTEGER NOT NULL,
  DESCRIPTION VARCHAR(50),
  PARENTID INTEGER,
  CONSTRAINT PK_TABLEA PRIMARY KEY (ID),
  CONSTRAINT FK_TABLEA_TABLEA
    FOREIGN KEY (PARENTID) REFERENCES TABLEA (ID)
    ON DELETE NO ACTION ON UPDATE CASCADE
);

COMMIT;

SET TERM ^^ ;
CREATE TRIGGER TABLEA_BI FOR TABLEA ACTIVE BEFORE INSERT POSITION 0 AS
DECLARE PREV_ID INTEGER;
BEGIN
  PREV_ID = NULL;
  SELECT ID FROM (SELECT MAX(a.ID) AS ID FROM TABLEA a WHERE a.ID < NEW.ID) INTO :PREV_ID;
  NEW.PARENTID = PREV_ID;
END
^^
SET TERM ; ^^


SET TERM ^^ ;
CREATE TRIGGER TABLEA_BU FOR TABLEA ACTIVE BEFORE UPDATE POSITION 0 AS
DECLARE PREV_ID INTEGER;
BEGIN
  PREV_ID = NULL;
  SELECT ID FROM (SELECT MAX(a.ID) AS ID FROM TABLEA a WHERE a.ID < OLD.ID) INTO :PREV_ID;
  SELECT ID FROM (SELECT MAX(a.ID) AS ID FROM TABLEA a WHERE a.ID < NEW.ID) INTO :PREV_ID;
  NEW.PARENTID = PREV_ID;
END
^^
SET TERM ; ^^


SET TERM ^^ ;
CREATE TRIGGER TABLEA_BD FOR TABLEA ACTIVE BEFORE DELETE POSITION 0 AS
DECLARE PREV_ID INTEGER;
BEGIN
  PREV_ID = NULL;
  SELECT ID FROM (SELECT MAX(a.ID) AS ID FROM TABLEA a WHERE a.ID < OLD.ID) INTO :PREV_ID;
END
^^
SET TERM ; ^^

COMMIT;"""

db = db_factory(init=init_script)

test_script = """INSERT INTO TABLEA (ID, DESCRIPTION) VALUES (1, 'Blue');
INSERT INTO TABLEA (ID, DESCRIPTION) VALUES (2, 'Red');
INSERT INTO TABLEA (ID, DESCRIPTION) VALUES (4, 'Green');
INSERT INTO TABLEA (ID, DESCRIPTION) VALUES (8, 'Yellow');
COMMIT;
SELECT * FROM TABLEA;"""

act = isql_act('db', test_script)

expected_stdout = """          ID DESCRIPTION                                            PARENTID
============ ================================================== ============
           1 Blue                                                     <null>
           2 Red                                                           1
           4 Green                                                         2
8 Yellow                                                        4"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
