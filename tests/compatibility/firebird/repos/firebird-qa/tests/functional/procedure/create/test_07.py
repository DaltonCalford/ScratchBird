#coding:utf-8

"""
ID:          procedure.create-07
TITLE:       CREATE PROCEDURE - try create SP with same name
DESCRIPTION:
FBTEST:      functional.procedure.create.07
"""

import pytest
from firebird.qa import *

init_script = """SET TERM ^;
CREATE PROCEDURE test RETURNS(id INT)AS
BEGIN
  ID=4;
  SUSPEND;
END ^
SET TERM ;^
commit;"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;
CREATE PROCEDURE test RETURNS(id INT)AS
BEGIN
  ID=5;
  SUSPEND;
END ^
SET TERM ;^"""

act = isql_act('db', test_script)

expected_stderr = """Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE PROCEDURE TEST failed
-Procedure TEST already exists"""

@pytest.mark.skip("Covered by lot of other tests.")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
