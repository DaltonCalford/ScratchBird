#coding:utf-8

"""
ID:          issue-5705
ISSUE:       5705
TITLE:       Minor performance optimization - avoid additional database attachment from security objects mapping code
DESCRIPTION:
  After discuss with Alex (letters 08-10 mar 2017) it was decided to estimate effect of optimization
  by evaluating difference of attachment_id between two subsequent connections to DB.
  NB: Alex said that there was no way to see special service attachment because is was made with turned off
  ability to trace it (see letter 09-mar-2017 16:16).

  SuperServer will have diff=3 (THREE) attachment_id because of CacheWriter and GarbageCollector.
  For that reason we detect FB architecture here and SKIP checking SS results by substitution of
  dummy "OK" instead.
JIRA:        CORE-5433
FBTEST:      bugs.core_5433
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    if act.get_server_architecture() in ['Classic', 'SuperClassic']:
        with act.db.connect() as con:
            att1 = con.info.id
        with act.db.connect() as con:
            att2 = con.info.id
        assert att2 - att1 == 1
    else:
        pytest.skip('Applies only to Classic and SuperClassic')

