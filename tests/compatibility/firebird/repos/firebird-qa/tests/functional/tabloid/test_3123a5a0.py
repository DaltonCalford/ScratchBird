#coding:utf-8

"""
ID:          n/a
ISSUE:       None
TITLE:       Check ability to reuse internal connection by EXECUTE STATEMENT.
DESCRIPTION: 
NOTES:
"""

import pytest
from firebird.qa import *

test_sql = f"""
    set list on;
    set count on;
    set term ^;
    execute block returns(sttm_state smallint) as
        declare cnt smallint;
        declare stm varchar(256) = 'select /* trace_me1 */ count(*) from rdb$relation_fields where rdb$relation_name = ?';
    begin
        for
            select trim(rdb$relation_name) as rel_name
            from rdb$relations r
            where r.rdb$system_flag = 1 and r.rdb$relation_name starting with 'RDB$'
            order by 1 rows 5
            as cursor c
        do
            execute statement (stm) (c.rel_name) into cnt;
        --------------------------------------------------
        for
            select /* trace_me2 */ s.mon$state
            from mon$statements s
            where mon$sql_text NOT containing 'execute block'
            into sttm_state
        do
            suspend;
    end
    ^
"""

db = db_factory()
act = isql_act('db', test_sql, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3')
def test_1(act: Action):
   
    act.expected_stdout = """
        STTM_STATE 0
        Records affected: 1
    """
    act.execute(combine_output = True)

    assert act.clean_stdout == act.clean_expected_stdout
