#coding:utf-8

"""
ID:          issue-8815
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8815
TITLE:       Restore the broken record layout optimization by gbak and extend it to the new datatypes
DESCRIPTION:
    Test creates a table with several numeric fields and runs query to RDB$FORMATS for this table.
    This it makes b/r and repeat query to this table.
    Fields in the rdb$formats.rdb$descriptor after b/r must be displayed in reversed order
    (comparing to initial output of this blob).
NOTES:
    [03.12.2025] pzotov
    1. Blob rdb$formats.rdb$descriptor has type greater than 1:
       ('BLOB segment 80, subtype FORMAT Nullable')
       This prevents such column to be used in the view.
       Attempt to use result of CAST(rdb$formats.rdb$descriptor as blob sub_type 1) leads
       to unreadable output (missed line breaks).
       Because of that, it was decided to store query in common Python variable, see 'TEST_QUERY'
    2. We have to take in account not the whole line for each field but substring that starts from
       second token (which points to type ID: 8 = short, 9 = long etc).

    Confirmed absense of expected fields order on 6.0.0.1361-56dfbc2.
    Confirmed improvement on 6.0.0.1361-579ff5c; 5.0.4.1737-a75ec4f.

    Thanks to dimitr for idea how to implement this test.
"""
import re
from io import BytesIO
from firebird.driver import SrvRestoreFlag

import pytest
from firebird.qa import *

init_sql = f"""
    set bail on;
    set blob all;
    recreate table test(a smallint, b int, c bigint, d int128);
    recreate table tchk(rdb_descr blob);
    commit;
"""

db = db_factory(init = init_sql)

act = python_act('db')

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.4')
def test_1(act: Action, capsys):

    TEST_QUERY = """
        set bail on;
        set blob all;
        set count on;
        select rdb$descriptor as rdb_descr
        from rdb$formats rf
        join rdb$relations rr on rf.rdb$relation_id = rr.rdb$relation_id
        where rr.rdb$relation_name = upper('test');
    """

    # Initial output:
    # RDB_DESCR:
    # Fields:
    # id offset type           length sub_type flags
    # --- ------ -------------- ------ -------- ----
    # 0      4  8 SHORT            2        0  0x00
    # 1      8  9 LONG             4        0  0x00
    # 2     16 19 BIGINT           8        0  0x00
    # 3     24 24 INT128          16        0  0x00

    # Expected output (after improvement): fields are in REVERSED order.
    # RDB_DESCR:
    # Fields:
    # id offset type           length sub_type flags
    # --- ------ -------------- ------ -------- ----
    # 0      8 24 INT128          16        0  0x00
    # 1     24 19 BIGINT           8        0  0x00
    # 2     32  9 LONG             4        0  0x00
    # 3     36  8 SHORT            2        0  0x00
    # ==============================================
    # 0      1  2   3             4         5    6
    #           ^
    #           +---- take in account only substring starting from this token

    p_field_format = re.compile('\\d+\\s+(short|long|bigint|int128)\\s+\\d+.*0x.*', re.IGNORECASE)
    
    act.isql(switches = ['-q'], input = TEST_QUERY, combine_output = True)
    init_formats_out = act.clean_stdout.splitlines()
    act.reset()
    init_formats_lst = [' '.join( p.split()[2:] ) for p in init_formats_out if p_field_format.search(p)]

    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream=backup, database=act.db.db_path, flags=SrvRestoreFlag.REPLACE)

    act.isql(switches = ['-q'], input = TEST_QUERY, combine_output = True)
    curr_formats_out = act.clean_stdout.splitlines()
    act.reset()
    curr_formats_lst = [' '.join( p.split()[2:] ) for p in curr_formats_out if p_field_format.search(p)]

    EXPECTED_MSG = 'Expected: order of fields has been reversed after b/r.'
    if init_formats_lst == list(reversed(curr_formats_lst)):
        print(EXPECTED_MSG)
    else:
        print('UNEXPECTED. Order of fields after b/r not equal to reversed:')
        print('Formats before b/r:')
        print('\n'.join(init_formats_out))
        print('#' * 80)
        print('Formats after b/r:')
        print('\n'.join(curr_formats_out))

    act.expected_stdout = EXPECTED_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
