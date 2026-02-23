#coding:utf-8

"""
ID:          issue-4723
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/4723
TITLE:       Optimize the record-level RLE algorithm for a denser compression of shorter-than-declared strings and sets of subsequent NULLs [CORE4401]
DESCRIPTION:
    Test creates table with nullable varchar column an adds lot of rows with NULL value.
    Then we run gstat in order to parse statistics related to data pages and avg fill ratio.
    gstat reports for data pages and avg ratio following values:
    4.0.5.3099:
        Pointer pages: 2, data page slots: 2144
        Data pages: 2144, average fill: 91%
    5.0.1.1399, 6.0.0.351:
        Pointer pages: 1, data page slots: 208
        Data pages: 208, average fill: 46%
    Test assumes that values returned for 5.x will not be change in too wide range for several upcoming years
    in any order - see MIN_* and MAX_* thresholds.
NOTES:
    [20.05.2024] pzotov
    Improvement URL (27-sep-2022 15:16):
    https://github.com/FirebirdSQL/firebird/commit/54f1990b98d3e510a10d06fe9ceb76456804da52
    Improved record compression (denser encoding of repeating bytes and less blocks) (#7302)

    NB: snapshots that were just before and after this commit CAN NOT be verified:
        5.0.0.745: raised BUGCHECK ("decompression overran buffer (179), file: sqz.cpp line: 293")
        5.0.0.756: crashed
    Checked on 5.0.1.1399, 6.0.0.351 for DB with page_size = 8192.
"""
import re

import pytest
import platform
from firebird.qa import *

N_ROWS = 30000
N_WIDT = 32760

MIN_DP_COUNT_THRESHOLD = 190
MAX_DP_COUNT_THRESHOLD = 230
MIN_AVG_FILL_THRESHOLD = 30
MAX_AVG_FILL_THRESHOLD = 60

init_ddl = f"""
    recreate table test (f01 varchar({N_WIDT}));
    commit;

    set term ^;
    execute block as
        declare n int = {N_ROWS};
    begin
        while (n > 0) do
        begin
            insert into test(f01) values(null);
            n = n - 1;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(page_size = 8192, init = init_ddl)
act = python_act('db')

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    act.gstat(switches=['-d','-t', 'TEST', '-user', act.db.user, '-pass', act.db.password])

    # 4.x: Pointer pages: 2, data page slots: 2144
    # 5.x: Pointer pages: 1, data page slots: 208
    p_pointer_pages_data_pages_slots = re.compile( r'Pointer\s+pages(:)?\s+\d+(,)?\s+data\s+page\s+slots(:)?\s+\d+' )

    # Data pages: 208, average fill: 46%
    p_data_pages_average_fill_ratio = re.compile( r'Data\s+pages(:)?\s+\d+(,)?\s+average\s+fill(:)?\s+\d+%' )

    data_pages_cnt = avg_fill_ratio = -1
    gstat_lines = act.stdout.splitlines()
    for line in gstat_lines:
        #print(line)
        if p_pointer_pages_data_pages_slots.search(line):
            data_pages_cnt = int(line.split()[-1])
        if p_data_pages_average_fill_ratio.search(line):
            avg_fill_ratio =  int(line.split()[-1].replace('%',''))


    data_pages_cnt_expected_msg = f'data_pages_cnt: expected, within {MIN_DP_COUNT_THRESHOLD=} ... {MAX_DP_COUNT_THRESHOLD=}'
    avg_fill_ratio_expected_msg = f'avg_fill_ratio: expected, within {MIN_AVG_FILL_THRESHOLD=} ... {MAX_AVG_FILL_THRESHOLD=}'
    if data_pages_cnt > 0 and avg_fill_ratio > 0:
        if data_pages_cnt >= MIN_DP_COUNT_THRESHOLD and data_pages_cnt <= MAX_DP_COUNT_THRESHOLD:
            print(data_pages_cnt_expected_msg)
        else:
            print(f'data_pages_cnt UNEXPECTED: {data_pages_cnt=} -- out of scope: {MIN_DP_COUNT_THRESHOLD=} ... {MAX_DP_COUNT_THRESHOLD=}')

        if avg_fill_ratio >= MIN_AVG_FILL_THRESHOLD and avg_fill_ratio <= MAX_AVG_FILL_THRESHOLD:
            print(avg_fill_ratio_expected_msg)
        else:
            print(f'avg_fill_ratio UNEXPECTED: {avg_fill_ratio=} -- out of scope: {MIN_AVG_FILL_THRESHOLD=} ... {MAX_AVG_FILL_THRESHOLD=}')
    else:
        print(f'ERROR: at least one of: {data_pages_cnt=}, {avg_fill_ratio=} is INVALID.')
        print('Could not properly parse gstat output:')
        for p in gstat_lines:
            print(p)

    act.expected_stdout = f"""
        {data_pages_cnt_expected_msg}
        {avg_fill_ratio_expected_msg}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

