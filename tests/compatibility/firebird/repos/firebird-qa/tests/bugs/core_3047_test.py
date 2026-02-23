#coding:utf-8

"""
ID:          issue-3428
ISSUE:       3428
TITLE:       Wrong logic is used to resolve EXECUTE BLOCK parameters collations
DESCRIPTION:
JIRA:        CORE-3047
FBTEST:      bugs.core_3047
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set term ^;
    -- win_ptbr should be resolved against connection charset (win1252), not database (utf8)
    execute block returns (c varchar(10) collate win_ptbr) as
    begin
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(charset='win1252')
