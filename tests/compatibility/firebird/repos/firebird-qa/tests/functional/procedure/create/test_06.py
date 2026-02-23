#coding:utf-8

"""
ID:          procedure.create-06
TITLE:       CREATE PROCEDURE - PSQL Stataments - SUSPEND
DESCRIPTION:
FBTEST:      functional.procedure.create.06
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SET TERM ^;
CREATE PROCEDURE test RETURNS(id INT)AS
BEGIN
  ID=4;
  SUSPEND;
  ID=5;
  SUSPEND;
END ^
SET TERM ;^
commit;
SHOW PROCEDURE test;"""

act = isql_act('db', test_script)

expected_stdout = """Procedure text:
=============================================================================

BEGIN
  ID=4;
  SUSPEND;
  ID=5;
  SUSPEND;
END
=============================================================================
Parameters:
ID                                OUTPUT INTEGER"""

@pytest.mark.skip("Covered by lot of other tests.")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
