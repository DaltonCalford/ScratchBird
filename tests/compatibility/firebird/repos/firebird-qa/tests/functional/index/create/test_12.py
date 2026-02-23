#coding:utf-8

"""
ID:          index.create-12
TITLE:       CREATE UNIQUE INDEX - Null value in table
DESCRIPTION:
FBTEST:      functional.index.create.12
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE t( a INTEGER);
commit;
INSERT INTO t VALUES(null);
INSERT INTO t VALUES(0);
INSERT INTO t VALUES(1);
INSERT INTO t VALUES(2);
INSERT INTO t VALUES(3);
INSERT INTO t VALUES(4);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """CREATE UNIQUE INDEX test ON t(a);"""

act = isql_act('db', test_script)

@pytest.mark.skip("Covered by lot of other tests.")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
