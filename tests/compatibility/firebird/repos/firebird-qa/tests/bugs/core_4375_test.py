#coding:utf-8

"""
ID:          issue-4697
ISSUE:       4697
TITLE:       Procedure executes infinitely if contains more than 32767 statements inside any BEGIN/END block
DESCRIPTION:
JIRA:        CORE-4375
FBTEST:      bugs.core_4375
"""

import pytest
from firebird.qa import *

init_script = """
    -- On Alpha2 (WI-T3.0.0.30809):
    -- Statement failed, SQLSTATE = 42000
    -- Dynamic SQL Error
    -- -SQL error code = -104
    -- -Invalid command
    -- -no column name specified for column number 1 in derived table C
    -- On Beta2 (WI-T3.0.0.31807) works OK:
    set term ^;
    execute block returns (SQL blob sub_type text) as
    begin
      select
          'create or alter procedure test_proc returns(id integer) as
    begin '
          || list('
        suspend; -- '||id, '')
    ||'
    end'

      from (select row_number()over() id from rdb$types a, rdb$types b rows 50000) c
      into :SQL;
      execute statement :SQL;
      --suspend;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select count(*) cnt from test_proc;

    set term ^;
    execute block returns(cnt int) as
        declare v_proc_src blob;
    begin
        execute statement 'select count(*) cnt from test_proc' into cnt;
        suspend;

        select rdb$procedure_source from rdb$procedures where rdb$procedure_name = upper('test_proc')
        into v_proc_src;
        select octet_length(:v_proc_src) - octet_length(replace(:v_proc_src, ascii_char(10), ''))
        from rdb$database
        into cnt;
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             50000
    CNT                             50000
    CNT                             50001
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

