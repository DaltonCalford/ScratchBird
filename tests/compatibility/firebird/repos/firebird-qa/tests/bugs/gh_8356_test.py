#coding:utf-8

"""
ID:          issue-8356
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8356
TITLE:       Make Trace use HEX representation for parameter values of types [VAR]CHAR CHARACTER SET OCTETS and [VAR]BINARY
DESCRIPTION:
NOTES:
    [21.01.2025] pzotov
    After fix parameters type '[va]rchar(N) character set octets' is shown in the trace as '[var]binary(N)'.

    Confirmed problem on 6.0.0.585: parameter values in the trace are shown in binary (non-readable) form.
    Checked on 6.0.0.590-7e96b33, 5.0.2.1597-4fa00f1 - all fine.
"""
import re

import pytest
from firebird.qa import *

init_sql = """
    set term ^;
    create procedure sp_test (
        a_vchr varchar(16) character set octets
        ,a_chr char(16) character set octets
        ,a_vbin varbinary(16)
        ,a_bin binary(16)
    ) as
        declare n smallint;
    begin
        n = 1;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init = init_sql)

act = python_act('db')

test_sql = """
    execute procedure sp_test( 
         gen_uuid()
        ,gen_uuid()
        ,gen_uuid()
        ,gen_uuid()
    );
"""

trace_events_lst = \
    [ 'time_threshold = 0'
      ,'log_procedure_start = true'
      ,'log_initfini = false'
    ]

@pytest.mark.trace
@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    with act.trace(db_events = trace_events_lst):
        act.reset()
        act.isql(switches = ['-q'], input = test_sql, combine_output = True)

    # Process trace
    # BEFORE FIX: param0 = varchar(16), "<binary data here>"
    # AFTER FIX:  param0 = varbinary(16), "D0EC952EC11A4C209011CF95C1712D2F"

    param_name_pattern = re.compile( r'\s?param\d+\s?=\s?(var)?(binary|char)\(\d+\)', re.IGNORECASE )
    # param_hexvalue_ptn = re.compile('')

    for line in act.trace_log:
        if param_name_pattern.search(line.lower()):
            param_name = line.split("=")[0].strip()
            param_val = line.split('"')[1]
            try:
               _ = int(param_val, 16)
               print(f'Parameter: {param_name}, value is in HEX form.')
            except ValueError as e:
               print(f'Parameter: {param_name}, value: "{param_val}" - not in HEX form.')

    act.expected_stdout = """
        Parameter: param0, value is in HEX form.
        Parameter: param1, value is in HEX form.
        Parameter: param2, value is in HEX form.
        Parameter: param3, value is in HEX form.
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
