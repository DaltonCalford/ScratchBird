#coding:utf-8

"""
ID:          issue-6431
ISSUE:       6431
TITLE:       Original content of column which is involved into ENCRYPT() is displayed as distorted view after this call
DESCRIPTION:
JIRA:        CORE-6186
FBTEST:      bugs.core_6186
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with
    a as (
        select 'Encrypts/decrypts data using symmetric cipher' as s_origin
        from rdb$database
    )
    select
        a.s_origin
        ,encrypt( a.s_origin using aes mode ofb key '0123456789012345' iv 'abcdefghhgfedcba') as s_encrypted
    from a
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    S_ORIGIN                        Encrypts/decrypts data using symmetric cipher
    S_ENCRYPTED                     910805BDA8B05C475E8B5D3D0971D58649EA0D549FEA1633A8811429183E925E1C2C77CE4E3B9DCDFA0C75997E
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
