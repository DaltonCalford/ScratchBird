#coding:utf-8

"""
ID:          index.create-08
TITLE:       CREATE INDEX - Table with data
DESCRIPTION:
FBTEST:      functional.index.create.08
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE t( a INTEGER);
commit;
INSERT INTO t VALUES(0);
INSERT INTO t VALUES(0);
INSERT INTO t VALUES(1);
INSERT INTO t VALUES(2);
INSERT INTO t VALUES(3);
INSERT INTO t VALUES(4);
INSERT INTO t VALUES(null);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """CREATE INDEX test ON t(a);
SHOW INDEX test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST INDEX ON T(A)"""

@pytest.mark.skip("Covered by lot of other tests.")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
