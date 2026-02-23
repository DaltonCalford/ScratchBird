#coding:utf-8

"""
ID:          view.create-09
TITLE:       CREATE VIEW as SELECT ....
DESCRIPTION:
  Create view without field list
FBTEST:      functional.view.create.10
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE T1 (ID INTEGER, NAME VARCHAR(10));
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """CREATE VIEW V1 AS SELECT ID AS VID, NAME FROM T1;
COMMIT;
SHOW VIEW V1;
"""

act = isql_act('db', test_script)

expected_stdout = """VID                             INTEGER Nullable
NAME                            VARCHAR(10) Nullable
View Source:
==== ======
SELECT ID AS VID, NAME FROM T1
"""

@pytest.mark.skip("Test not needed: there are lot tests which use this functionality.")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
