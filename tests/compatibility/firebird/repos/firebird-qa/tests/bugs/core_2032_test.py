#coding:utf-8

"""
ID:          issue-2469
ISSUE:       2469
TITLE:       Stored procedure recursively called by calculated field fails after reconnect
DESCRIPTION:
NOTES:
  Confirmed bug on 4.0.0.2353; 3.0.8.33401.
  Checked on 4.0.0.2365, 3.0.8.33415 -- all fine.
JIRA:        CORE-2032
FBTEST:      bugs.core_2032
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure strange_procedure (
        result_to_return float,
        reference_id integer)
    returns (result float) as
    begin
        suspend;
    end
    ^
    set term ;^
    commit;

    create table test (
        id integer not null,
        result_to_return float,
        reference_id integer,
        calc_fld computed by (
            (
                select result
                from
                strange_procedure(
                    test.result_to_return,
                    test.reference_id
                )
            )
        )
    );
    commit;

    insert into test (id, result_to_return, reference_id) values (1, 20, 0);
    insert into test (id, result_to_return, reference_id) values (0, 10, null);
    commit;


    set term ^;
    create or alter procedure strange_procedure (
        result_to_return float,
        reference_id integer)
    returns (
        result float)
    as
    declare variable sum_of_referenced_items float;
    begin
        -- initialize variables
        result = result_to_return;
        sum_of_referenced_items = 0;

        -- if reference is null then it
        -- must be record id = 0
        if (reference_id is null) then
        begin
            select sum(t.calc_fld)
            from test t where t.reference_id = 0
            into :sum_of_referenced_items;

            if (sum_of_referenced_items > :result)
            then
                result = sum_of_referenced_items;
        end

        suspend;
    end
    ^
    set term ;^
    commit;
    set list on;
    select calc_fld as value_before_reconnect from test where ID = 0;
    commit;
    connect '$(DSN)' user 'SYSDBA' password 'masterkey';
    select calc_fld as value_after_reconnect from test where ID = 0;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    VALUE_BEFORE_RECONNECT          20
    VALUE_AFTER_RECONNECT           20
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

