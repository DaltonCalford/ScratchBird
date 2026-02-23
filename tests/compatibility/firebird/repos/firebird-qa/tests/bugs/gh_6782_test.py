#coding:utf-8

"""
ID:          issue-6782
ISSUE:       6782
TITLE:       Getting "records fetched" for functions/procedures in trace
DESCRIPTION:
    Confirmed bug on 4.0.0.2436, there was no:
      * lines with number of fetched rows;
      * additional info about number of fetches/reads/writes/marks (after elapsed time).

    In other words, trace log:
    * was before fix:
      Procedure <some_name>
            0 ms
    * became after fix:
      Procedure <some_name>
      5 records fetched    <<< --- added
            0 ms, 10 fetch(es)
                  ^^^^^^^^^^^^ --- added

    Test parses trace log and search there lines with names of known procedures/functions and
    then checks presence of lines with number of fetched records (for selectable procedures) and
    additional statistics ('fetches/reads/writes/marks').
FBTEST:      bugs.gh_6782
NOTES:
    [30.06.2022] pzotov
        Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.509.
    [04.07.2025] pzotov
        Changed list of patterns to find name of procedures / functions (standalone / packaged):
        we have to take in account name of SQL schema and presense of double quotes in 6.x.
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import locale
import re
import pytest
from firebird.qa import *

init_script = '''
    create table test(id int);
    insert into test(id) select row_number()over() from rdb$types rows 5;
    commit;

    set term ^;
    create procedure standalone_selectable_sp returns(id int) as
    begin
        for select id from test as cursor c
        do begin
            update test set id = -id * (select count(*) from rdb$database)
            where current of c;
            suspend;
        end
    end
    ^

    create procedure standalone_nonselected_sp as
    begin
         for select id from test as cursor c
         do begin
             update test set id = -id * (select count(*) from rdb$database)
             where current of c;
         end
    end
    ^

    create function standalone_func returns int as
    begin
        update test set id = rand()*10000000;
  	    return (select max(id) from test);
    end
    ^

    create package pg_test as
    begin
  	    procedure packaged_selectable_sp returns(id int);
  	    function packaged_func returns int;
  	    procedure packaged_nonselected_sp;
    end
    ^

    create package body pg_test as
    begin
      	procedure packaged_selectable_sp returns(id int) as
      	begin
      		for select id from test as cursor c
      		do begin
      			update test set id = -id * (select count(*) from rdb$database)
      			where current of c;
      			suspend;
      		end
      	end

      	procedure packaged_nonselected_sp as
      	begin
      		for select id from test as cursor c
      		do begin
      			update test set id = -id * (select count(*) from rdb$database)
      			where current of c;
      		end
      	end

      	function packaged_func returns int as
      	begin
      		update test set id = rand()*10000000;
      		return (select min(id) from test);
      	end
    end
    ^


    create procedure sp_main as
        declare c int;
    begin
        for select id from standalone_selectable_sp into c do
        begin
          -- nop --
        end
        ----------------------
        c = standalone_func();
        ----------------------
        execute procedure standalone_nonselected_sp;
        ----------------------

        for select id from pg_test.packaged_selectable_sp into c do
        begin
          -- nop --
        end
        ----------------------
        c = pg_test.packaged_func();
        ----------------------

        execute procedure pg_test.packaged_nonselected_sp;
    end
    ^
    set term ;^
    commit;

    --set list on;
    --execute procedure sp_main;
    --commit;
'''

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout_5x = """
    Procedure STANDALONE_SELECTABLE_SP:
    FOUND line with number of fetched records
    FOUND line with execution statistics

    Function STANDALONE_FUNC:
    FOUND line with execution statistics

    Procedure STANDALONE_NONSELECTED_SP:
    FOUND line with execution statistics

    Procedure PG_TEST.PACKAGED_SELECTABLE_SP:
    FOUND line with number of fetched records
    FOUND line with execution statistics

    Function PG_TEST.PACKAGED_FUNC:
    FOUND line with execution statistics

    Procedure PG_TEST.PACKAGED_NONSELECTED_SP:
    FOUND line with execution statistics

    Procedure SP_MAIN:
    FOUND line with execution statistics
"""

expected_stdout_6x = """
    Procedure "PUBLIC"."STANDALONE_SELECTABLE_SP":
    FOUND line with number of fetched records
    FOUND line with execution statistics

    Function "PUBLIC"."STANDALONE_FUNC":
    FOUND line with execution statistics

    Procedure "PUBLIC"."STANDALONE_NONSELECTED_SP":
    FOUND line with execution statistics

    Procedure "PUBLIC"."PG_TEST"."PACKAGED_SELECTABLE_SP":
    FOUND line with number of fetched records
    FOUND line with execution statistics

    Function "PUBLIC"."PG_TEST"."PACKAGED_FUNC":
    FOUND line with execution statistics

    Procedure "PUBLIC"."PG_TEST"."PACKAGED_NONSELECTED_SP":
    FOUND line with execution statistics

    Procedure "PUBLIC"."SP_MAIN":
    FOUND line with execution statistics
"""

@pytest.mark.trace
@pytest.mark.version('>=3.0.8')
def test_1(act: Action, capsys):

    trace_cfg_items = [
        'time_threshold = 0',
        'log_errors = true',
        'log_procedure_finish = true',
        'log_function_finish = true',
    ]


    sql_run='''
      set list on;
      execute procedure sp_main;
    '''

    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):
        act.isql(input = sql_run, combine_output = True)

    allowed_patterns = \
    (
         r'Procedure\s+("PUBLIC".)?(")?(STANDALONE_SELECTABLE_SP|STANDALONE_NONSELECTED_SP|SP_MAIN)(")?:'
        ,r'Function\s+("PUBLIC".)?(")?STANDALONE_FUNC(")?:'
        ,r'Procedure\s+("PUBLIC".)?(")?PG_TEST(")?.(")?(PACKAGED_SELECTABLE_SP|PACKAGED_NONSELECTED_SP)(")?:'
        ,r'Function\s+("PUBLIC".)?(")?PG_TEST(")?.(")?(PACKAGED_FUNC)(")?:'
        ,r'\d+\s+record(s|\(s\))?\s+fetched'
        ,r'\s+\d+\s+ms(,)?'
    )
    # 0 ms, 1 read(s), 72 fetch(es), 12 mark(s)< False
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    for line in act.trace_log:
        if line.strip():
            # print('>'+line.strip()+'<', act.match_any(line, allowed_patterns))
            if act.match_any(line, allowed_patterns):
                if ' ms' in line and 'fetch' in line:
                    print('FOUND line with execution statistics')
                elif 'record' in line and 'fetch' in line:
                    print('FOUND line with number of fetched records')
                else:
                    print(line)
            
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
