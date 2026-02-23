#coding:utf-8

"""
ID:          issue-4667
ISSUE:       4667
TITLE:       Ability to trace stored functions execution
DESCRIPTION:
  Test checks two cases: 1) when execution of function is ENABLED and 2) DISABLED.
  In 1st case we search in trace log rows which prove that function execution was actually logged,
  and in 2nd case we have to ensure that trace log does NOT contain text about this event.
  Both standalone and packaged functions are checked.
JIRA:        CORE-4345
FBTEST:      bugs.core_4345
"""

import pytest
import re
from firebird.qa import *

substitutions = [('^((?!PARAM0|EXECUTE_FUNCTION_START|EXECUTE_FUNCTION_FINISH|SA_FUNC|PG_FUNC).)*$', ''),
                 ('LOG_FUNC_ENABLED.*EXECUTE_FUNCTION_START', 'LOG_FUNC_ENABLED EXECUTE_FUNCTION_START'),
                 ('LOG_FUNC_ENABLED.*EXECUTE_FUNCTION_FINISH', 'LOG_FUNC_ENABLED EXECUTE_FUNCTION_FINISH')]

init_script = """
    set term ^;
    create or alter function sa_func(a int) returns bigint as
    begin
      return a * a;
    end
    ^
    recreate package pg_test as
    begin
        function pg_func(a int) returns bigint;
    end
    ^
    create package body pg_test as
    begin
        function pg_func(a int) returns bigint as
        begin
            return a * a;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=substitutions)

expected_stdout = """
    LOG_FUNC_ENABLED 2016-02-10T15:10:43.5940 (1700:00C52280) EXECUTE_FUNCTION_START
    LOG_FUNC_ENABLED FUNCTION SA_FUNC:
    LOG_FUNC_ENABLED PARAM0 = INTEGER, "123"
    LOG_FUNC_ENABLED 2016-02-10T15:10:43.5940 (1700:00C52280) EXECUTE_FUNCTION_FINISH
    LOG_FUNC_ENABLED FUNCTION SA_FUNC:
    LOG_FUNC_ENABLED PARAM0 = INTEGER, "123"
    LOG_FUNC_ENABLED PARAM0 = BIGINT, "15129"
    LOG_FUNC_ENABLED 2016-02-10T15:10:43.5940 (1700:00C52280) EXECUTE_FUNCTION_START
    LOG_FUNC_ENABLED FUNCTION PG_TEST.PG_FUNC:
    LOG_FUNC_ENABLED PARAM0 = INTEGER, "456"
    LOG_FUNC_ENABLED 2016-02-10T15:10:43.5940 (1700:00C52280) EXECUTE_FUNCTION_FINISH
    LOG_FUNC_ENABLED FUNCTION PG_TEST.PG_FUNC:
    LOG_FUNC_ENABLED PARAM0 = INTEGER, "456"
    LOG_FUNC_ENABLED PARAM0 = BIGINT, "207936"
"""

trace = ['time_threshold = 0',
         'log_errors = true',
         'log_connections = true',
         'log_transactions = true',
         'log_function_start = true',
         'log_function_finish = true'
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    output = []
    trace_timestamp_prefix = '[.*\\s+]*20[0-9]{2}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3,4}\\s+\\(.+\\)'
    func_start_ptn = re.compile(trace_timestamp_prefix + '\\s+(FAILED){0,1}\\s*EXECUTE_FUNCTION_START$', re.IGNORECASE)
    func_finish_ptn = re.compile(trace_timestamp_prefix + '\\s+(FAILED){0,1}\\s*EXECUTE_FUNCTION_FINISH$', re.IGNORECASE)
    func_name_ptn = re.compile('Function\\s+(SA_FUNC|PG_TEST.PG_FUNC):$')
    func_param_prn = re.compile('param[0-9]+\\s+=\\s+', re.IGNORECASE)
    #
    func_script = """
    set list on;
    set term ^;
    execute block as -- returns( sa_func_result bigint, pg_func_result bigint ) as
        declare sa_func_result bigint;
        declare pg_func_result bigint;
    begin
        sa_func_result = sa_func(%s);
        pg_func_result = pg_test.pg_func(%s);
        --suspend;
    end
    ^
    set term ;^
    commit;
"""
    # Case 1: Trace functions enabled
    with act.trace(db_events=trace):
        act.isql(switches=['-n', '-q'], input=func_script % (123, 456), combine_output = True)
    #
    for line in act.trace_log:
        if (func_start_ptn.search(line)
            or func_finish_ptn.search(line)
            or func_name_ptn.search(line)
            or func_param_prn.search(line) ):
            output.append('LOG_FUNC_ENABLED ' + line.upper())
    # Case 2: Trace functions disabled
    act.trace_log.clear()
    with act.trace(db_events=trace[:-2]):
        act.isql(switches=['-n', '-q'], input=func_script % (789, 987), combine_output = True)
    #
    for line in act.trace_log:
        if (func_start_ptn.search(line)
            or func_finish_ptn.search(line)
            or func_name_ptn.search(line)
            or func_param_prn.search(line) ):
            output.append('LOG_FUNC_DISABLED ' + line.upper())
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = '\n'.join(output)
    assert act.clean_stderr == act.clean_expected_stderr
