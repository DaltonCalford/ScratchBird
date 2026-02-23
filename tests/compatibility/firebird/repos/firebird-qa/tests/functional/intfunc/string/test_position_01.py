#coding:utf-8

"""
ID:          intfunc.string.position
ISSUE:       1926
TITLE:       POSITION( <string> IN <string> )
DESCRIPTION:
  POSITION(X IN Y) returns the position of the substring X in the string Y.
  Returns 0 if X is not found within Y.
NOTES:
[03.02.2022] pcisar
  Merged with "functional.intfunc.string.position_02" test to simplify the suite structure.
  Now each string function has only one test file.
FBTEST:      functional.intfunc.string.position_01
JIRA:        CORE-1511
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
select position('beau' IN 'il fait beau dans le nord' ) from rdb$database;
-- next is from functional.intfunc.string.position_02
SELECT POSITION('beau','beau,il fait beau') C1,POSITION('beau','beau,il fait beau',2) C2 FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
POSITION
============
9
          C1           C2
============ ============
           1           14
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
