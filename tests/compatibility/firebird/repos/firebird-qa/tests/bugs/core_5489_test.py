#coding:utf-8

"""
ID:          issue-5759
ISSUE:       5759
TITLE:       Bad performance for NULLs filtering inside a navigational index scan
DESCRIPTION:
  See prototype and explanations for this test in CORE_5435.fbt
  Confirmed improvement:

  3.0.2.32643, 4.0.0.563:
  **********
    PLAN (TEST ORDER TEST_F01_ID)
    1 records fetched
       1143 ms, 2375 read(s), 602376 fetch(es) ---------------- poor :(
    Table                              Natural     Index
    ****************************************************
    TEST                                          300000

  3.0.2.32708, 4.0.0.572:
  **********
     PLAN (TEST ORDER TEST_F01_ID)
     0 ms, 22 read(s), 63 fetch(es) --------------------------- cool :)
    Table                              Natural     Index
    ****************************************************
    TEST                                              20
JIRA:        CORE-5489
FBTEST:      bugs.core_5489
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

init_sql = """
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
       while (i < n) do
       begin
           insert into test(id, f01, f02) values(:i, nullif(mod(:i, :n/20), 0), iif(mod(:i,3)<2, 0, 1))
           returning :i+1 into i;
       end
   end
   ^
   set term ;^
   commit;

   execute procedure sp_add_init_data(300000);
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
        from test
        where f01               -- ###################################################################
              IS NULL           -- <<< ::: NB ::: we check here 'f01 is NULL', exactly as ticket says.
              and f02=0         -- ###################################################################
        order by f01, id
    ) ;
"""

trace = ['time_threshold = 0',
         'log_statement_finish = true',
         'print_plan = true',
         'print_perf = true',
         'log_initfini = false',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.2')
def test_1(act: Action):

    act.isql(switches=[], input=init_sql, combine_output = True)
    assert act.clean_stdout == ''

    with act.trace(db_events=trace):
        act.reset()
        act.isql(switches=[], input=test_script, combine_output = True)

    # Process trace
    run_with_plan = ''
    num_of_fetches = 99999999
    for line in act.trace_log:
        if line.lower().startswith('plan ('):
            run_with_plan = line.strip().upper()
        elif 'fetch(es)' in line:
            words = line.split()
            for k in range(len(words)):
                if words[k].startswith('fetch'):
                    num_of_fetches = int(words[k-1])
    if act.is_version('<6'):
        expected_plan = 'PLAN (TEST ORDER TEST_F01_ID)'
    else:
        expected_plan = 'PLAN ("PUBLIC"."TEST" ORDER "PUBLIC"."TEST_F01_ID")'

    assert run_with_plan == expected_plan
    assert num_of_fetches < FETCHES_THRESHOLD
