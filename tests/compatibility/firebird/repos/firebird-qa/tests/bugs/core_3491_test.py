#coding:utf-8

"""
ID:          issue-3850
ISSUE:       3850
TITLE:       Altering of a TYPE OF COLUMN parameter affects the original column
DESCRIPTION:
JIRA:        CORE-3491
FBTEST:      bugs.core_3491
NOTES:
    [27.06.2025] pzotov
    Removed 'SHOW' command. It is enough to check twise results of query to rdb$ tables - they must be same.
    Test script was checked on 2.5.0.26074 - bug has been confirmed.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create view v_info as
    select rf.rdb$field_name fld_name, f.rdb$field_type fld_type, f.rdb$field_length fld_length, f.rdb$field_scale fld_scale
    from rdb$relation_fields rf
    left join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    where rf.rdb$relation_name = upper('test');

    create table test (f01 integer);
    commit;
    set term ^;
    create or alter procedure sp_test returns (o_result type of column test.f01) as
    begin
        suspend;
    end^
    set term ;^
    commit;

    select 'point-1' as msg, v.* from v_info v;

    set term ^;
    create or alter procedure sp_test returns (o_result varchar(10)) as
    begin
        suspend;
    end^
    set term ;^
    commit;

    select 'point-2' as msg, v.* from v_info v;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('Table: .*', '')])

expected_stdout = """
    MSG                       point-1
    FLD_NAME                      F01
    FLD_TYPE                        8
    FLD_LENGTH                      4
    FLD_SCALE                       0

    MSG                       point-2
    FLD_NAME                      F01
    FLD_TYPE                        8
    FLD_LENGTH                      4
    FLD_SCALE                       0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

