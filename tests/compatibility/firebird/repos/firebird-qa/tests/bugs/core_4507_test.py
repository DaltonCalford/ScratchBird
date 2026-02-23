#coding:utf-8

"""
ID:          issue-4826
ISSUE:       4826
TITLE:       Unable delete procedure source on Firebird 3.0 Alpha 2.0
DESCRIPTION:
JIRA:        CORE-4507
FBTEST:      bugs.core_4507
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure sp_test(x int, y int) returns(z bigint) as
    begin
       z = x + y;
       suspend;
    end
    ^set term ;^
    commit;

    set blob all;
    set list on;
    select rdb$procedure_source from rdb$procedures where rdb$procedure_name = upper('sp_test');

    update rdb$procedures set rdb$procedure_source = null where rdb$procedure_name = upper('sp_test');
    commit;
    select iif(rdb$procedure_source is null, 'NO_SOURCE', 'HAS_SOURCE') sp_src from rdb$procedures where rdb$procedure_name = upper('sp_test');
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$PROCEDURE_SOURCE.*', '')])

expected_stdout = """
    begin
       z = x + y;
       suspend;
    end
    SP_SRC                          NO_SOURCE
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

