#coding:utf-8

"""
ID:          issue-1281
ISSUE:       1281
TITLE:       DDL - object in use
DESCRIPTION:
JIRA:        CORE-888
FBTEST:      bugs.core_0888
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SET TERM ^ ;
CREATE PROCEDURE TestProc
AS
BEGIN
   EXIT;
END ^
SET TERM ; ^

EXECUTE PROCEDURE TestProc;

DROP PROCEDURE TestProc;
"""

act = isql_act('db', test_script)


@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
