#coding:utf-8

"""
ID:          issue-1850
ISSUE:       1850
TITLE:       Collation not propagated between record formats
DESCRIPTION:
JIRA:        CORE-1432
FBTEST:      bugs.core_1432
"""

import pytest
from firebird.qa import *

init_script = """CREATE DOMAIN D_1250 VARCHAR(10) CHARACTER SET WIN1250 COLLATE WIN1250;
CREATE DOMAIN D_CZ VARCHAR(10) CHARACTER SET WIN1250 COLLATE WIN_CZ;

CREATE TABLE T (I INTEGER, A D_1250);
INSERT INTO T VALUES(10, 'a');
COMMIT;
"""

db = db_factory(charset='WIN1250', init=init_script)

test_script = """SET COUNT ON;

SELECT * FROM T WHERE A='A';

ALTER TABLE T ALTER A TYPE D_CZ;
commit;

SELECT * FROM T WHERE A='A';
"""

act = isql_act('db', test_script)

expected_stdout = """Records affected: 0

           I A
============ ==========
          10 a

Records affected: 1
"""

@pytest.mark.version('>=2.5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

