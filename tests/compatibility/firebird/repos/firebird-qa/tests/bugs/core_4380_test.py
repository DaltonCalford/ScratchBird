#coding:utf-8

"""
ID:          issue-4702
ISSUE:       4702
TITLE:       ISQL truncates blob when reading an empty segment
DESCRIPTION:
JIRA:        CORE-4380
FBTEST:      bugs.core_4380
"""

import pytest
import re
from firebird.qa import *

init_script = """
    set term ^;
    create procedure sp_test_master(a_id int) returns(o_txt varchar(20)) as
    begin
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    PARAMETERS:
    NUMBER NAME TYPE
    0 A_ID INPUT
    0 O_TXT OUTPUT
    VARIABLES:
    NUMBER NAME
    BLR TO SOURCE MAPPING:
    BLR OFFSET LINE COLUMN
    VALUES: <OFFSET> <LINE> <COLUMN>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    # NB: i'm not sure that this test properly reflects the trouble described in the ticket.
    # At least on 3.0 Alpha 1, Alpha 2 and Beta 2 (31807) output is identical.
    sql_script = """
    set blob all;
    set list on;
    select rdb$debug_info from rdb$procedures;
    """
    act.isql(switches=[], input=sql_script)
    # RDB$DEBUG_INFO                  1a:1e1
    #        Parameters:
    #            Number Name                             Type
    #        --------------------------------------------------
    #                 0 A_ID                             INPUT
    #                 0 O_TXT                            OUTPUT
    #
    #        Variables:
    #            Number Name
    #        -------------------------------------------
    #                 0 O_TXT
    #
    #        BLR to Source mapping:
    #        BLR offset       Line     Column
    #        --------------------------------
    #                42          2         79
    #                ^           ^          ^
    #                |           |          |
    #                +-----------+----------+---- all of them can vary!
    # Print content of log with filtering lines:we are interesting only for rows
    # which contain words: {'Parameters', 'Number', 'Variables', 'BLR'}.
    # For last line (with three numbers for offset, line and col) we just check
    # matching of row to appropriate pattern.
    # NB: we remove all exsessive spaces from printed lines.
    pattern = re.compile("[\\s]+[0-9]+[\\s]+[0-9]+[\\s]+[0-9]+")
    for line in act.stdout.splitlines():
        line = line.upper()
        if ('PARAMETER' in line or
            'NUMBER' in line or
            'INPUT' in line or
            'OUTPUT' in line or
            'VARIABLE' in line or
            'BLR' in line):
            print(' '.join(line.split()).upper())
        if pattern.match(line):
            print('VALUES: <OFFSET> <LINE> <COLUMN>')
    # Test
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
