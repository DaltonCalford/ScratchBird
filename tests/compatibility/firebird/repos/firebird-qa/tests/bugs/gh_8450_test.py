#coding:utf-8

"""
ID:          issue-8450
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8450
TITLE:       Slightly better checking for a valid database file
DESCRIPTION:
    Test checks that one can not to establish connection if file (supposed to be Firebird database)
    contains either incompleted starting part of actual FDB or just random bytes (see 'content_type').
    Set of gdscodes can differ depending on size and content_type but no bugchecks should occur for any checked size.
NOTES:
    [07.12.2025] pzotov
    1. Related issues:
        #2897 "Success message when connecting to tiny trash database file", ex. CORE-2484
        #6747 "Wrong message when connecting to tiny trash database file", ex. CORE-6518 // 18.03.2021
        #6755 "Connect to database that contains broken pages can lead to FB crash", ex. CORE-6528 // 31.03.2021
        #6968 "On Windows, engine may hung when works with corrupted database and read after the end of file" // 14.09.2021
    2. Localized text can present in error message ("end of file encountered"). It is suppressed, see substitutions.
    3. Status-vector differs on Windows vs Linux, expected output has been separated depending on OS.

    Checked on Windows 6.0.0.1364-4ae039f; Linux 6.0.0.1364-9048844
"""
import os
from pathlib import Path
import random
import string

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, connect

db = db_factory(page_size = 16384)

substitutions = [ ('^((?!(gdscode|valid|error|less|size)).)*$', ''), ('(-)?error', 'error'), ('(-)?file (")?\\S+(")?', 'file <db_file>') ]

act = python_act('db', substitutions = substitutions)
tmp_fdb = temp_file('tmp_core_1715.fdb')

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fdb: Path, capsys):

    CHECKED_SIZES = (0,1,7,8,15,16,63,64,127,128,255,256,511,512,1023,1024,2047,2048,4095,4096,8191,8192,16383,16384,32767,32768,65535,65536)

    with act.db.connect() as con:
        db_file = con.info.name

    with open(db_file, 'rb') as f:
        db_raw_content = f.read()

    for chk_size in CHECKED_SIZES:
        for content_type in ('fdb_starting_part', 'random_bytes'):
            with open(tmp_fdb, 'wb') as f:
                if content_type == 'fdb_starting_part':
                    f.write(db_raw_content[:chk_size])
                else:
                    f.write( bytearray( ''.join(random.choices(string.ascii_letters + string.digits, k = chk_size)).encode('ascii') ) )
            try:
                with connect( 'inet://' + str(tmp_fdb), user = act.db.user, password = act.db.password, charset = 'utf8') as con:
                    print(f'*** WEIRD *** Connection has established to invalid database file, {con.info.id=}')
            except DatabaseError as e:
                print(f'{content_type=}, size = {chk_size}:')
                print(e.__str__().lower())
                for x in e.gds_codes:
                    print(f'gdscode={x}')

    expected_out_win = """
        content_type='fdb_starting_part', size = 0:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='random_bytes', size = 0:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='fdb_starting_part', size = 1:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 1:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 7:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 7:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 8:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 8:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 15:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 15:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 16:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 16:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 63:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 63:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 64:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 64:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 127:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 127:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 128:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 128:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 255:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 255:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 256:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 256:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 511:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 511:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 512:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 512:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 1023:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 1023:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 1024:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 1024:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 2047:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 2047:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 2048:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 2048:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 4095:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 4095:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 4096:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 4096:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 8191:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 8191:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 8192:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 8192:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 16383:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 16383:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 16384:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='random_bytes', size = 16384:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 32767:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='random_bytes', size = 32767:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 32768:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='random_bytes', size = 32768:
        file <db_file> is not a valid database
        gdscode=335544323
        content_type='fdb_starting_part', size = 65535:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='random_bytes', size = 65535:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 65536:
        i/o error during "readfile" operation for file <db_file>
        -error while trying to read from file
        gdscode=335544344
        gdscode=335544736

        content_type='random_bytes', size = 65536:
        file <db_file> is not a valid database
        gdscode=335544323
    """


    expected_out_nix = """
        content_type='fdb_starting_part', size = 0:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 0:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 1:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 1:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 7:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 7:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 8:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 8:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 15:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 15:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 16:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 16:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 63:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 63:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 64:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 64:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 127:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 127:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 128:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 128:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 255:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 255:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 256:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 256:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 511:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 511:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 512:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 512:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 1023:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 1023:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 1024:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 1024:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 2047:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 2047:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 2048:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 2048:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 4095:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 4095:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 4096:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 4096:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 8191:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 8191:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 8192:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 8192:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 16383:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='random_bytes', size = 16383:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 16384:
        i/o error during "read" operation for file <db_file>
        file <db_file> is less than expected
        gdscode=335544344
        gdscode=335545273

        content_type='random_bytes', size = 16384:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 32767:
        i/o error during "read" operation for file <db_file>
        file <db_file> is less than expected
        gdscode=335544344
        gdscode=335545273

        content_type='random_bytes', size = 32767:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 32768:
        i/o error during "read" operation for file <db_file>
        file <db_file> is less than expected
        gdscode=335544344
        gdscode=335545273

        content_type='random_bytes', size = 32768:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 65535:
        i/o error during "read" operation for file <db_file>
        file <db_file> is less than expected
        gdscode=335544344
        gdscode=335545273

        content_type='random_bytes', size = 65535:
        file <db_file> is not a valid database
        gdscode=335544323

        content_type='fdb_starting_part', size = 65536:
        i/o error during "read" operation for file <db_file>
        file <db_file> is less than expected
        gdscode=335544344
        gdscode=335545273

        content_type='random_bytes', size = 65536:
        file <db_file> is not a valid database
        gdscode=335544323
    """

    act.expected_stdout = expected_out_win if os.name == 'nt' else expected_out_nix
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
