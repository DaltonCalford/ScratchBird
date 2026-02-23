#coding:utf-8

"""
ID:          issue-1689
ISSUE:       1689
TITLE:       Small bug with default value for domains in PSQL
DESCRIPTION:
JIRA:        CORE-1267
FBTEST:      bugs.core_1267
"""

import pytest
from firebird.qa import *

init_script = """CREATE DOMAIN BIT AS SMALLINT CHECK (VALUE IN (0,1) OR VALUE IS NULL);
commit;
"""

db = db_factory(init=init_script)

test_script = """set term ^;

EXECUTE BLOCK
RETURNS (
  ID BIT)
AS
BEGIN
  SUSPEND;
END ^

set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
     ID
=======
 <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

