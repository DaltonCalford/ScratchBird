#coding:utf-8

"""
ID:          issue-6557
ISSUE:       6557
TITLE:       Unable to specify new 32k page size
DESCRIPTION:
NOTES:
    [29.05.2020] pzotov
        Issues remain for some kind of commands: parser should be more rigorous.
        Sent letter to Alex and Dmitry, 29.05.2020 12:28.
    [12.03.2025] pzotov
        Separated expected_out for 6.x after commit #d7a0e694
        ("Replace 'upper' with 'nearest' rounding for the user-specified page size").
        Now, if we specify page_size = 16383 then actual page size must be 16384 (previously it was 8192).
        Checked on 6.0.0.663-d7a0e69 (letter from dimitr, 12.03.2025 19:52).
    [13.03.2025] pzotov
        Changed expected out for 6.x after fixed https://github.com/FirebirdSQL/firebird/issues/8470.
        Question about 'page size NULL' and 'page size DEFAULT' not yet resolved: whether they allowed or no.

    [03.07.2025] pzotov
    Adjusted expected_output for FB 6.x: min page_size is 8192.
    ::: NB ::: Test diration time is about 75 seconds.
    Checked on 6.0.0.892
"""

import pytest
from pathlib import Path
from firebird.qa import *
from firebird.driver import DatabaseError, ShutdownMode, ShutdownMethod

db = db_factory(do_not_create=True)

act = python_act('db', substitutions=[('Token unknown.*line.*', 'Token unknown')])

page_list= ['9223372036854775809',
            '9223372036854775808',
            '9223372036854775807',
            '4294967297',
            '4294967296',
            '4294967295',
            '2147483649',
            '2147483648',
            '2147483647',
            '65537',
            '32769',
            '32768',
            '32767',
            '16385',
            '16384',
            '16383',
            '8193',
            '8192',
            '8191',
            '4097',
            '4096',
            '4095',
            '2049',
            '2048',
            '2047',
            '1025',
            '1024',
            '1023',
            '0',
            '0x10000',
            '0xFFFF',
            '0x8000',
            '0x7FFF',
            '0x4000',
            '0x3FFF',
            '0x2000',
            '0x1FFF',
            '0x1000',
            '0xFFF',
            '0x800',
            '0x7FF',
            '0x400',
            '0x3FF',
            'default',
            'null',
            'qwerty',
            '-32768'
            ]


