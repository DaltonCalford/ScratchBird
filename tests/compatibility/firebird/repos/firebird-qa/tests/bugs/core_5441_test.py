#coding:utf-8

"""
ID:          issue-5712
ISSUE:       5712
TITLE:       Cache physical numbers of often used data pages to reduce number of fetches of pointer pages
DESCRIPTION:
  We create table with single field, add several rows and create index.
  Number of these rows must be enough to fit all of them in the single data page.
  Than we do loop and query on every iteration one row, using PLAN INDEX.
  Only _first_ iteration should lead to reading PP (and this requires 1 fetch),
  but all subseq. must require to read only DP. This should refect in trace as:
    * 4 fetches for 1st statement;
    * 3 fetches for statements starting with 2nd.
  Distinct number of fetches are accumulated in Python dict, and is displayed finally.
  We should have TWO distinct elements in this dict, and numbers in their values must
  differ at (N-1), where N = number of rows in the table.
JIRA:        CORE-5441
FBTEST:      bugs.core_5441
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(x int primary key);
    commit;
    insert into test(x) select r from (
        select row_number()over() r
        from rdb$types a,rdb$types b
        rows 10
    )
    order by rand();
    commit;

    set term ^;
    create procedure sp_test as
        declare n int;
        declare c int;
    begin
        n = 10;
        while( n > 0 ) do
        begin
            execute statement ( 'select 1 from test where x = ? rows 1' ) ( :n ) into c;
            n = n - 1;
        end
    end^
    set term ;^
    commit;
"""

db = db_factory(page_size=8192, init=init_script)

act = python_act('db')

expected_stdout = """
    fetches=3 occured 9 times
    fetches=4 occured 1 times
"""

trace = ['time_threshold = 0',
         'log_initfini = false',
         'log_statement_finish = true',
         'include_filter = "%(select % from test where x = ?)%"',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.2')
def test_1(act: Action, capsys):
    with act.trace(db_events=trace), act.db.connect() as con:
        c = con.cursor()
        c.call_procedure('sp_test')
    # Process trace
    fetches_distinct_amounts = {}
    for line in act.trace_log:
        if 'fetch(es)' in line:
            words = line.split()
            for k in range(len(words)):
                if words[k].startswith('fetch'):
                    amount = words[k - 1]
                    if not amount in fetches_distinct_amounts:
                        fetches_distinct_amounts[amount] = 1
                    else:
                        fetches_distinct_amounts[amount] += 1
    for k, v in sorted(fetches_distinct_amounts.items()):
        print(f'fetches={k} occured {v} times')
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
