#coding:utf-8

"""
ID:          issue-3322
ISSUE:       3322
TITLE:       Trace output could contain garbage data left from filtered out statements
DESCRIPTION:
[08.02.2022] pcisar
  Fails on Windows 3.0.8 with unexpected additional output line:
    + 0 records fetched
      1 records fetched
[02.03.2022] pzotov: RESOLVED.
  Problem on Windows was caused by excessive query:
      "select current_user, current_role from rdb$database"
  -- which is done by ISQL 3.x when it gets commands from STDIN via PIPE mechanism.
  Strange ZERO in "0 records" (instead of expected "1 record") can be considered as non-identified bug in trace or ISQL.
  Additional parameter in the trace config (include_filter) and using query with apropriate code (which contain required "tag")
  filters out this excessive query and trace log became identical in Windows and Linux, both for FB 3.x and 4.x

  Discussed with Alex et al, since 28-feb-2022 18:05 +0300.
  Alex explanation: 28-feb-2022 19:52 +0300
  subj: "Firebird new-QA: weird result for trivial test (outcome depends on presence of... running trace session!)"

  Checked on 3.0.8.33535, 4.0.1.2692.

JIRA:        CORE-2940
FBTEST:      bugs.core_2940
"""

import pytest
import platform
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions=[('^((?!((records fetched)|tag_for)).)*$', '')])


expected_stdout = """
    select /* tag_for_include */ 1 as k1 from rdb$types rows 15
    15 records fetched
"""

test_script = """
    select /* tag_for_include */ 1 as k1 from rdb$types rows 15;

    -- statistics for this statement should NOT appear in trace log:
    select 2 k2 from rdb$types rows 2 /* tag_for_exclude */; 

    -- statistics for this statement should NOT appear in trace log:
    select 3 tag_for_exclude from rdb$types rows 3; 

    -- statistics for this statement should NOT appear in trace log:
    set term ^;
    execute block returns(k4 int) as
    begin
       for select 4 from rdb$types rows 4 into k4 do suspend;
    end -- tag_for_exclude
    ^
    set term ;^
    quit;
"""

trace = [
           'log_statement_finish = true'
          ,'include_filter = "%tag_for_include%"'
          ,'exclude_filter = "%tag_for_exclude%"'
          ,'print_perf = true'
          ,'time_threshold = 0'
         ]


@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    with act.trace(db_events=trace):
        act.isql(switches=[], input=test_script)
    act.expected_stdout = expected_stdout
    act.trace_to_stdout()
    assert act.clean_stdout == act.clean_expected_stdout
