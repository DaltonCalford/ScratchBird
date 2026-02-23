#coding:utf-8

"""
ID:          issue-1325
ISSUE:       1325
TITLE:       ALL predicate works incorrectly for some subqueries
DESCRIPTION:
JIRA:        CORE-925
FBTEST:      bugs.core_0925
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE STAFF (EMPNUM CHAR(3) NOT NULL UNIQUE,
                     EMPNAME CHAR(20),
                     GRADE DECIMAL(4),
                     CITY CHAR(15));
 CREATE TABLE WORKS ( EMPNUM CHAR(3) NOT NULL,
                      PNUM CHAR(3) NOT NULL,
                      HOURS DECIMAL(5),
                      UNIQUE(EMPNUM,PNUM));
 COMMIT;

 INSERT INTO STAFF VALUES ('E1','Alice',12,'Deale');
 INSERT INTO STAFF VALUES ('E2','Betty',10,'Vienna');
 INSERT INTO STAFF VALUES ('E3','Carmen',13,'Vienna');
 INSERT INTO STAFF VALUES ('E4','Don',12,'Deale');
 INSERT INTO STAFF VALUES ('E5','Ed',13,'Akron');

 INSERT INTO WORKS VALUES ('E1','P1',40);
 INSERT INTO WORKS VALUES ('E1','P2',20);
 INSERT INTO WORKS VALUES ('E1','P3',80);
 INSERT INTO WORKS VALUES ('E1','P4',20);
 INSERT INTO WORKS VALUES ('E1','P5',12);
 INSERT INTO WORKS VALUES ('E1','P6',12);
 INSERT INTO WORKS VALUES ('E2','P1',40);
 INSERT INTO WORKS VALUES ('E2','P2',80);
 INSERT INTO WORKS VALUES ('E3','P2',20);
 INSERT INTO WORKS VALUES ('E4','P2',20);
 INSERT INTO WORKS VALUES ('E4','P4',40);
 INSERT INTO WORKS VALUES ('E4','P5',80);
 COMMIT;
"""

db = db_factory(charset='ISO8859_1', init=init_script)

test_script = """SELECT EMPNUM
FROM WORKS
GROUP BY EMPNUM
HAVING EMPNUM = ALL
  ( SELECT WORKS.EMPNUM
    FROM WORKS JOIN STAFF ON WORKS.EMPNUM = STAFF.EMPNUM );


"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
