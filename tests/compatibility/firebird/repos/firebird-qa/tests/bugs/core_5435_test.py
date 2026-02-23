#coding:utf-8

"""
ID:          issue-5707
ISSUE:       5707
TITLE:       Badly selective index could be used for extra filtering even if selective index is used for sorting
DESCRIPTION:
  Test creates table and fills it with data like it was specified in the source ticket,
  but query has been CHANGED after discuss with dimitr (see letter 19-jan-2017 08:43):
  instead of 'f01 is null' we use 'f01 = 1' because actually problem that was fixed
  is NOT related with NULL handling.
  Usage of NULL will hide effect of improvement in optimizer because there is old bug
  in FB from early years which prevent engine from fast navigate on index (i.e. PLAN ORDER)
  when expression is like 'is NULL'.

  Implementation details: we start trace and run ISQL with query, then stop trace, open its log
  and parse it with seeking lines with 'plan (' and 'fetch(es)'.
  Expected plan:
    PLAN (TEST ORDER TEST_F01_ID) -- confirmed on WI-T4.0.0.503
    0 ms, 3 read(s), 44 fetch(es)
  WRONG (ineffective) plan:
    PLAN (TEST ORDER TEST_F01_ID INDEX (TEST_F02_ONLY)) -- detected on WI-T4.0.0.463
    21 ms, 115 read(s), 157 fetch(es)
Value of fetches is compared with threshold (currently = 80) which was received after several runs.
JIRA:        CORE-5435
FBTEST:      bugs.core_5435
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=8192)

act = python_act('db')

FETCHES_THRESHOLD = 80

async_init_script = """
   recreate table test
   (
       id int not null,
       f01 int,
       f02 int
   );

   set term ^;
   create or alter procedure sp_add_init_data(a_rows_to_add int)
   as
       declare n int;
       declare i int = 0;
   begin
       n = a_rows_to_add;
       while ( i < n ) do
       begin
           insert into test(id, f01, f02) values( :i, nullif(mod(:i, :n/20), 0), iif( mod(:i,3)<2, 0, 1) )
           returning :i+1 into i;
       end
   end
   ^
   set term ;^
   commit;

   execute procedure sp_add_init_data( 300000 );
   commit;

   create index test_f01_id on test(f01, id);
   create index test_f02_only on test(f02);
   commit;
"""

test_script = """
    set list on;
    select count(*) cnt_check
    from (
        select *
        from test                 -- ############################################################################
        where f01 = 1 and f02 = 0 -- <<< ::: NB ::: we check here 'f01 = 1' rather than 'f01 is NULL' <<< !!! <<<
        order by f01, id          -- ############################################################################
    );
"""

trace = ['time_threshold = 0',
         'log_initfini = false',
         'print_plan = true',
         'print_perf = true',
         'log_statement_finish = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    act.isql(switches=[], input=async_init_script)
    #
    with act.trace(db_events=trace):
        act.reset()
        act.isql(switches=[], input=test_script)
    # Process trace
    run_with_plan = ''
    num_of_fetches = -1
    for line in act.trace_log:
        if line.lower().startswith('plan ('):
            run_with_plan = line.strip()
            if 'fetch(es)' in line:
                words = line.split()
                for k in range(len(words)):
                    if words[k].startswith('fetch'):
                        num_of_fetches = int(words[k - 1])
                        break

    if act.is_version('<6'):
        expected_plan = 'PLAN (TEST ORDER TEST_F01_ID)'
    else:
        expected_plan = 'PLAN ("PUBLIC"."TEST" ORDER "PUBLIC"."TEST_F01_ID")'

    assert run_with_plan == expected_plan
    assert num_of_fetches < FETCHES_THRESHOLD
