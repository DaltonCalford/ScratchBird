#coding:utf-8

"""
ID:          create-database-00
TITLE:       Verify ability to create database with different values of PAGE_SIZE
DESCRIPTION: Test creates database with specifying different values for page_size, each is degree of 2, from 0 to 256 Kb.
NOTES:
    This test replaces old ones: test_03, test_04, test_05, test_06, test_07 and test_12.
    Custome database config object is used here.
    Checked on 6.0.0.172, 5.0.0.1294, 4.0.5.3040, 3.0.12.33725

    [08.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.930; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""
import pytest
from pathlib import Path
import time

from firebird.qa import *
from firebird.driver import DatabaseError, driver_config, create_database

db = db_factory()
act = python_act('db')

tmp_fdb = temp_file('chk_diff_page_size.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_fdb: Path, capsys):
    PAGE_SIZE_SET = [1024*i for i in (0,1,2,4,8,16,32,64,128,256)]

    for pg_val in PAGE_SIZE_SET:
        db_cfg_name = f'tmp_{pg_val}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.database.value = str(tmp_fdb)
        db_cfg_object.page_size.value = pg_val

        try:
            tmp_fdb.unlink(missing_ok = True)
            with create_database(db_cfg_name) as con:
                cur = con.cursor()
                act.print_data_list(cur.execute(f'select {pg_val} as specified_page_size, mon$page_size as actual_page_size from mon$database'))
        except DatabaseError as e:
            print( e.__str__() )


    fb3x_expected_out = """
        SPECIFIED_PAGE_SIZE             0
        ACTUAL_PAGE_SIZE                8192

        SPECIFIED_PAGE_SIZE             1024
        ACTUAL_PAGE_SIZE                4096

        SPECIFIED_PAGE_SIZE             2048
        ACTUAL_PAGE_SIZE                4096

        SPECIFIED_PAGE_SIZE             4096
        ACTUAL_PAGE_SIZE                4096

        SPECIFIED_PAGE_SIZE             8192
        ACTUAL_PAGE_SIZE                8192

        SPECIFIED_PAGE_SIZE             16384
        ACTUAL_PAGE_SIZE                16384

        SPECIFIED_PAGE_SIZE             32768
        ACTUAL_PAGE_SIZE                16384

        SPECIFIED_PAGE_SIZE             65536
        ACTUAL_PAGE_SIZE                8192

        SPECIFIED_PAGE_SIZE             131072
        ACTUAL_PAGE_SIZE                8192

        SPECIFIED_PAGE_SIZE             262144
        ACTUAL_PAGE_SIZE                8192
    """

    fb4x_expected_out = """
        SPECIFIED_PAGE_SIZE             0
        ACTUAL_PAGE_SIZE                8192

        SPECIFIED_PAGE_SIZE             1024
        ACTUAL_PAGE_SIZE                4096

        SPECIFIED_PAGE_SIZE             2048
        ACTUAL_PAGE_SIZE                4096

        SPECIFIED_PAGE_SIZE             4096
        ACTUAL_PAGE_SIZE                4096

        SPECIFIED_PAGE_SIZE             8192
        ACTUAL_PAGE_SIZE                8192

        SPECIFIED_PAGE_SIZE             16384
        ACTUAL_PAGE_SIZE                16384

        SPECIFIED_PAGE_SIZE             32768
        ACTUAL_PAGE_SIZE                32768

        SPECIFIED_PAGE_SIZE             65536
        ACTUAL_PAGE_SIZE                32768

        SPECIFIED_PAGE_SIZE             131072
        ACTUAL_PAGE_SIZE                32768

        SPECIFIED_PAGE_SIZE             262144
        ACTUAL_PAGE_SIZE                32768
    """

    fb6x_expected_out = """
        SPECIFIED_PAGE_SIZE             0
        ACTUAL_PAGE_SIZE                8192
        SPECIFIED_PAGE_SIZE             1024
        ACTUAL_PAGE_SIZE                8192
        SPECIFIED_PAGE_SIZE             2048
        ACTUAL_PAGE_SIZE                8192
        SPECIFIED_PAGE_SIZE             4096
        ACTUAL_PAGE_SIZE                8192
        SPECIFIED_PAGE_SIZE             8192
        ACTUAL_PAGE_SIZE                8192
        SPECIFIED_PAGE_SIZE             16384
        ACTUAL_PAGE_SIZE                16384
        SPECIFIED_PAGE_SIZE             32768
        ACTUAL_PAGE_SIZE                32768
        SPECIFIED_PAGE_SIZE             65536
        ACTUAL_PAGE_SIZE                32768
        SPECIFIED_PAGE_SIZE             131072
        ACTUAL_PAGE_SIZE                32768
        SPECIFIED_PAGE_SIZE             262144
        ACTUAL_PAGE_SIZE                32768
    """

    act.expected_stdout = fb3x_expected_out if act.is_version('<4') else fb4x_expected_out if act.is_version('<6') else fb6x_expected_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
