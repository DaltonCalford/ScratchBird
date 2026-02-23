#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8890
TITLE:       WITH TIME ZONE types store non-zero garbage for padding instead of 0x00.
DESCRIPTION:
    Test creates external table to store there data of types: 'timestamp with time zone', 'time with time zone'.
    Then we open appropriate file as binary, parse its content and check that final two bytes of record that
    stores 'time[stamp] with time zone' value are 0x0 (see 'pad_bytes').
NOTES:
    [16.02.2026] pzotov
    Checked on 6.0.0.1454-f2612e4.
"""
import time
import struct

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

DATE_BIN_LEN = 4

ts_tz_data = temp_file('gh_8890_ts_tz.dat')
tm_tz_data = temp_file('gh_8890_tm_tz.dat')

@pytest.mark.version('>=6.0')
def test_1(act: Action, ts_tz_data: Path, tm_tz_data: Path, capsys):
    
    x_types_map = {
        'timestamp' : (ts_tz_data, '2026-02-08 14:32:13.4567 Pacific/Samoa', DATE_BIN_LEN)
       ,'time'      : (tm_tz_data, '14:32:13.4567 Pacific/Fiji'            ,      0      )
    }
    fails_cnt = 0

    for xtype_name, v in x_types_map.items():
        
        x_ext_file, xdts, xtime_offset = v[:3]
        x_fld_type = xtype_name + ' with time zone'
        
        # ----------------------------------
        x_ext_file.unlink(missing_ok = True)
        # ----------------------------------
        test_dml = f"""
            set bail on;
            recreate table tsz_ext external '{x_ext_file}' (
               tmz {x_fld_type}
              ,eol char(2) character set ascii default '--'
            );
            commit;
            insert into tsz_ext(tmz) values ('{xdts}');
            commit;
        """
        act.isql(switches=['-q'], input = test_dml, charset='utf8', io_enc = 'utf-8', combine_output = True)
        assert act.clean_stdout == ''
        act.reset()

        #---+++***---+++***---+++***---+++***---+++***---+++***---+++***---+++***---+++***---+++***---+++***

        with open(x_ext_file, 'rb') as f:
            ext_raw_data = f.read()

        rec_bytes = xtime_offset + 8 + 2 
        for row_idx in range(len(ext_raw_data)//rec_bytes):

            # https://github.com/FirebirdSQL/firebird/issues/8890
            # "WITH TIME ZONE types store non-zero garbage for padding instead of 0x00"
            #
            pad_bytes = struct.unpack_from('@H', ext_raw_data[row_idx * rec_bytes + xtime_offset + 6 : row_idx * rec_bytes + xtime_offset + 8])[0]
            if pad_bytes != 0:
                print(f"Garbage found for {xtype_name=}, actual padding bytes: {hex(pad_bytes)}")
                fails_cnt = fails_cnt + 1

        
    expected_msg = f'ALL FINE for {row_idx+1} rows'
    if fails_cnt == 0:
        print(expected_msg)

    act.expected_stdout = f"""
        {expected_msg}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
