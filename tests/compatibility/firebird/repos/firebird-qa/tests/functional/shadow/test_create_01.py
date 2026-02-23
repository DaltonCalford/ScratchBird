#coding:utf-8

"""
ID:          shadow.create-01
TITLE:       CREATE SHADOW: check basic usage
DESCRIPTION:
NOTES:
    [30.12.2024] pzotov
    Splitted expected out for FB 6.x because columns rdb$file_sequence, rdb$file_start and rdb$file_length
    have NULLs instead of zeroes, see:
    https://github.com/FirebirdSQL/firebird/commit/f0740d2a3282ed92a87b8e0547139ba8efe61173
    ("Wipe out multi-file database support (#8047)")
    Checked on 6.0.0.565
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create shadow 1 '$(DATABASE_LOCATION)test_defaults.shd';
    commit;
    -- SHOW DATABASE -- Removed from here because this test must verify only ability to create shadow.
    set list on;
    set count on;
    select
        --right(trim(rdb$file_name), char_length('test_defaults.shd')) as file_name
        iif( replace(rdb$file_name,'\\','/') containing replace('$(DATABASE_LOCATION)','','/')
             and
             upper(right( trim(rdb$file_name), char_length('test_defaults.shd') )) = upper('test_defaults.shd')
            ,'OK'
            ,'BAD: ' || rdb$file_name
           ) check_shd_file_name
       ,rdb$file_sequence as file_sequence
       ,rdb$file_start as file_start
       ,rdb$file_length as file_length
       ,rdb$file_flags as file_flags
       ,rdb$shadow_number as shadow_number
    from rdb$files;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout_5x = """
    CHECK_SHD_FILE_NAME             OK
    FILE_SEQUENCE                   0
    FILE_START                      0
    FILE_LENGTH                     0
    FILE_FLAGS                      1
    SHADOW_NUMBER                   1
    Records affected: 1
"""

expected_stdout_6x = """
    CHECK_SHD_FILE_NAME             OK
    FILE_SEQUENCE                   <null>
    FILE_START                      <null>
    FILE_LENGTH                     <null>
    FILE_FLAGS                      1
    SHADOW_NUMBER                   1
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
