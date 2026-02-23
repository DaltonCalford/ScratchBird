#coding:utf-8

"""
ID:          issue-8681
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8681
TITLE:       Regression in 6.x: COALESCE(<non_empty_arg_1>, <non_empty_arg_2>) can return empty string
NOTES:
    [09.08.2025] pzotov
    Bug was found when source code of OLTP-EMUL test was reimplemented to enable run it
    on FB 6.x (space-only columns not allowed on FB 6.x, see #8452).
    Checked on 6.0.0.1164-6b5aa1c.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate exception ex_empty_input_str 'Empty input argument: @1.';
    set term ^;
    create or alter procedure sp_test(
         a_main_view varchar(63) NOT null
        ,a_aux_view varchar(63) default null
    ) returns (
        id_selected int
    ) as
    begin
        a_aux_view = coalesce( a_aux_view, a_main_view );
        if ( trim(a_aux_view) = '' ) then
        begin
            exception ex_empty_input_str using('a_aux_view');
        end

        id_selected = 1;
        suspend;

    end
    ^
    commit
    ^
    execute block returns(id_selected int) as
        declare v_sttm varchar(8190);
    begin
        execute statement ('select id_selected from sp_test(?, ?)') ('v_main_view', 'v_aux_view') into id_selected;
        suspend;
    end
    ^
    set term ;^
"""
substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        ID_SELECTED 1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
