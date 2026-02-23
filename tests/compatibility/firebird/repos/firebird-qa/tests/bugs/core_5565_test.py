#coding:utf-8

"""
ID:          issue-5832
ISSUE:       5832
TITLE:       No integer division possible in dialect 1
DESCRIPTION:
JIRA:        CORE-5565
FBTEST:      bugs.core_5565
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    set sql dialect 1;
    commit;
    connect '$(DSN)' user 'SYSDBA' password 'masterkey';
    set term ^;
    execute block as
        declare c int;
    begin
        select 1/1 as x from rdb$database into c;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.execute()
