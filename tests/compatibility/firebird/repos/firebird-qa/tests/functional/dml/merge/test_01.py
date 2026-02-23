#coding:utf-8

"""
ID:          dml.merge-01
FBTEST:      functional.dml.merge.01
ISSUE:       1201
JIRA:        CORE-815
TITLE:       MERGE statement
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T1 (ID integer, NAME char(10), PRIMARY KEY(id));
CREATE TABLE T2 ( ID integer, NAME char(10), PRIMARY KEY(id));
COMMIT;
INSERT INTO T1 (ID,NAME) VALUES (1,'1NOMT1');
INSERT INTO T1 (ID,NAME) VALUES (2,'2NOMT1');
INSERT INTO T2 (ID,NAME) VALUES (1,'1NOMT2');
INSERT INTO T2 (ID,NAME) VALUES (2,'2NOMT2');
INSERT INTO T2 (ID,NAME) VALUES (3,'3NOMT2');
COMMIT;

"""

db = db_factory(init=init_script)

test_script = """MERGE
 INTO T1
 USING (SELECT * FROM T2 WHERE id > 1) cd
	ON (T1.id = cd.id)
	WHEN MATCHED THEN
	UPDATE SET
	 name = cd.name
	WHEN NOT MATCHED THEN
	INSERT (id, name)
	 VALUES (cd.id, cd.name);
COMMIT;
SELECT ID, NAME FROM T1;
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID NAME
============ ==========
           1 1NOMT1
           2 2NOMT2
           3 3NOMT2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
