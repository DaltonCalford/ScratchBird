#coding:utf-8

"""
ID:          issue-3604
ISSUE:       3604
TITLE:       OVERLAY() fails when used with text BLOBs containing multi-byte chars
DESCRIPTION:
JIRA:        CORE-3231
FBTEST:      bugs.core_3231
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        # Test non multi-bytes
        c.execute("with q(s) as (select cast('abcdefghijklmno' as blob sub_type 1 character set utf8) from rdb$database) select overlay (s placing cast('0123456789' as blob sub_type 1 character set utf8) from 5) from q")
        # Test UTF8
        c.execute("with q(s) as (select cast('abcdefghijklmno' as blob sub_type 1 character set utf8) from rdb$database) select overlay (s placing cast(_iso8859_1 'áé' as blob sub_type 1 character set utf8) from 5) from q")
        # Test ISO8859_1
        c.execute("with q(s) as (select cast('abcdefghijklmno' as blob sub_type 1 character set utf8) from rdb$database) select overlay (s placing cast(_iso8859_1 'áé' as blob sub_type 1 character set iso8859_1) from 5) from q")
