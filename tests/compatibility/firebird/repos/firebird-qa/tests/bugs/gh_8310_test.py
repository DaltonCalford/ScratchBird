#coding:utf-8

"""
ID:          issue-8310
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8310
TITLE:       Collect network statistics and make it available for the user applications
DESCRIPTION:
NOTES:
    [18.11.2024] pzotov
    ::: NB ::: Currently the ticket is incompletely checked.
    Test verifies only ability to obtain in ISQL wire counters and statistics as described in the doc.
    More complex test(s) will be implemented after firebird-driver become to recognize appropriate API changes.

    Checked on 6.0.0.532; 5.0.2.1567.
"""
import os
import pytest
from firebird.qa import *

db = db_factory()

test_sql = f"""
    set bail on;
    set list on;
    set wire;
    out {os.devnull};
    select count(*) from rdb$fields;
    show wire_stat;
    out;
"""

act = isql_act('db', test_sql, substitutions=[ ('\\d+', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.2')
def test_1(act: Action):
    
    act.expected_stdout = """
        Wire logical statistics:
        send packets = 6
        recv packets = 5
        send bytes = 184
        recv bytes = 224
        Wire physical statistics:
        send packets = 3
        recv packets = 2
        send bytes = 184
        recv bytes = 224
        roundtrips = 2

        Wire logical statistics:
        send packets = 18
        recv packets = 15
        send bytes = 1480
        recv bytes = 944
        Wire physical statistics:
        send packets = 15
        recv packets = 11
        send bytes = 1480
        recv bytes = 944
        roundtrips = 11
    """
    act.execute(combine_output = True)

    assert act.clean_stdout == act.clean_expected_stdout

