#coding:utf-8

"""
ID:          issue-2683
ISSUE:       2683
TITLE:       Internal Firebird consistency check when alter dependent procedures
DESCRIPTION:
JIRA:        CORE-2257
FBTEST:      bugs.core_2257
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """set term ^ ;
CREATE OR ALTER PROCEDURE B
AS
begin

end ^

CREATE OR ALTER PROCEDURE A
AS
begin
  execute procedure B;
end ^

COMMIT WORK ^

execute procedure A ^

COMMIT WORK ^

CREATE OR ALTER PROCEDURE B
AS
begin

end ^

COMMIT WORK ^

execute procedure A ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
