#coding:utf-8

"""
ID:          procedure.alter-01
TITLE:       ALTER PROCEDURE - Simple ALTER
DESCRIPTION:
FBTEST:      functional.procedure.alter.01
"""

import pytest
from firebird.qa import *

init_script = """SET TERM ^;
CREATE PROCEDURE test RETURNS (id INTEGER)AS
BEGIN
  id=1;
END ^
SET TERM ;^
commit;
"""

db = db_factory(init=init_script)

test_script = """SET TERM ^;
ALTER PROCEDURE test RETURNS (id INTEGER)AS
BEGIN
  id=2;
END ^
SET TERM ;^
commit;
EXECUTE PROCEDURE test;"""

act = isql_act('db', test_script)

expected_stdout = """
ID
============
2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
