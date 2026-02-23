#coding:utf-8

"""
ID:          issue-2292
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2292
TITLE:       Isql's extracted script is unusable with interdependent selectable procedures in FB 2.1 [CORE1862]
DESCRIPTION: 
    Test creates SP which has output parameter *and* SUSPEND clause.
    Then we extract metadata and check whether this procedure header contains 'SUSPEND'.
    On FB 2.0.7.13318 extracted metadata contains 'EXIT':
        CREATE PROCEDURE SP_TEST RETURNS (O INTEGER)
        AS 
        BEGIN EXIT; END ^

    Although such code can be compiled, this SP could be called (and returned empty resultset) only in 2.0.7 and before.
    Since 2.1 attempt to call such SP will raise:
        Statement failed, SQLSTATE = 42000
        ...
        -invalid request BLR at offset ...
        -Procedure SP_TEST is not selectable (it does not contain a SUSPEND statement)
"""

import re
import pytest
from firebird.qa import *

init_sql = """
    set term ^ ;
    create or alter procedure sp_test returns(out_value int) as
    begin
        out_value = 1;
        suspend;
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db')

@pytest.mark.version('>=3.0.0')
def test_1(act: Action, capsys):

    meta_sql = act.extract_meta()
    EXPECTED_MSG = 'OK'

    p = re.compile(r'SP_TEST\s+RETURNS \(OUT_VALUE INTEGER\)\s+AS\s+BEGIN\s+SUSPEND;\s+END\s?\^', re.IGNORECASE)
    
    if p.search(meta_sql):
        print(EXPECTED_MSG)
    else:
        print(f'Could not find pattern "{p.pattern}" in extracted metadata.')
        print(meta_sql)

    act.expected_stdout = EXPECTED_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
