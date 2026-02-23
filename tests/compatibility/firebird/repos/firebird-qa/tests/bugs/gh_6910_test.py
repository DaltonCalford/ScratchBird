#coding:utf-8

"""
ID:          issue-6910
ISSUE:       6910
TITLE:       Add way to retrieve statement BLR with Statement::getInfo and ISQL's SET EXEC_PATH_DISPLAY BLR
DESCRIPTION:
  Test issues 'set exec_path_display blr' and runs empty EXECUTE BLOCK, then it does reconnect
  and run empty EB again.

  We verify that:
  * BLR-statements are issued for this EB;
    ( https://github.com/FirebirdSQL/firebird/commit/55704efd24b706272211f921d69db602e838ea38 )
  * state of 'set exec_path_display blr' command will not change after reconnect.
    ( https://github.com/FirebirdSQL/firebird/commit/32c3cf573bf36f576b6116983786107df5a2cb33 )
FBTEST:      bugs.gh_6910
NOTES:
    [15.05.2025] pzotov
    Splitted expected_out for versions up to 5.x and 6.x+ (they become differ since 6.0.0.776).
    Checked on 6.0.0.778
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set exec_path_display blr;
    set term ^;

    execute block as
    begin

    end^
    commit^

    connect '$(DSN)' user sysdba password 'masterkey'^

    execute block as
    begin

    end^
"""

act = isql_act('db', test_script) # , substitutions=[('[ \t]+', ' ')])

expected_5x = """
    Execution path (BLR):
    0 blr_version5,
    1 blr_begin,
    2    blr_message, 1, 1,0,
    6       blr_short, 0,
    8    blr_begin,
    9       blr_stall,
    10       blr_label, 0,
    12          blr_begin,
    13             blr_end,
    14       blr_end,
    15    blr_send, 1,
    17       blr_begin,
    18          blr_assignment,
    19             blr_literal, blr_short, 0, 0,0,
    24             blr_parameter, 1, 0,0,
    28          blr_end,
    29    blr_end,
    30 blr_eoc
    Execution path (BLR):
    0 blr_version5,
    1 blr_begin,
    2    blr_message, 1, 1,0,
    6       blr_short, 0,
    8    blr_begin,
    9       blr_stall,
    10       blr_label, 0,
    12          blr_begin,
    13             blr_end,
    14       blr_end,
    15    blr_send, 1,
    17       blr_begin,
    18          blr_assignment,
    19             blr_literal, blr_short, 0, 0,0,
    24             blr_parameter, 1, 0,0,
    28          blr_end,
    29    blr_end,
    30 blr_eoc
"""

expected_6x = """
    Execution path (BLR):
    0 blr_version5,
    1 blr_begin,
    2    blr_begin,
    3       blr_label, 0,
    5          blr_begin,
    6             blr_end,
    7       blr_end,
    8    blr_end,
    9 blr_eoc
    Execution path (BLR):
    0 blr_version5,
    1 blr_begin,
    2    blr_begin,
    3       blr_label, 0,
    5          blr_begin,
    6             blr_end,
    7       blr_end,
    8    blr_end,
    9 blr_eoc
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_5x if act.is_version('<6') else expected_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
