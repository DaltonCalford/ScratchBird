#coding:utf-8

"""
ID:          issue-6686
ISSUE:       6686
TITLE:       EXECUTE STATEMENT fails on FB 4.x if containing time/timestamp with time zone parameters
DESCRIPTION:
JIRA:        CORE-6453
FBTEST:      bugs.core_6453
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
        declare sttm varchar(100);
        declare result varchar(100);
    begin
        sttm = 'select current_time from rdb$database';
        execute statement sttm into result;

        sttm = 'select current_timestamp from rdb$database';
        execute statement sttm into result;
    end
    ^
    set term ;^

"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
