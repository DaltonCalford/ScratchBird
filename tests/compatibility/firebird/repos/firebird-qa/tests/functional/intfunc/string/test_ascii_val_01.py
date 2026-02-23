#coding:utf-8

"""
ID:          intfunc.string.ascii_val
TITLE:       ASCII_VAL( <string> )
DESCRIPTION:
  Returns the ASCII code of the first character of the specified string.
  1. Returns 0 if the string is empty
  2. Throws an error if the first character is multi-byte
FBTEST:      functional.intfunc.string.ascii_val_01
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        with con.cursor() as c:
            result = c.execute("select ascii_val('A') from rdb$database").fetchone()
            assert result == (65,)
            result = c.execute("select ascii_val('Ã') from rdb$database").fetchone()
            assert result == (195,)
            result = c.execute("select ascii_val(cast('A' as BLOB)) from rdb$database").fetchone()
            assert result == (65,)
            result = c.execute("select ascii_val(NULL) from rdb$database").fetchone()
            assert result == (None,)
            result = c.execute("select ascii_val('') from rdb$database").fetchone()
            assert result == (0,)
