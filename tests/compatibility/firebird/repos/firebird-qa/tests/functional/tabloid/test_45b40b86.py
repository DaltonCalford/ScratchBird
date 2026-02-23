#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/45b40b86b94bec9deadcab5d376e079700cd68aa
TITLE:       Fix old problem triggered by schemas changes (related to tests/bugs/gh_8057_test.py)
DESCRIPTION:
    This test contains smallest part of test for #8057 that caused weird error
    "message length error (encountered 506, expected 253)" on attempting
    to create trivial function after trying to drop non-existing filter.
    Dropping objects in bugs/gh_8057_test.py is not mandatory so this test preserves the issue
    which can disappear if gh_8057_test.py will be simplified (by removing unneeded code).
NOTES:
    [18.07.2025] pzotov
    Discussed with Adriano, 18.07.2025 08:24
    Checked on 6.0.0.1039-45b40b8.
"""

import pytest
from firebird.qa import *

test_sql = f"""
    set bail on;
    set heading off;
    set term ^;
    execute block as
        declare v_sttm varchar(1024);
    begin
        begin
            v_sttm = 'drop filter foo';
            execute statement v_sttm;
            when any do
            begin
            end
        end
        execute statement 'create or alter function bar() returns int as begin return 1; end';
    end
    ^
    set term ;^
    commit;
    select 'Ok' from rdb$database;
"""

db = db_factory()
act = isql_act('db', test_sql)

@pytest.mark.version('>=6')
def test_1(act: Action):
    act.expected_stdout = f"""
        Ok
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
