#coding:utf-8

"""
ID:          intfunc.string.overlay
TITLE:       OVERLAY function
DESCRIPTION:
  Returns string1 replacing the substring FROM start FOR length by string2.
FBTEST:      functional.intfunc.string.overlay_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select  OVERLAY( 'il fait beau dans le sud  de la france' PLACING 'NORD' FROM 22 for 4 ) from rdb$database;")

expected_stdout = """
OVERLAY
==========================================
il fait beau dans le NORD de la france
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
