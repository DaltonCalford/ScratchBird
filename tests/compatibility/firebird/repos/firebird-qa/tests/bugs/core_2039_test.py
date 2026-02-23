#coding:utf-8

"""
ID:          issue-2476
ISSUE:       2476
TITLE:       Domain-level CHECK constraints wrongly process NULL values
DESCRIPTION:
JIRA:        CORE-2039
FBTEST:      bugs.core_2039
"""

import pytest
from firebird.qa import *

init_script = """CREATE DOMAIN D_DATE AS DATE
CHECK (VALUE BETWEEN DATE '01.01.1900' AND DATE '01.01.2050');

CREATE PROCEDURE TMP (PDATE D_DATE)
AS BEGIN END;

COMMIT;
"""

db = db_factory(init=init_script)

test_script = """EXECUTE PROCEDURE TMP (NULL);
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