@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    if act.is_version('<6'):
        expected_stdout = """
            create database ... page_size 9223372036854775809 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -9223372036854775809
            create database ... page_size 9223372036854775809
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775808 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -9223372036854775808
            create database ... page_size 9223372036854775808
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775807 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -9223372036854775807
            create database ... page_size 9223372036854775807
            DB created. Actual page_size: 32768
            create database ... page_size 4294967297 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -4294967297
            create database ... page_size 4294967297
            DB created. Actual page_size: 32768
            create database ... page_size 4294967296 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -4294967296
            create database ... page_size 4294967296
            DB created. Actual page_size: 32768
            create database ... page_size 4294967295 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -4294967295
            create database ... page_size 4294967295
            DB created. Actual page_size: 32768
            create database ... page_size 2147483649 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -2147483649
            create database ... page_size 2147483649
            DB created. Actual page_size: 32768
            create database ... page_size 2147483648 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -2147483648
            create database ... page_size 2147483648
            DB created. Actual page_size: 32768
            create database ... page_size 2147483647 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 2147483647
            DB created. Actual page_size: 32768
            create database ... page_size 65537 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 65537
            DB created. Actual page_size: 32768
            create database ... page_size 32769 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 32769
            DB created. Actual page_size: 32768
            create database ... page_size 32768 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 32768
            DB created. Actual page_size: 32768
            create database ... page_size 32767 default character set win1251
            DB created. Actual page_size: 16384
            create database ... page_size 32767
            DB created. Actual page_size: 16384
            create database ... page_size 16385 default character set win1251
            DB created. Actual page_size: 16384
            create database ... page_size 16385
            DB created. Actual page_size: 16384
            create database ... page_size 16384 default character set win1251
            DB created. Actual page_size: 16384
            create database ... page_size 16384
            DB created. Actual page_size: 16384
            create database ... page_size 16383 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 16383
            DB created. Actual page_size: 8192
            create database ... page_size 8193 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 8193
            DB created. Actual page_size: 8192
            create database ... page_size 8192 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 8192
            DB created. Actual page_size: 8192
            create database ... page_size 8191 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 8191
            DB created. Actual page_size: 4096
            create database ... page_size 4097 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 4097
            DB created. Actual page_size: 4096
            create database ... page_size 4096 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 4096
            DB created. Actual page_size: 4096
            create database ... page_size 4095 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 4095
            DB created. Actual page_size: 4096
            create database ... page_size 2049 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 2049
            DB created. Actual page_size: 4096
            create database ... page_size 2048 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 2048
            DB created. Actual page_size: 4096
            create database ... page_size 2047 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 2047
            DB created. Actual page_size: 4096
            create database ... page_size 1025 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 1025
            DB created. Actual page_size: 4096
            create database ... page_size 1024 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 1024
            DB created. Actual page_size: 4096
            create database ... page_size 1023 default character set win1251
            DB created. Actual page_size: 4096
            create database ... page_size 1023
            DB created. Actual page_size: 4096
            create database ... page_size 0 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0
            DB created. Actual page_size: 8192
            create database ... page_size 0x10000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x10000
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x8000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x8000
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x4000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x4000
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x2000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x2000
            DB created. Actual page_size: 8192
            create database ... page_size 0x1FFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x1FFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x1000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x1000
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x800 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x800
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FF
            DB created. Actual page_size: 8192
            create database ... page_size 0x400 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x400
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FF
            DB created. Actual page_size: 8192
            create database ... page_size default default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -default
            create database ... page_size default
            DB created. Actual page_size: 8192
            create database ... page_size null default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -null
            create database ... page_size null
            DB created. Actual page_size: 8192
            create database ... page_size qwerty default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -qwerty
            create database ... page_size qwerty
            DB created. Actual page_size: 8192
            create database ... page_size -32768 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            --
            create database ... page_size -32768
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            --
        """
    else:
        expected_stdout = """
            create database ... page_size 9223372036854775809 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775809
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775808 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775808
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775807 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 9223372036854775807
            DB created. Actual page_size: 32768
            create database ... page_size 4294967297 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 4294967297
            DB created. Actual page_size: 32768
            create database ... page_size 4294967296 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 4294967296
            DB created. Actual page_size: 32768
            create database ... page_size 4294967295 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 4294967295
            DB created. Actual page_size: 32768
            create database ... page_size 2147483649 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 2147483649
            DB created. Actual page_size: 32768
            create database ... page_size 2147483648 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 2147483648
            DB created. Actual page_size: 32768
            create database ... page_size 2147483647 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 2147483647
            DB created. Actual page_size: 32768
            create database ... page_size 65537 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 65537
            DB created. Actual page_size: 32768
            create database ... page_size 32769 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 32769
            DB created. Actual page_size: 32768
            create database ... page_size 32768 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 32768
            DB created. Actual page_size: 32768
            create database ... page_size 32767 default character set win1251
            DB created. Actual page_size: 32768
            create database ... page_size 32767
            DB created. Actual page_size: 32768
            create database ... page_size 16385 default character set win1251
            DB created. Actual page_size: 16384
            create database ... page_size 16385
            DB created. Actual page_size: 16384
            create database ... page_size 16384 default character set win1251
            DB created. Actual page_size: 16384
            create database ... page_size 16384
            DB created. Actual page_size: 16384
            create database ... page_size 16383 default character set win1251
            DB created. Actual page_size: 16384
            create database ... page_size 16383
            DB created. Actual page_size: 16384
            create database ... page_size 8193 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 8193
            DB created. Actual page_size: 8192
            create database ... page_size 8192 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 8192
            DB created. Actual page_size: 8192
            create database ... page_size 8191 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 8191
            DB created. Actual page_size: 8192
            create database ... page_size 4097 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 4097
            DB created. Actual page_size: 8192
            create database ... page_size 4096 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 4096
            DB created. Actual page_size: 8192
            create database ... page_size 4095 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 4095
            DB created. Actual page_size: 8192
            create database ... page_size 2049 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 2049
            DB created. Actual page_size: 8192
            create database ... page_size 2048 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 2048
            DB created. Actual page_size: 8192
            create database ... page_size 2047 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 2047
            DB created. Actual page_size: 8192
            create database ... page_size 1025 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 1025
            DB created. Actual page_size: 8192
            create database ... page_size 1024 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 1024
            DB created. Actual page_size: 8192
            create database ... page_size 1023 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 1023
            DB created. Actual page_size: 8192
            create database ... page_size 0 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0
            DB created. Actual page_size: 8192
            create database ... page_size 0x10000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x10000
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x8000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x8000
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x4000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x4000
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x2000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x2000
            DB created. Actual page_size: 8192
            create database ... page_size 0x1FFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x1FFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x1000 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x1000
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0xFFF
            DB created. Actual page_size: 8192
            create database ... page_size 0x800 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x800
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x7FF
            DB created. Actual page_size: 8192
            create database ... page_size 0x400 default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x400
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FF default character set win1251
            DB created. Actual page_size: 8192
            create database ... page_size 0x3FF
            DB created. Actual page_size: 8192
            create database ... page_size default default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -default
            create database ... page_size default
            DB created. Actual page_size: 8192
            create database ... page_size null default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -null
            create database ... page_size null
            DB created. Actual page_size: 8192
            create database ... page_size qwerty default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            -qwerty
            create database ... page_size qwerty
            DB created. Actual page_size: 8192
            create database ... page_size -32768 default character set win1251
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            --
            create database ... page_size -32768
            Statement failed, SQLSTATE = 42000
            Dynamic SQL Error
            -SQL error code = -104
            -Token unknown
            --
        """


    with act.connect_server() as srv:
        for page_size in page_list:
            for charset in [' default character set win1251', '']:
                cmd = f"create database '{act.db.dsn}' user {act.db.user} password '{act.db.password}' page_size {page_size}{charset}"
                print(f'create database ... page_size {page_size}{charset}')
                act.reset()
                act.isql(switches=['-q', '-b'], input=f'{cmd}; ALTER DATABASE SET LINGER TO 0;',
                           combine_output=True, connect_db=False, credentials=False)
                print(act.stdout)
                #
                if act.db.db_path.is_file():
                    with act.db.connect() as con:
                        print('DB created. Actual page_size:', con.info.page_size)
                    srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                                          method=ShutdownMethod.FORCED, timeout=0)
                    srv.database.bring_online(database=act.db.db_path)
                    act.db.drop()
    #
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    #
    act.db.create() # to ensure clean teardown
    assert act.clean_stdout == act.clean_expected_stdout
