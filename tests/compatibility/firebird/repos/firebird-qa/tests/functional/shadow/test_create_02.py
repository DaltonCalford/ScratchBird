#coding:utf-8

"""
ID:          shadow.create-02
TITLE:       CREATE SHADOW: check usage with presense of 'FILE <...>' clause.
DESCRIPTION:
NOTES:
    [29.12.2024] pzotov
    Added restriction for FB 6.x: this test now must be skipped, see:
    https://github.com/FirebirdSQL/firebird/commit/f0740d2a3282ed92a87b8e0547139ba8efe61173
    ("Wipe out multi-file database support (#8047)")
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create shadow 1 manual conditional '$(DATABASE_LOCATION)TEST.SHD' file '$(DATABASE_LOCATION)TEST.S00' starting at page 1000;
    commit;
    set list on;
    set count on;
    select
        right(trim(rdb$file_name), char_length('test.s??')) as file_name
       ,rdb$file_sequence as file_sequence
       ,rdb$file_start as file_start
       ,rdb$file_length as file_length
       ,rdb$file_flags as file_flags
       ,rdb$shadow_number as shadow_number
    from rdb$files;
"""

act = isql_act('db', test_script)

expected_stdout = """
    FILE_NAME                       TEST.SHD
    FILE_SEQUENCE                   0
    FILE_START                      0
    FILE_LENGTH                     0
    FILE_FLAGS                      5
    SHADOW_NUMBER                   1

    FILE_NAME                       TEST.S00
    FILE_SEQUENCE                   1
    FILE_START                      1000
    FILE_LENGTH                     0
    FILE_FLAGS                      5
    SHADOW_NUMBER                   1

    Records affected: 2
"""

@pytest.mark.version('>=3,<6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
