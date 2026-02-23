#coding:utf-8

"""
ID:          issue-3730
ISSUE:       3730
TITLE:       Blob filter to translate internal debug info into text representation
DESCRIPTION:
JIRA:        CORE-3364
FBTEST:      bugs.core_3364
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure sp_test(a_n smallint) returns(n_fact bigint) as
    begin
        n_fact = iif(a_n > 0, a_n, 0);

        while (a_n > 1) do
        begin
          n_fact = n_fact * (a_n - 1);
          a_n = a_n -1;
        end
        suspend;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(page_size=4096, sql_dialect=3, init=init_script)

test_script = """
    set list on;
    set blob all;
    select rdb$debug_info as blob_id from rdb$procedures where upper(rdb$procedure_name) = upper('sp_test');
"""

'''
BLOB_ID                         1a:f0
Parameters:
Number Name                             Type
--------------------------------------------------
0 A_N                              INPUT
0 N_FACT                           OUTPUT
Variables:
Number Name
-------------------------------------------
0 N_FACT
BLR to Source mapping:
BLR offset       Line     Column
--------------------------------
36          2          5
38          3          9
73          5          9
92          6          9
94          7         11
116          8         11
142         10          9
'''

substitutions=[ ('BLOB_ID .*', ''),
                ('[ \t]+', ' '),
                ('-', ''),
                ('\\d+[ ]+\\d+[ ]+\\d+', '')
              ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Parameters:
    Number Name Type
    0 A_N INPUT
    0 N_FACT OUTPUT
    Variables:
    Number Name
    0 N_FACT
    BLR to Source mapping:
    BLR offset Line Column
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

