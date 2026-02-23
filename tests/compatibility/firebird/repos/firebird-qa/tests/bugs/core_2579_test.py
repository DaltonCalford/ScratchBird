#coding:utf-8

"""
ID:          issue-2989
ISSUE:       2989
TITLE:       Parameters and variables cannot be used as expressions in EXECUTE PROCEDURE parameters without a colon prefix
DESCRIPTION:
JIRA:        CORE-2579
FBTEST:      bugs.core_2579
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """set term ^ ;

create procedure P123 (param int)
as
begin
   execute procedure p123 (param);
end ^

set term ; ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
