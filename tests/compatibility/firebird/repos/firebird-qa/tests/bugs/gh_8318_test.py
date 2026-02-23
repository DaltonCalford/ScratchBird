#coding:utf-8

"""
ID:          issue-8318
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8318
TITLE:       Send blob contents in the same data stream as main resultset
DESCRIPTION: We create four tables: one pair with data which can be compressed with maximal degree ('tblob_max*', 'ttext_max*')
             and second whith data that is almost incompressible  ('tblob_min*', 'ttext_min*').
             Each row in these tables is fulfilled by long binary data (see 'TEXT_LEN' setting), thus DB must have 1-byte charset.
             Then we run queries using ISQL feature that allows to see network statistics (SET WIRE ON), with redirecting output
             to appropriate OS null device.
             Finally, we parse network statistics and gather only roundtrip values from it.
             RATIO between rountrips that were performed during selects BLOB vs VARCHAR is compared to MAX_ALLOWED_ROUND_TRIPS_RATIO.
             This ratio BEFORE improvement was about 38 for maximal compressability and 17 for incompressible data.
             AFTER improvement ratio reduced to ~1. or can be even less than this.
NOTES:
    [28.02.2025] pzotov
    Thanks to Vlad for suggestion about this test implementation.
    Confirmed poor network statistics (RATIO between rountrips) for 6.0.0.607-1985b88 (03.02.2025).
    Checked on 6.0.0.656-25fb454 - all fine.

    [09.04.2025] pzotov
    Checked on 5.0.3.1639-f47fcd9 (intermediate snapshot). Reduced min_version to 5.0.3.
"""

import os
import re
import time

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

TEXT_LEN = 32765
ROWS_CNT = 50
MAX_ALLOWED_ROUND_TRIPS_RATIO = 2

init_sql = f"""
    recreate table tblob_max_compressible(b blob sub_type 0);
    recreate table ttext_max_compressible(c varchar({TEXT_LEN}));
    recreate table tblob_min_compressible(b blob sub_type 0);
    recreate table ttext_min_compressible(c varchar({TEXT_LEN}));
     
    set term ^;
    execute block as
        declare n int = {ROWS_CNT};
        declare s varchar({TEXT_LEN}) character set octets;
        declare x blob character set octets;
    begin
        x = lpad('', {TEXT_LEN}, 'A' );
        while (n>0) do
        begin
            insert into tblob_max_compressible(b) values( :x );
            insert into ttext_max_compressible(c) values( :x );
            n = n - 1;
        end
    end
    ^
    execute block as
        declare n int = {ROWS_CNT};
        declare s varchar({TEXT_LEN}) character set octets;
        declare x blob character set octets;
    begin
        while (n>0) do
        begin
            x = '';
            while (octet_length(x) < {TEXT_LEN}-16) do
            begin
                x = blob_append(x, gen_uuid() );
            end
            insert into tblob_min_compressible(b) values( :x );
            insert into ttext_min_compressible(c) values( :x );
            n = n - 1;
        end
    end
    ^
    set term ;^
    commit;
    alter database set linger to 0;
    commit;
"""

db = db_factory(init = init_sql, charset = 'win1251')
act = python_act('db') # , substitutions = [(r'record length: \d+, key length: \d+', 'record length: NN, key length: MM')])

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.3')
def test_1(act: Action, capsys):
    test_sql = f"""
        rollback;
        set list on;
        set blob all;
        -- set echo on;

        connect '{act.db.dsn}';
        set wire on;
        out {os.devnull}; -- tmp_ttext_max_compressible.tmp;
        select * from ttext_max_compressible;
        out;
        set wire off;
        rollback;

        connect '{act.db.dsn}';
        set wire on;
        out {os.devnull}; -- tmp_blob_max_compressible.tmp;
        select * from tblob_max_compressible;
        out;
        set wire off;
        rollback;

        connect '{act.db.dsn}';
        set wire on;
        out {os.devnull}; -- tmp_ttext_min_compressible.tmp;
        select * from ttext_min_compressible;
        out;
        set wire off;
        rollback;

        connect '{act.db.dsn}';
        set wire on;
        out {os.devnull}; -- tmp_tblob_min_compressible.tmp;
        select * from tblob_min_compressible;
        out;
        set wire off;
    """

    act.isql(switches = ['-q'], input = test_sql, combine_output = True)

    rt_pattern = re.compile('roundtrips', re.IGNORECASE);

    keys = iter(('ttext_max_compressible', 'tblob_max_compressible',  'ttext_min_compressible', 'tblob_min_compressible', ))
    rountrips_map = {}

    if act.return_code == 0:
        # Print only interesting lines from ISQl output tail:
        for line in act.clean_stdout.splitlines():
            if rt_pattern.search(line):
                # print(line)
                rountrips_map[ next(keys) ] = int(line.split('=')[1])
        
        rtrips_max_compr_ratio  = rountrips_map[ 'tblob_max_compressible' ] / rountrips_map[ 'ttext_max_compressible' ]
        rtrips_min_compr_ratio  = rountrips_map[ 'tblob_min_compressible' ] / rountrips_map[ 'ttext_min_compressible' ]

        msg_prefix = 'Ratio between roundtrips when data compressibility is'
        poor_ratio_found = 0
        if rtrips_max_compr_ratio <= MAX_ALLOWED_ROUND_TRIPS_RATIO:
            print(f'{msg_prefix} maximal: EXPECTED.')
        else:
            print(f"{msg_prefix} maximal: UNEXPECTED, {rountrips_map[ 'tblob_max_compressible' ]} / {rountrips_map[ 'ttext_max_compressible' ]} = {rtrips_max_compr_ratio} - greater than {MAX_ALLOWED_ROUND_TRIPS_RATIO=}")
            poor_ratio_found = 1

        if rtrips_min_compr_ratio <= MAX_ALLOWED_ROUND_TRIPS_RATIO:
            print(f'{msg_prefix} minimal: EXPECTED.')
        else:
            print(f"{msg_prefix} maximal: UNEXPECTED, {rountrips_map[ 'tblob_min_compressible' ]} / {rountrips_map[ 'ttext_min_compressible' ]} = {rtrips_min_compr_ratio} - greater than {MAX_ALLOWED_ROUND_TRIPS_RATIO=}")
            poor_ratio_found = 1

        if poor_ratio_found == 1:
            print('Check full output:')
            for line in act.clean_stdout.splitlines():
                print(line)

    else:
        # If retcode !=0 then we can print the whole output of failed gbak:
        for line in act.clean_stdout.splitlines():
            print(line)
    act.reset()

    expected_stdout = f"""
        {msg_prefix} maximal: EXPECTED.
        {msg_prefix} minimal: EXPECTED.
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
