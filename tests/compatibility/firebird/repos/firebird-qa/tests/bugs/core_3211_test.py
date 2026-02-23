#coding:utf-8

"""
ID:          issue-3585
ISSUE:       3585
TITLE:       String truncation occurs when selecting from a view containing NOT IN inside
DESCRIPTION:
JIRA:        CORE-3211
FBTEST:      bugs.core_3211
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T ( ID integer, FIELD1 varchar(30) );
COMMIT;
CREATE VIEW VT ( ID )
AS
  select T1.ID from T as T1 where T1.FIELD1 not in ( select T2.FIELD1 from T as T2 where T2.FIELD1 = 'system1' )
;
COMMIT;
INSERT INTO T (ID, FIELD1) VALUES (1, 'system');
INSERT INTO T (ID, FIELD1) VALUES (2, 'system');
INSERT INTO T (ID, FIELD1) VALUES (3, 'system');
INSERT INTO T (ID, FIELD1) VALUES (4, 'system');
INSERT INTO T (ID, FIELD1) VALUES (5, 'system');
COMMIT;"""

db = db_factory(init=init_script)

act = isql_act('db', "select * from VT;")

expected_stdout = """
          ID
============
           1
           2
           3
           4
           5
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

